#ifndef _LR_CONTEXT_H_
#define _LR_CONTEXT_H_
#include <lancerrender.h>
#include "lr_errors.h"
#include "lr_blockalloc.h"
#include <stdint.h>
#include <glad/glad.h>

#define LR_MAX_VIEWPORTS (8)
#define LR_MAX_TEXTURES (8)
#define LR_MAX_MATERIAL_ADDRESS (1U << 23)
#define LR_INITIAL_CAPACITY (256)
#define LR_INITIAL_TRANSFORM_CAPACITY (256)

typedef struct LR_Viewport {
    int x;
    int y;
    int width;
    int height;
} LR_Viewport;


typedef struct LR_DrawCommand {
    uint64_t key;
    LR_Handle material; 
    LR_Geometry *geometry;
    LR_Handle transform; 
    uint64_t lightingHash;
    LR_Handle lighting; 
    LR_Handle objectData;
    LR_Handle uniformBuffer; 
    LR_Handle uniformBufferOffset;
    LRPRIMTYPE primitive;
    int baseVertex;
    int startIndex;
    int countIndex;
} LR_DrawCommand;

#include "lr_2d.h"

#define DEPTHMODE_ALL (0)
#define DEPTHMODE_NOWRITE (1)
#define DEPTHMODE_NONE (2)

typedef struct LR_LightingInfo {
    int size;
    int hash;
} LR_LightingInfo;

struct LR_Context {
    /* context info */
    int gles;
    LR_ErrorCallback errorcb;
    LR_TexLoadCallback texcb;
    /* gl state */
    int anisotropy;
    int maxAnisotropy;
    int scissorEnabled;
    LRCULL cullMode;
    int blendEnabled;
    LRBLEND srcblend;
    LRBLEND destblend;
    GLuint bound_program;
    GLuint bound_vao;
    GLuint bound_textures[LR_MAX_TEXTURES];
    int currentUnit;
    int depthMode;
    /* lr objects */
    BlockAlloc *materials;
    LR_2D *ren2d;
    /* camera */
    int vp_version;
    LR_Matrix4x4 view;
    LR_Matrix4x4 projection;
    LR_Matrix4x4 viewprojection;
    /* frame */
    int inframe;
    /* viewport */
    int viewportSP;
    LR_Viewport viewports[LR_MAX_VIEWPORTS];
    /* transforms */
    int currentFrame;
    LR_Matrix4x4 *transforms;
    int transformsCapacity;
    int transformPtr;
    /* commands */
    LR_DrawCommand *commands;
    int commandsCapacity;
    int commandPtr;
    /* lighting */
    LR_Handle lastLighting;
    void *lightingInfo;
    int lightingPtr;
    int lightingSize;
};



#define LR_LightingInfoPtr(x) ((void*)((char*)(x) + sizeof(LR_LightingInfo)))

#define FRAME_CHECK_VOID(name) do { if(!ctx->inframe) { LR_CriticalErrorFunc(ctx, #name " must call LR_BeginFrame"); return; } } while (0)
#define FRAME_CHECK_RET(name, x) do { if(!ctx->inframe) { LR_CriticalErrorFunc(ctx, #name " must call LR_BeginFrame"); return (x); } } while (0)
#define GL_OFFSET(x) ((void*)(uintptr_t)(x))

int LR_GetLightingInfo(LR_Context *ctx, LR_Handle h, int *outSize, void **outData);

/* defined in lr_sort.c */
void LR_CmdSort(LR_Context *ctx);
/* GL State */
void LR_BindProgram(LR_Context *ctx, GLuint program);
void LR_BindVAO(LR_Context *ctx, GLuint vao);
void LR_UnbindTex(LR_Context *ctx, GLuint tex);
void LR_BindTex(LR_Context *ctx, int unit, GLenum target, GLuint tex);
void LR_BindTexForModify(LR_Context *ctx, GLenum target, GLuint tex);
void LR_ReloadTex(LR_Context *ctx, LR_Texture *tex);
void LR_SetBlendMode(LR_Context *ctx, int blendEnabled, LRBLEND srcblend, LRBLEND destblend);
void LR_SetCull(LR_Context *ctx, LRCULL cull);
void LR_SetDepthMode(LR_Context *ctx, int depthMode);
#endif
