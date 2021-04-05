#include "lr_context.h"
#include "lr_2d.h"
#include "lr_material.h"
#include "lr_geometry.h"
#include "lr_fnv1a.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <glad/glad.h>
#include <SDL2/SDL.h>


void LR_CriticalErrorFunc(LR_Context *ctx, const char *msg)
{
    if(ctx->errorcb) {
        ctx->errorcb(LRERRORTYPE_CRITICAL, msg);
    } else {
        fprintf(stderr, "%s\n", msg);
        fflush(stderr);
        abort();
    }
}

void LR_WarningFunc(LR_Context *ctx, const char *msg)
{
    if(ctx->errorcb) {
        ctx->errorcb(LRERRORTYPE_WARN, msg);
    } else {
        fprintf(stderr, "%s\n", msg);
        fflush(stderr);
    }
}

static const char* GL_ErrorString(GLenum err)
{
    switch(err) {
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        default:
            return "Unknown Error";
    }
}

void LR_GLCheckError(LR_Context *ctx, const char *loc)
{
    GLenum err = glGetError();
    char emsg[512];
    if(err != GL_NO_ERROR) {
        snprintf(emsg, 512, "GL Error %s (0x%x) - %s", GL_ErrorString(err), err, loc);
        LR_CriticalErrorFunc(ctx, emsg);
    }
}

void LR_SetDepthMode(LR_Context *ctx, int depthMode)
{
    if(ctx->depthMode != depthMode) {
        ctx->depthMode = depthMode;
        switch(depthMode) {
            case DEPTHMODE_NONE:
                glDisable(GL_DEPTH_TEST);
                break;
            case DEPTHMODE_NOWRITE:
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
                break;
            default:
                glEnable(GL_DEPTH_TEST);
                glDepthMask(GL_TRUE);
                break;
        }
    }
}

static void CheckExtensions(LR_Context *ctx)
{
    GLint n, i;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    int xAni = 1;
    for(i = 0; i < n; i++) {
        const char *ext = (const char *)glGetStringi(GL_EXTENSIONS, i);
        if(xAni && !strcmp(ext, "GL_EXT_texture_filter_anisotropic")) {
            xAni = 0;
            ctx->anisotropy = 1;
            glGetIntegerv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &ctx->maxAnisotropy);
        }
        if(!xAni) break;
    }
}

LREXPORT LR_Context *LR_Init(int gles)
{
    LR_Context *ctx = (LR_Context*)malloc(sizeof(struct LR_Context));
    memset(ctx, 0, sizeof(struct LR_Context));
    if(!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        free((void*)ctx);
        return NULL;
    }
    ctx->gles = gles;
    CheckExtensions(ctx);
    ctx->ren2d = LR_2D_Init(ctx);
    ctx->cullMode = LRCULL_CCW;
    ctx->commandsCapacity = LR_INITIAL_CAPACITY;
    ctx->commands = (LR_DrawCommand*)malloc(LR_INITIAL_CAPACITY * sizeof(LR_DrawCommand));
    ctx->lightingInfo = malloc(LR_INITIAL_CAPACITY * 64);
    ctx->lightingSize = LR_INITIAL_CAPACITY * 64;
    ctx->transformsCapacity = LR_INITIAL_TRANSFORM_CAPACITY;
    ctx->transforms = (LR_Matrix4x4*)malloc(LR_INITIAL_TRANSFORM_CAPACITY * sizeof(LR_Matrix4x4));
    ctx->materials = blockalloc_Init(sizeof(LR_Material), LR_MAX_MATERIAL_ADDRESS);
    glDisable(GL_BLEND);
    return ctx;
}

#define OFFSET_PTR(type,ptr,offset)(  (type*)(&((char*)(ptr))[(offset)])  )
#define ALIGN_VEC4(x) ((x) + (-(x) & 15))

