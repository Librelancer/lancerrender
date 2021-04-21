#include "lr_material.h"
#include "lr_geometry.h"
#include "lr_context.h"
#include "lr_shader.h"
#include "lr_texture.h"
#include <stdlib.h>
#include "lr_string.h"
#include "lr_fnv1a.h"

/* We use a magic number to know if a material has been freed or not, as blockalloc 
 * will overwrite the first int of a block when freed
 */
#define LR_MATERIAL_MAGIC (1U << 30)
#define MATERIAL_VALID(x) ((*(uint32_t*)(x) == LR_MATERIAL_MAGIC))
#define HANDLE_CHECK(ctx,mat,func) if(!(mat)) LR_CriticalErrorFunc((ctx), #func ": Invalid handle")
#define MAX_SHADERS (10)

static LR_Material *FromHandle(LR_Context *ctx, LR_Handle handle)
{
    LR_Material *mat = (LR_Material*)(blockalloc_HandleToPtr(ctx->materials, handle));
    if(!mat) return NULL;
    if(!MATERIAL_VALID(mat)) return NULL;
    return mat;
}

typedef struct Sampler {
    char *name;
    LR_Texture *texture;
} Sampler;

struct INT_LR_Material_ {
    LRBLEND srcblend;
    LRBLEND destblend;
    LRCULL cull;
    LR_ShaderCollection *shaders;
    //textures
    Sampler samplers[LR_MAX_SAMPLERS];
    //material uniforms
    int hashFsMaterial;
    void *fsMaterialPtr;
    int fsMaterialSize;
    int hashVsMaterial;
    void *vsMaterialPtr;
    int vsMaterialSize;
};

LREXPORT LR_Handle LR_Material_Create(LR_Context *ctx)
{
    LR_Handle handle = blockalloc_Alloc(ctx->materials);
    if(!handle) {
        if(blockalloc_FailReason(ctx->materials) == bafail_realloc) {
            LR_CriticalErrorFunc(ctx, "LR_Material_Create: materials block realloc failed, state corrupt");
        }
        return 0;
    }
    LR_Material *mat = (LR_Material*)(blockalloc_HandleToPtr(ctx->materials, handle));
    mat->magic = LR_MATERIAL_MAGIC;
    mat->transparent = 0;
    mat->pimpl = malloc(sizeof(INT_LR_Material_));
    mat->pimpl->cull = LRCULL_CCW;
    memset(mat->pimpl, 0, sizeof(INT_LR_Material_));
    return handle;
}

LREXPORT LR_Handle LR_Material_CreateTemporary(LR_Context *ctx)
{
    FRAME_CHECK_RET("LR_Material_CreateTemporary", 0);
    LR_Handle handle = LR_Material_Create(ctx);
    LRVEC_ADD_VAL(ctx, &ctx->tempMaterials, LR_Handle, handle);
    return handle;
}

LREXPORT LR_Handle LR_Material_CloneTemporary(LR_Context *ctx, LR_Handle src)
{
    FRAME_CHECK_RET("LR_Material_CloneTemporary", 0);
    LR_Material *oldMat = FromHandle(ctx,src);
    HANDLE_CHECK(ctx, oldMat, "LR_Material_CloneTemporary");
    LR_Handle handle = LR_Material_Create(ctx);
    LRVEC_ADD_VAL(ctx, &ctx->tempMaterials, LR_Handle, handle);
    LR_Material *newMat = FromHandle(ctx, handle);
    newMat->transparent = oldMat->transparent;
    memcpy(newMat->pimpl, oldMat->pimpl, sizeof(INT_LR_Material_));
    //Copy buffers
    INT_LR_Material_ *op = oldMat->pimpl;
    INT_LR_Material_ *np = newMat->pimpl;
    if(op->vsMaterialPtr) {
       np->vsMaterialPtr = malloc(op->vsMaterialSize);
       memcpy(np->vsMaterialPtr, op->vsMaterialPtr, op->vsMaterialSize);
    }
    if(op->fsMaterialPtr) {
        np->fsMaterialPtr = malloc(op->fsMaterialSize);
        memcpy(np->fsMaterialPtr, op->fsMaterialPtr, op->fsMaterialSize);
    }
    //ret
    return handle;
}

LREXPORT void LR_Material_SetBlendMode(LR_Context *ctx, LR_Handle material, int blendEnabled, LRBLEND srcblend, LRBLEND destblend)
{
    LR_Material *mat = FromHandle(ctx,material);
    HANDLE_CHECK(ctx, mat, "LR_Material_SetBlendMode");
    mat->transparent = blendEnabled;
    if(blendEnabled) {
        mat->pimpl->srcblend = srcblend;
        mat->pimpl->destblend = destblend;
    }
}

LREXPORT void LR_Material_SetCull(LR_Context *ctx, LR_Handle material, LRCULL cull)
{
    LR_Material *mat = FromHandle(ctx,material);
    HANDLE_CHECK(ctx, mat, "LR_Material_SetCull");
    mat->pimpl->cull = cull;
}

int LR_Material_IsTransparent(LR_Context *ctx, LR_Handle material)
{
    LR_Material *mat = FromHandle(ctx,material);
    HANDLE_CHECK(ctx, mat, "LR_Material_IsTransparent");
    return mat->transparent;
}

LREXPORT void LR_Material_SetShaders(LR_Context *ctx, LR_Handle material, LR_ShaderCollection *collection)
{
    LR_Material *mat = FromHandle(ctx,material);
    HANDLE_CHECK(ctx, mat, "LR_Material_SetShaders");
    mat->pimpl->shaders = collection;
}

