#include "lr_context.h"
#include "lr_2d.h"
#include "lr_texture.h"
#include "lr_shader.h"
#include "lr_geometry.h"
#include <stdlib.h>
#include <math.h>


#ifdef __GNUC__
#define LR_PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#endif

#ifdef _MSC_VER
#define LR_PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop))
#endif

LR_PACK(struct Vertex2D
{
    float x; float y;
    float u; float v;
    uint32_t circle;
    uint32_t color;
});
typedef struct Vertex2D Vertex2D;

struct LR_2D {
    LR_Shader *shader;
    LR_VertexDeclaration *decl;
    LR_Geometry *geom;
    LR_Texture *dot;
    LR_Texture *currentTexture;
    Vertex2D *vertices;
    int vCount;
    int modelviewproj;
    int xW;
    int xH;
};


static LR_Matrix4x4 Mat4x4_OrthographicOffCenter(
    float left,
    float right,
    float bottom,
    float top,
    float zNearPlane,
    float zFarPlane)
{
    LR_Matrix4x4 matrix;
    //Identity fill
    #define M(x,y) matrix.m[(x - 1) * 4 + (y - 1)]
    M(1,2) = 0;
    M(1,3) = 0;
    M(1,4) = 0;
    M(2,1) = 0;
    M(2,3) = 0;
    M(2,4) = 0;
    M(3,1) = 0;
    M(3,2) = 0;
    M(3,4) = 0;
    M(4,4) = 1;
    //Set others
    M(1,1) = 2.0 / (right - left);
    M(2,2) = 2.0 / (top - bottom);
    M(3,3) = 1.0 / (zNearPlane - zFarPlane);
    M(4,1) = (left + right) / (left - right);
    M(4,2) = (top + bottom) / (bottom - top);
    M(4,3) = zNearPlane / (zNearPlane - zFarPlane);
    #undef M
    return matrix;
}

#include "shader2d_vertex.h"
#include "shader2d_fragment.h"

#define MAX_QUADS (512)
#define MAX_VERT (MAX_QUADS * 4)
#define MAX_INDEX (MAX_QUADS * 6)


LR_2D *LR_2D_Init(LR_Context *ctx)
{
    LR_2D *r2d = (LR_2D*)malloc(sizeof(struct LR_2D));
    r2d->shader = LR_Shader_Create(ctx, shader2d_vertex, shader2d_fragment);
    r2d->vertices = NULL;
    r2d->currentTexture = NULL;
    r2d->vCount = 0;
    r2d->xW = 0;
    r2d->xH = 0;
    LR_VertexElement elems[] = {
        { .slot = LRELEMENTSLOT_POSITION, .type = LRELEMENTTYPE_FLOAT, .elements = 2, .normalized = 0, .offset = 0 },
        { .slot = LRELEMENTSLOT_TEXTURE1, .type = LRELEMENTTYPE_FLOAT, .elements = 2, .normalized = 0, .offset = sizeof(float) * 2 },
        { .slot = LRELEMENTSLOT_TEXTURE2, .type = LRELEMENTTYPE_USHORT, .elements = 2, .normalized = 0, .offset = sizeof(float) * 4 },
        { .slot = LRELEMENTSLOT_COLOR, .type = LRELEMENTTYPE_BYTE, .elements = 4, .normalized = 1, .offset = sizeof(float) * 5 }
    };
    r2d->decl = LR_VertexDeclaration_Create(ctx, sizeof(float) * 6, 4, elems);
    r2d->geom = LR_StreamingGeometry_Create(ctx, r2d->decl, MAX_VERT);
    uint16_t indices[MAX_INDEX];
    int iptr = 0;
    for(int i = 0; i < MAX_VERT; i += 4) {
        /* Triangle 1 */
		indices[iptr++] = (uint16_t)i;
		indices[iptr++] = (uint16_t)(i + 1);
		indices[iptr++] = (uint16_t)(i + 2);
		/* Triangle 2 */
		indices[iptr++] = (uint16_t)(i + 1);
		indices[iptr++] = (uint16_t)(i + 3);
		indices[iptr++] = (uint16_t)(i + 2);
    }
    LR_StreamingGeometry_SetIndices(ctx, r2d->geom, indices, MAX_INDEX);
    r2d->dot = LR_Texture_Create(ctx, 0);
    LR_Texture_Allocate(ctx, r2d->dot, LRTEXTYPE_2D, LRTEXFORMAT_COLOR, 1, 1);
    uint32_t whitePixel = 0xFFFFFFFF;
    LR_Texture_SetRectangle(ctx, r2d->dot, 0, 0, 1, 1, &whitePixel);
    //uniforms
    LR_BindProgram(ctx, r2d->shader->programID);
    r2d->modelviewproj = glGetUniformLocation(r2d->shader->programID, "modelviewproj");
    int tex = glGetUniformLocation(r2d->shader->programID, "tex");
    glUniform1i(tex, 1);
    int blend = glGetUniformLocation(r2d->shader->programID, "blend");
    glUniform1f(blend, 0);
    int circle = glGetUniformLocation(r2d->shader->programID, "circle");
    glUniform1i(circle, 0);
    return r2d;
}