LREXPORT LR_Handle LR_SetLights(LR_Context *ctx, void *data, int size)
{
    FRAME_CHECK_RET("LR_SetLights", 0);
    if(!size) return 0;
    int hash = fnv1a_32(data, size);
    int allocSize = ALIGN_VEC4(size);
    if(ctx->lastLighting) {
         LR_LightingInfo info = *OFFSET_PTR(LR_LightingInfo, ctx->lightingInfo, ctx->lastLighting - 1);
         if(info.size == allocSize && info.hash == hash)
            return ctx->lastLighting;
    }
    //ensure size
    int reqSize = ctx->lightingPtr + allocSize + sizeof(LR_LightingInfo);
    if(ctx->lightingSize < reqSize) {
        int s2 = ctx->lightingSize;
        while(s2 < reqSize) s2 *= 2;
        ctx->lightingInfo = realloc(ctx->lightingInfo, s2);
        ctx->lightingSize = s2;
    }
    //header
    LR_LightingInfo *info = OFFSET_PTR(LR_LightingInfo, ctx->lightingInfo, ctx->lightingPtr);
    info->size = allocSize;
    info->hash = hash;
    //copy data
    memcpy(OFFSET_PTR(void, ctx->lightingInfo, ctx->lightingPtr + sizeof(LR_LightingInfo)), data, size);
    //pad with zero
    if(size < allocSize) {
        for(int i = size; i < allocSize; i++) {
            *OFFSET_PTR(char, ctx->lightingInfo, ctx->lightingPtr + sizeof(LR_LightingInfo) + i) = 0;
        }
    }
    LR_Handle handle = ctx->lightingPtr + 1;
    ctx->lightingPtr += allocSize + sizeof(LR_LightingInfo);
    ctx->lastLighting = handle;
    return handle;
}

int LR_GetLightingInfo(LR_Context *ctx, LR_Handle h, int *outSize, void **outData)
{
    if(!h) return 0;
    LR_LightingInfo info = *OFFSET_PTR(LR_LightingInfo, ctx->lightingInfo, h - 1);
    *outData = OFFSET_PTR(void, ctx->lightingInfo, h - 1 + sizeof(LR_LightingInfo));
    *outSize = info.size;
    return info.hash;
}


LREXPORT void LR_SetErrorCallback(LR_Context *ctx, LR_ErrorCallback cb)
{
    ctx->errorcb = cb;
}

void LR_BindVAO(LR_Context *ctx, GLuint vao)
{
    if(ctx->bound_vao != vao) {
        ctx->bound_vao = vao;
        GL_CHECK(ctx, glBindVertexArray(vao));
    }
}

void LR_BindProgram(LR_Context *ctx, GLuint program)
{
    if(ctx->bound_program != program) {
        ctx->bound_program = program;
        GL_CHECK(ctx, glUseProgram(program));
    }
}

void LR_UnbindTex(LR_Context *ctx, GLuint tex)
{
    for(int i = 0; i < LR_MAX_TEXTURES; i++) {
        if(ctx->bound_textures[i] == tex)
            ctx->bound_textures[i] = 0;
    }
}

void LR_BindTexForModify(LR_Context *ctx, GLenum target, GLuint tex)
{
    if(ctx->bound_textures[ctx->currentUnit] == tex) return;
    LR_BindTex(ctx, 0, target, tex);
}

void LR_BindTex(LR_Context *ctx, int unit, GLenum target, GLuint tex)
{
    LR_AssertTrue(ctx, unit >= 0 && unit < LR_MAX_TEXTURES);
    if(ctx->currentUnit != unit) {
        glActiveTexture(GL_TEXTURE0 + unit);
        ctx->currentUnit = unit;
    }
    if(ctx->bound_textures[unit] != tex) {
        GL_CHECK(ctx, glBindTexture(target, tex));
        ctx->bound_textures[unit] = tex;
    }
}

static GLenum GLBlendMode(LRBLEND b) {
    switch(b) {
        case LRBLEND_SRCALPHA:
            return GL_SRC_ALPHA;
        case LRBLEND_INVSRCALPHA:
            return GL_ONE_MINUS_SRC_ALPHA;
        case LRBLEND_ONE:
            return GL_ONE;
        case LRBLEND_ZERO:
        default:
            return GL_ZERO;
    }
}

void LR_SetBlendMode(LR_Context *ctx, int blendEnabled, LRBLEND srcblend, LRBLEND destblend)
{
    if(blendEnabled) {
        if(!ctx->blendEnabled) {
            glEnable(GL_BLEND);
            ctx->blendEnabled = 1;
        }
        if(srcblend != ctx->srcblend || destblend != ctx->destblend) {
            glBlendFunc(GLBlendMode(srcblend), GLBlendMode(destblend));
            ctx->srcblend = srcblend;
            ctx->destblend = destblend;
        }
    } else {
        if(ctx->blendEnabled) {
            glDisable(GL_BLEND);
        }
        ctx->blendEnabled = 0;
    }
}

