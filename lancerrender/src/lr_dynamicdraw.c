#include "lr_dynamicdraw.h"
#include "lr_geometry.h"
#include "lr_context.h"
#include "lr_errors.h"
#include "lr_material.h"
#include <stdlib.h>
#include <string.h>

#define DDRAW_MAX_INDICES (32)
#define DDRAW_INITIAL_CAPACITY (64)
struct LR_DynamicDraw {
    LR_Geometry *streamingGeometry;
    LR_VertexDeclaration *decl;
    LR_Handle material;
    int vertexBuffSize;
    int indexBuffSize;
    void *vertexStream;
    int vertexPtr;
    uint16_t *indexStream;
    int indexPtr;
    int samplerIndex;
    int vertexCount; //vertices/draw
    int indexCount; //indices/draw
    LR_Texture *lastDrawTex;
    uint16_t indexTemplate[DDRAW_MAX_INDICES];
};

LREXPORT LR_DynamicDraw *LR_DynamicDraw_Create(
    LR_Context *ctx, 
    LR_VertexDeclaration *vtx,
    LR_Handle material,
    int vertexCount, 
    int indexCount, 
    uint16_t *indexTemplate
)
{
    LR_AssertTrue(ctx, indexCount <= DDRAW_MAX_INDICES);
    LR_DynamicDraw *dd = malloc(sizeof(LR_DynamicDraw));
    memset(dd, 0, sizeof(LR_DynamicDraw));
    dd->decl = vtx;
    dd->vertexBuffSize = DDRAW_INITIAL_CAPACITY * vertexCount;
    dd->indexBuffSize = DDRAW_INITIAL_CAPACITY * indexCount;
    dd->material = material;
    dd->vertexCount = vertexCount;
    dd->indexCount = indexCount;
    dd->samplerIndex = -1;
    dd->streamingGeometry = LR_StreamingGeometry_Create(ctx, vtx, DDRAW_INITIAL_CAPACITY * vertexCount, DDRAW_INITIAL_CAPACITY * indexCount);
    memcpy(dd->indexTemplate, indexTemplate, indexCount * sizeof(uint16_t));
    return dd;
}

LREXPORT void LR_DynamicDraw_SetSamplerIndex(LR_Context *ctx, LR_DynamicDraw *dd, int index)
{
    dd->samplerIndex = index;
}

LREXPORT void LR_DynamicDraw_Draw(LR_Context *ctx, LR_DynamicDraw *dd, void *vertexData, int vertexDataSize, LR_Texture *tex, float zVal)
{
    FRAME_CHECK_VOID("LR_DynamicDraw_Draw");
    LR_AssertTrue(ctx, vertexDataSize == dd->decl->stride * dd->vertexCount);
    if(!dd->vertexStream) dd->vertexStream = LR_StreamingGeometry_Begin(ctx, dd->streamingGeometry);
    if((dd->vertexPtr + dd->vertexCount) > dd->vertexBuffSize) {
        dd->vertexBuffSize *= 2;
        dd->vertexStream = LR_StreamingGeometry_Resize(ctx, dd->streamingGeometry, dd->vertexBuffSize * dd->decl->stride);
    }
    memcpy(&((char*)dd->vertexStream)[dd->vertexPtr * dd->decl->stride], vertexData, vertexDataSize);
    //add draw call
    LR_DrawCommand cmd = {
        .key = KEY_FROMZ(zVal),
        .material = dd->material,
        .geometry = NULL,
        .d = {
            .dd = dd,
            .tex = tex,
            .baseVertex = dd->vertexPtr
        }
    };
    LRVEC_ADD_VAL(ctx, &ctx->commands, LR_DrawCommand, cmd);
    //increment
    dd->vertexPtr += dd->vertexCount;
}

void LR_DynamicDraw_Flush(LR_Context *ctx, LR_DynamicDraw *dd)
{
    if(!dd->indexPtr) return;
    LR_StreamingGeometry_FinishIndices(ctx, dd->streamingGeometry, dd->indexPtr);
    LR_BindVAO(ctx, dd->streamingGeometry->vao);
    if(dd->samplerIndex != -1) {
        LR_Material_SetSamplerTex(ctx, dd->material, dd->samplerIndex, dd->lastDrawTex);
    }
    LR_DrawCommand cmd = {
        .material = dd->material,
        .geometry = NULL
    };
    LR_Material_Prepare(ctx, dd->decl, &cmd);
    GL_CHECK(ctx, glDrawElements(GL_TRIANGLES, dd->indexPtr, GL_UNSIGNED_SHORT, NULL));
    dd->indexStream = NULL;
    dd->indexPtr = 0;
    dd->lastDrawTex = NULL;
}

void LR_DynamicDraw_Process(LR_Context *ctx, LR_DrawCommand *cmd)
{
    LR_DynamicDraw *dd = cmd->d.dd;
    if(dd->vertexStream) {
        LR_StreamingGeometry_Finish(ctx, dd->streamingGeometry, dd->vertexPtr);
        dd->vertexStream = NULL;
    }
    if(dd->lastDrawTex && dd->lastDrawTex != cmd->d.tex) {
        LR_DynamicDraw_Flush(ctx, dd);
    }
    dd->lastDrawTex = cmd->d.tex;
    if(dd->indexPtr + dd->indexCount >= dd->indexBuffSize) {
        dd->indexBuffSize *= 2;
        dd->indexStream = LR_StreamingGeometry_ResizeIndices(ctx, dd->streamingGeometry, dd->indexBuffSize * sizeof(uint16_t));
    }
    if(!dd->indexStream) {
        dd->indexStream = LR_StreamingGeometry_BeginIndices(ctx, dd->streamingGeometry);
    }
    for(int i = 0; i < dd->indexCount; i++) {
        dd->indexStream[dd->indexPtr++] = dd->indexTemplate[i] + (cmd->d.baseVertex);
    }
}

LREXPORT void LR_DynamicDraw_Destroy(LR_Context *ctx, LR_DynamicDraw *dd)
{
    //TODO: free geometries
    free(dd);
}