static void CheckState(LR_Context *ctx, LR_Texture* tex, int addCount)
{
    LR_2D *r2d = ctx->ren2d;
    if((r2d->currentTexture && r2d->currentTexture != tex) || //flush on texture change
        (r2d->vCount + addCount) >= MAX_VERT //or overflow
    ) {
        LR_Flush2D(ctx);
    }
    r2d->currentTexture = tex;
    if(!ctx->ren2d->vertices) {
        ctx->ren2d->vertices = LR_StreamingGeometry_Begin(ctx, ctx->ren2d->geom);
    }
}

void LR_Flush2D(LR_Context *ctx)
{
    LR_2D *r2d = ctx->ren2d;
    if(!r2d->vertices) return;
    LR_StreamingGeometry_Finish(ctx, r2d->geom, r2d->vCount);
    r2d->vertices = 0;
    if(!r2d->vCount) return;
    //bind
    LR_BindVAO(ctx, r2d->geom->vao);
    if(!LR_Texture_EnsureLoaded(ctx, r2d->currentTexture))
        LR_CriticalErrorFunc(ctx, "Texture not resident @ LR_Flush2D");
    LR_BindTex(ctx, 1, r2d->currentTexture->target, r2d->currentTexture->textureObj);
    LR_Texture_SetFilter(ctx, r2d->currentTexture, LRTEXFILTER_LINEAR);
    LR_BindProgram(ctx, r2d->shader->programID);
    LR_SetBlendMode(ctx, 1, LRBLEND_SRCALPHA, LRBLEND_INVSRCALPHA);
    LR_SetCull(ctx, LRCULL_NONE);
    LR_SetDepthMode(ctx, DEPTHMODE_NONE);
    //set viewport
    int vpW = ctx->viewports[ctx->viewportSP].width;
    int vpH = ctx->viewports[ctx->viewportSP].height;
    if(vpW != r2d->xW || vpH != r2d->xH) {
        LR_Matrix4x4 viewproj = Mat4x4_OrthographicOffCenter(0, vpW, vpH, 0, 0, 1);
        r2d->xW = vpW;
        r2d->xH = vpH;
        glUniformMatrix4fv(r2d->modelviewproj, 1, GL_FALSE, (GLfloat*)&viewproj);
    }
    //draw
    GL_CHECK(ctx, glDrawElements(GL_TRIANGLES, (r2d->vCount / 4) * 6, GL_UNSIGNED_SHORT, 0));
    r2d->vCount = 0;
    r2d->currentTexture = NULL;
}

#define BUILD_VERTEX(indexer,_x,_y,_u,_v,_color) do { \
    Vertex2D *vert = &indexer; \
    vert->x = (_x); \
    vert->y = (_y); \
    vert->u = (_u); \
    vert->v = (_v); \
    vert->color = (_color); \
} while (0) 

