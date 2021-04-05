#ifndef _LR_SHADER_H_
#define _LR_SHADER_H_
#include <lancerrender.h>
#include <glad/glad.h>

#define LR_MAX_SAMPLERS (8)

struct LR_Shader {
    GLuint programID;
    GLuint vertexID;
    GLuint fragmentID;
    GLint samplerLocations[LR_MAX_SAMPLERS];
    GLint posView;
    GLint posProjection;
    GLint posViewProjection;
    GLint posWorld;
    GLint posNormal;
    GLint pos_vsMaterial;
    GLint pos_fsMaterial;
    GLint pos_Lighting;
    int hash_fsMaterial;
    int hash_vsMaterial;
    int cameraVersion; 
    int hash_Lighting;
    int size_Lighting;  
    uint64_t currentTransform;
};

#define LR_MAX_VERTEX_PAIRS (8)
#define LR_MAX_FEATURE_PAIRS (16)

typedef struct ShaderPair {
    int capsBits;
    LR_Shader *shader;
} ShaderPair; //12 bytes
 
typedef struct ShaderVariants {
    LR_Shader *defShader; //8 bytes
    ShaderPair capPairs[LR_MAX_FEATURE_PAIRS]; //(16 * 12) = 192 bytes
    int capPairsCount; //4 bytes
} ShaderVariants; //204 bytes

typedef struct ShaderVertexPair {
    uint64_t vertHash; //8 bytes
    ShaderVariants variants; //204 bytes
} ShaderVertexPair; //212 bytes

struct LR_ShaderCollection {
    ShaderVariants defPair; //204 bytes
    ShaderVertexPair vertexSpecific[LR_MAX_VERTEX_PAIRS]; //1696 bytes
    int vertexSpecificCount; //4 bytes
}; //1904 bytes

void LR_Shader_ResetSamplers(LR_Context *ctx, LR_Shader *shader);

/* index is 1-based, texture unit 0 is reserved for modifying texture state */
void LR_Shader_SetSamplerIndex(LR_Context *ctx, LR_Shader *shader, const char *sampler, int index);

LR_Shader* LR_ShaderCollection_GetShader(LR_Context *ctx, LR_ShaderCollection *col, LR_VertexDeclaration *decl, int caps);
void LR_Shader_SetCamera(LR_Context *ctx, LR_Shader *shader);
void LR_Shader_SetTransform(LR_Context *ctx, LR_Shader *shader, LR_Handle transform);
void LR_Shader_SetFsMaterial(LR_Context *ctx, LR_Shader *sh, int hash, void *data, int size);
void LR_Shader_SetVsMaterial(LR_Context *ctx, LR_Shader *sh, int hash, void *data, int size);
void LR_Shader_SetLighting(LR_Context *ctx, LR_Shader *sh, int hash, void *data, int size);
#endif