void LR_SetCull(LR_Context *ctx, LRCULL cull)
{
    if(ctx->cullMode != cull) {
        ctx->cullMode = cull;
        if(cull == LRCULL_NONE) {
            glDisable(GL_CULL_FACE);
        } else if (cull == LRCULL_CW) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_FRONT);
        } else if (cull == LRCULL_CCW) {
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);
        }
    }
}

void LR_ReloadTex(LR_Context *ctx, LR_Texture *tex)
{
    if(ctx->texcb) {
        ctx->texcb(ctx, tex);
    }
}


LREXPORT void LR_ClearAll(LR_Context *ctx, float red, float green, float blue, float alpha)
{
    FRAME_CHECK_VOID("LR_ClearAll");
    glClearColor(red,green,blue,alpha);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

static GLenum GLPrim(LR_Context *ctx, LRPRIMTYPE ptype)
{
    if(ptype == LRPRIMTYPE_TRIANGLELIST)
        return GL_TRIANGLES;
    else if (ptype == LRPRIMTYPE_TRIANGLESTRIP)
        return GL_TRIANGLE_STRIP;
    else if (ptype == LRPRIMTYPE_LINELIST)
        return GL_LINES;
    
    LR_CriticalErrorFunc(ctx, "Invalid primitive type");
    return 0; //suppress warning
}

static void LR_FlushDraws(LR_Context *ctx)
{
    LR_Flush2D(ctx);
    if(!ctx->commandPtr) return;
    LR_CmdSort(ctx);
    for(int i = 0; i < ctx->commandPtr; i++) {
        LR_DrawCommand *cmd = &ctx->commands[i];
        LR_Material_Prepare(ctx, cmd->geometry->decl, cmd);
        LR_BindVAO(ctx, cmd->geometry->vao);
        GL_CHECK(ctx, glDrawElementsBaseVertex(
            GLPrim(ctx, cmd->primitive),
            cmd->countIndex,
            GL_UNSIGNED_SHORT,
            GL_OFFSET(cmd->startIndex * 2),
            cmd->baseVertex)
        );
    }
    ctx->commandPtr = 0;
}

LREXPORT void LR_SetCamera(
    LR_Context *ctx, 
    LR_Matrix4x4 *view, 
    LR_Matrix4x4 *projection,
    LR_Matrix4x4 *viewprojection
)
{
    if(ctx->inframe) {
        LR_FlushDraws(ctx);
    }
    ctx->vp_version++;
    ctx->view = *view;
    ctx->projection = *projection;
    ctx->viewprojection = *viewprojection;
}

static void LR_AddCommand(LR_Context *ctx, LR_DrawCommand *cmd)
{
    LR_Flush2D(ctx);
    if((ctx->commandPtr + 1) >= ctx->commandsCapacity) {
        ctx->commandsCapacity *= 2;
        ctx->commands = realloc((void*)ctx->commands, ctx->commandsCapacity * sizeof(LR_DrawCommand));
    }
    ctx->commands[ctx->commandPtr++] = *cmd;
}

static void LR_AddTransform(LR_Context *ctx, LR_Matrix4x4 *world, LR_Matrix4x4 *normal)
{
    if((ctx->transformPtr + 2) >= ctx->transformsCapacity) {
        ctx->transformsCapacity *= 2;
        ctx->transforms = realloc((void*)ctx->transforms, ctx->transformsCapacity * sizeof(LR_Matrix4x4));
    }
    ctx->transforms[ctx->transformPtr++] = *world;
    ctx->transforms[ctx->transformPtr++] = *normal;
}

LREXPORT void LR_ClearDepth(LR_Context *ctx)
{
    FRAME_CHECK_VOID("LR_ClearDepth");
    LR_FlushDraws(ctx);
    glClear(GL_DEPTH_BUFFER_BIT);
}

LREXPORT LR_Handle LR_AllocTransform(LR_Context *ctx, LR_Matrix4x4 *world, LR_Matrix4x4 *normal)
{
    FRAME_CHECK_RET("LR_AllocTransform", -1);
    LR_Handle retval = (LR_Handle)ctx->transformPtr;
    LR_AddTransform(ctx, world, normal);
    return retval;
}

LREXPORT void LR_BeginFrame(LR_Context *ctx, int width, int height)
{
    LR_AssertTrue(ctx, !ctx->inframe);
    LR_Viewport vp = { .x = 0, .y = 0, .width = width, .height = height };
    ctx->viewports[0] = vp;
    GL_CHECK(ctx, glViewport(0, 0, width, height));
    ctx->inframe = 1;
    ctx->currentFrame++;
    ctx->transformPtr = 0;
    ctx->lightingPtr = 0;
    ctx->lastLighting = 0;
}

LREXPORT void LR_PushViewport(LR_Context *ctx, int x, int y, int width, int height)
{
    FRAME_CHECK_VOID("LR_PushViewport");
    LR_FlushDraws(ctx);
    LR_Viewport vp = { .x = 0, .y = 0, .width = width, .height = height };
    if(ctx->viewportSP >= LR_MAX_VIEWPORTS) {
        LR_CriticalErrorFunc(ctx, "Viewport stack overflow (" LX_TOSTRING(LR_MAX_VIEWPORTS) ")");
        return;
    }
    ctx->viewportSP++;
    ctx->viewports[ctx->viewportSP] = vp;
    GL_CHECK(ctx, glViewport(x, y, width, height));
}

LREXPORT void LR_PopViewport(LR_Context *ctx)
{
    FRAME_CHECK_VOID("LR_PopViewport");
    LR_FlushDraws(ctx);
    if(ctx->viewportSP <= 0) {
        LR_CriticalErrorFunc(ctx, "Viewport stack underflow");
        return;
    }
    ctx->viewportSP--;
    LR_Viewport vp = ctx->viewports[ctx->viewportSP];
    GL_CHECK(ctx, glViewport(vp.x, vp.y, vp.width, vp.height));
}

LREXPORT void LR_Scissor(LR_Context *ctx, int x, int y, int width, int height)
{
    FRAME_CHECK_VOID("LR_Scissor");
    LR_FlushDraws(ctx);
    if(!ctx->scissorEnabled) {
        ctx->scissorEnabled = 1;
        glEnable(GL_SCISSOR_TEST);
    }
    int vpH = ctx->viewports[ctx->viewportSP].height;
    if(width < 1) width = 1;
    if(height < 1) height = 1;
    GL_CHECK(ctx, glScissor(x, vpH - y - height, width, height));
}

LREXPORT void LR_ClearScissor(LR_Context *ctx)
{
    FRAME_CHECK_VOID("LR_ClearScissor");
    LR_FlushDraws(ctx);
    if(ctx->scissorEnabled) {
        ctx->scissorEnabled = 0;
        glDisable(GL_SCISSOR_TEST);
    }
}

LREXPORT void LR_EndFrame(LR_Context *ctx)
{
    LR_AssertTrue(ctx, ctx->inframe);
    LR_AssertTrue(ctx, ctx->viewportSP == 0);
    LR_FlushDraws(ctx);
    if(ctx->scissorEnabled) {
        ctx->scissorEnabled = 0;
        glDisable(GL_SCISSOR_TEST);
    }
    ctx->inframe = 0;
}

LREXPORT void LR_Draw(
    LR_Context *ctx,
    LR_Handle material,
    LR_Geometry *geometry,
    LR_Handle transform,
    LR_Handle lighting,
    LRPRIMTYPE primitive,
    float zval,
    int baseVertex,
    int startIndex,
    int indexCount
)
{
    FRAME_CHECK_VOID("LR_Draw");
    uint64_t key = 0;
    if(LR_Material_IsTransparent(ctx, material)) {
        //convert Z to fixed point to allow it to be compared in int form
        uint64_t reg = ((uint64_t)zval) & (1UL << 36); //max Z is (1 << 36) away
        int fracBits = (1 << 23); //32-bit float can have 23 bits of precision.
        int frac = ((int)zval * fracBits) & fracBits;
        key = (uint64_t)reg << 59 | (uint64_t)frac;
    } else {
        //opaque drawn first by setting highest bit in key
        //sort by material
        key = (1UL << 63) | (uint64_t)material;
    }
    LR_DrawCommand command = {
        .key = key,
        .material = material,
        .geometry = geometry,
        .transform = transform,
        .lighting = lighting,
        .primitive = primitive,
        .baseVertex = baseVertex,
        .startIndex = startIndex,
        .countIndex = indexCount
    };
    LR_AddCommand(ctx, &command);
}

LREXPORT void LR_Destroy(LR_Context *ctx)
{
    LR_AssertTrue(ctx, !ctx->inframe);
    blockalloc_Destroy(ctx->materials);
    LR_2D_Destroy(ctx, ctx->ren2d);
    free((void*)ctx->commands);
    free((void*)ctx->transforms);
    free((void*)ctx);
}