LREXPORT void LR_Material_SetSamplerName(LR_Context *ctx, LR_Handle material, int index, const char *name)
{
    LR_Material *mat = FromHandle(ctx,material);
    HANDLE_CHECK(ctx, mat, "LR_Material_SetSamplerName");
    if(mat->pimpl->samplers[index].name) {
        free(mat->pimpl->samplers[index].name);
    }
    mat->pimpl->samplers[index].name = lr_strdup(name);
}

LREXPORT void LR_Material_SetSamplerTex(LR_Context *ctx, LR_Handle material, int index, LR_Texture *tex)
{
    LR_Material *mat = FromHandle(ctx,material);
    HANDLE_CHECK(ctx, mat, "LR_Material_SetSamplerTex");
    mat->pimpl->samplers[index].texture = tex;
}

LREXPORT void LR_Material_SetFragmentParameters(LR_Context *ctx, LR_Handle material, void *data, int size)
{
    LR_Material *mat = FromHandle(ctx,material);
    HANDLE_CHECK(ctx, mat, "LR_Material_SetFragmentParameters");
    LR_AssertTrue(ctx, size % 16 == 0);
    INT_LR_Material_ *p = mat->pimpl;
    if(p->fsMaterialPtr) {
        free(p->fsMaterialPtr);
    }
    p->hashVsMaterial = (int)fnv1a_32(data, size);
    p->fsMaterialPtr = malloc(size);
    p->fsMaterialSize = size;
    memcpy(p->fsMaterialPtr, data, size);
}

LREXPORT void LR_Material_SetVertexParameters(LR_Context *ctx, LR_Handle material, void *data, int size)
{
    LR_Material *mat = FromHandle(ctx,material);
    HANDLE_CHECK(ctx, mat, "LR_Material_SetVertexParameters");
    LR_AssertTrue(ctx, size % 16 == 0);
    INT_LR_Material_ *p = mat->pimpl;
    if(p->vsMaterialPtr) {
        free(p->vsMaterialPtr);
    }
    p->hashVsMaterial = (int)fnv1a_32(data, size);
    p->vsMaterialPtr = malloc(size);
    p->vsMaterialSize = size;
    memcpy(p->vsMaterialPtr, data, size);
}

LREXPORT void LR_Material_Free(LR_Context *ctx, LR_Handle material)
{
    LR_Material *mat = FromHandle(ctx,material);
    HANDLE_CHECK(ctx, mat, "LR_Material_Free");
    INT_LR_Material_ *p = mat->pimpl;
    if(p->vsMaterialPtr) {
        free(p->vsMaterialPtr);
    }
    if(p->fsMaterialPtr) {
        free(p->fsMaterialPtr);
    }
    free(p);
    blockalloc_Free(ctx->materials, material);
}


void LR_Material_Prepare(LR_Context *ctx, LR_VertexDeclaration* decl, LR_DrawCommand *cmd)
{
    LR_Material *mat = FromHandle(ctx, cmd->material);
    HANDLE_CHECK(ctx, mat, "LR_Material_Prepare");
    /* Fixed-function state */
    LR_SetCull(ctx, mat->pimpl->cull);
    if(mat->transparent) {
        LR_SetBlendMode(ctx, 1, mat->pimpl->srcblend, mat->pimpl->destblend);
        LR_SetDepthMode(ctx, DEPTHMODE_NOWRITE);
    } else {
        LR_SetBlendMode(ctx, 0, 0, 0);
        LR_SetDepthMode(ctx, DEPTHMODE_ALL);
    }
    /* SHADER */
    LR_Shader *shader = LR_ShaderCollection_GetShader(ctx, mat->pimpl->shaders, decl, 0);
    INT_LR_Material_ *p = mat->pimpl;
    /* do samplers */
    for(int i = 0; i < LR_MAX_SAMPLERS; i++) {
        if(p->samplers[i].texture) {
            /* LR_Shader caches this, usually no-op */
            LR_Shader_SetSamplerIndex(ctx, shader, p->samplers[i].name, (i + 1));
            /* Check texture is loaded */
            if(LR_Texture_EnsureLoaded(ctx, p->samplers[i].texture)) {
                //bind tex
                LR_BindTex(
                    ctx, (i + 1), 
                    p->samplers[i].texture->target, 
                    p->samplers[i].texture->textureObj
                );
                //set wrap + filtering
                LR_Texture_SetFilter(ctx, p->samplers[i].texture, LRTEXFILTER_LINEAR);
            }
        }
    }
    /* material uniforms */
    if(p->fsMaterialPtr) {
        LR_Shader_SetFsMaterial(ctx, shader, p->hashFsMaterial, p->fsMaterialPtr, p->fsMaterialSize);
    }
    if(p->vsMaterialPtr) {
        LR_Shader_SetVsMaterial(ctx, shader, p->hashVsMaterial, p->vsMaterialPtr, p->vsMaterialSize);
    }
    /* do camera */
    LR_Shader_SetCamera(ctx, shader);
    /* do lighting */
    if(cmd->geometry) {
        int ltHash;
        void *ltData;
        int ltSize;
        if((ltHash = LR_GetLightingInfo(ctx, cmd->g.lighting, &ltSize, &ltData))) {
            LR_Shader_SetLighting(ctx, shader, ltHash, ltData, ltSize);
        }
        /* transform */
        LR_Shader_SetTransform(ctx, shader, cmd->g.transform);
    }
    /* use program */
    LR_BindProgram(ctx, shader->programID);
}