#include <lancerrender_shaderfile.h>
#include "miniz.h"
#include "lr_errors.h"

typedef struct GlslShader {
    int caps;
    char *vertex;
    char *fragment;
} GlslShader;

typedef struct BuiltShaderFile {
    int nshaders;
    GlslShader *shaders;
} BuiltShaderFile;

static void FreeParsedFile(BuiltShaderFile *file)
{
    for(int i = 0; i < file->nshaders; i++) {
        free(file->shaders[i].vertex);
        free(file->shaders[i].fragment);
    }
    free(file->shaders);
    free(file);
}

static BuiltShaderFile* ReadFile(SDL_RWops *rw)
{
    uint32_t magic;
    uint32_t compsize;
    uint32_t decompsize;
    if(!rw->read(rw, &magic, 4, 1)) return NULL;
    if(magic != 0xABCDABCD) return NULL;
    if(!rw->read(rw, &compsize, 4, 1)) return NULL;
    if(!rw->read(rw, &decompsize, 4, 1)) return NULL;
    unsigned char *indata = malloc(compsize);
    if(!rw->read(rw, indata, compsize, 1)) {
        free(indata);
        return NULL;
    }
    unsigned char *decomp = malloc(decompsize);
    mz_ulong decompResult = decompsize;
    int res = uncompress(decomp, &decompResult, indata, compsize);
    free(indata);
    if(decompsize != decompResult ||
        res != Z_OK) {
        free(decomp);
        return NULL;
    }
    BuiltShaderFile *shf = malloc(sizeof(BuiltShaderFile));
    unsigned char *dcptr = decomp;
    #define READ_INT(output) do { \
        output = *(uint32_t*)dcptr; \
        dcptr += 4; \
    } while(0)
    READ_INT(shf->nshaders);
    shf->shaders = malloc(shf->nshaders * sizeof(GlslShader));
    for(int i = 0; i < shf->nshaders; i++) {
        READ_INT(shf->shaders[i].caps);
        int vlen, flen;
        READ_INT(vlen);
        shf->shaders[i].vertex = malloc(vlen + 1);
        memcpy(shf->shaders[i].vertex, dcptr, vlen);
        dcptr += vlen;
        shf->shaders[i].vertex[vlen] = 0;
        READ_INT(flen);
        shf->shaders[i].fragment = malloc(flen + 1);
        memcpy(shf->shaders[i].fragment, dcptr, flen);
        shf->shaders[i].fragment[flen] = 0;
        dcptr += flen;
    }
    #undef READ_INT
    free(decomp);
    return shf;
}



LREXPORT void LR_ShaderCollection_DefaultShadersFromFile(LR_Context *ctx, LR_ShaderCollection *col, SDL_RWops *rw)
{
    BuiltShaderFile *file = ReadFile(rw);
    if(!file) {
        LR_CriticalErrorFunc(ctx, "invalid shader file passed to LR_ShaderCollection_DefaultShadersFromFile");
        return;
    }
    for(int i = 0; i < file->nshaders; i++) {
        LR_Shader *sh = LR_Shader_Create(ctx, file->shaders[i].vertex, file->shaders[i].fragment);
        LR_ShaderCollection_AddDefaultShader(ctx, col, file->shaders[i].caps, sh);
    }
    FreeParsedFile(file);
}

LREXPORT void LR_ShaderCollection_AddShadersFromFile(LR_Context *ctx, LR_ShaderCollection *col, LR_VertexDeclaration *decl, SDL_RWops *rw)
{
    BuiltShaderFile *file = ReadFile(rw);
    if(!file) {
        LR_CriticalErrorFunc(ctx, "invalid shader file passed to LR_ShaderCollection_AddShadersFromFile");
        return;
    }
    for(int i = 0; i < file->nshaders; i++) {
        LR_Shader *sh = LR_Shader_Create(ctx, file->shaders[i].vertex, file->shaders[i].fragment);
        LR_ShaderCollection_AddShaderByVertex(ctx, col, decl, file->shaders[i].caps, sh);
    }
    FreeParsedFile(file);
}