static inline void BuildQuad(LR_2D *r2d, int x, int y, int width, int height, float u1, float u2, float v1, float v2, uint32_t color)
{
    int c = r2d->vCount;
    //build quad
    BUILD_VERTEX(r2d->vertices[c++], x, y, u1, v1, color);
    BUILD_VERTEX(r2d->vertices[c++], x + width, y, u2, v1, color);
    BUILD_VERTEX(r2d->vertices[c++], x, y + height, u1, v2, color);
    BUILD_VERTEX(r2d->vertices[c++], x + width, y + height, u2, v2, color);
    //increment
    r2d->vCount += 4;
}

LREXPORT void LR_2D_DrawImage(LR_Context *ctx, LR_Texture *tex, int x, int y, int width, int height, uint32_t color)
{
    FRAME_CHECK_VOID("LR_2D_DrawImage");
    LR_2D *r2d = ctx->ren2d;
    CheckState(ctx, tex, 4);
    if(width <= 0) {
        width = LR_Texture_GetWidth(ctx, tex);
    }
     if(height <= 0) {
        height = LR_Texture_GetHeight(ctx, tex);
    }
    BuildQuad(r2d, x, y, width, height, 0, 1, 0, 1, color);
}

LREXPORT void LR_2D_FillRectangle(LR_Context *ctx, int x, int y, int width, int height, uint32_t color)
{
    FRAME_CHECK_VOID("LR_2D_FillRectangle");
    LR_2D *r2d = ctx->ren2d;
    CheckState(ctx, r2d->dot, 4);
    BuildQuad(r2d, x, y, width, height, 0, 0, 0, 0, color);
}

LREXPORT void LR_2D_FillRectangleColors(LR_Context *ctx, int x, int y, int width, int height, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br)
{
    FRAME_CHECK_VOID("LR_2D_FillRectangleColors");
    LR_2D *r2d = ctx->ren2d;
    CheckState(ctx, r2d->dot, 4);
    int c = r2d->vCount;
    //build quad
    BUILD_VERTEX(r2d->vertices[c++], x, y, 0, 0, tl);
    BUILD_VERTEX(r2d->vertices[c++], x + width, y, 0, 0, tr);
    BUILD_VERTEX(r2d->vertices[c++], x, y + height, 0, 0, bl);
    BUILD_VERTEX(r2d->vertices[c++], x + width, y + height, 0, 0, br);
    //increment
    r2d->vCount += 4;
}

LREXPORT void LR_2D_DrawLine(LR_Context *ctx, float x1, float y1, float x2, float y2, uint32_t color)
{
    FRAME_CHECK_VOID("LR_2D_DrawLine");
    LR_2D *r2d = ctx->ren2d;
    CheckState(ctx, r2d->dot, 4);
    int cx = r2d->vCount;
    //build quad
    float edgeY = (y2 - y1);
    float edgeX = (x2 - x1);
    float angle = atan2f(edgeY, edgeX);
    float s = sin(angle);
    float c = cos(angle);
    float x = x1;
    float y = y1;
    float w = sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1));
    BUILD_VERTEX(r2d->vertices[cx++], x, y, 0, 0, color);
    BUILD_VERTEX(r2d->vertices[cx++], x + w * c, y + w * s, 0, 0, color);
    BUILD_VERTEX(r2d->vertices[cx++], x - s, y + c, 0, 0, color);
    BUILD_VERTEX(r2d->vertices[cx++], x + w * c - s, y + w * s + c, 0, 0, color);
    //increment
    r2d->vCount += 4;
}

void LR_2D_Destroy(LR_Context *ctx, LR_2D *r2d)
{
    LR_VertexDeclaration_Free(ctx, r2d->decl);
    LR_Texture_Destroy(ctx, r2d->dot);
    free(r2d);
}


