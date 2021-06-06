#include "lr_shader.h"
#include "lr_errors.h"
#include "lr_context.h"
#include "lr_geometry.h"
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static void PrintInfoLog(LR_Context *ctx, GLuint obj, void (APIENTRYP infolog)(GLuint, GLsizei, GLsizei*,GLchar*)) 
{
    char buffer[2048];
    buffer[2047] = '\0';
    int length;
    infolog(obj, 2047, &length, buffer);
    char *il = buffer;
    int doprint = 0;
    while(*il) {
        if(!isspace(*il)){
            doprint = 1;
            break;
        }
        il++;
    }
    if(doprint) LR_WarningFunc(ctx, buffer);
}

LREXPORT LR_Shader *LR_Shader_Create(LR_Context *ctx, const char *vertex_source, const char *fragment_source)
{
    LR_Shader *sh = (LR_Shader*)malloc(sizeof(LR_Shader));
    GL_CHECK(ctx, sh->vertexID = glCreateShader(GL_VERTEX_SHADER));
    GL_CHECK(ctx, sh->fragmentID = glCreateShader(GL_FRAGMENT_SHADER));
    GL_CHECK(ctx, sh->programID = glCreateProgram());
    //source
    GLchar const *vfiles[2];
    vfiles[0] = ctx->gles ? "#version 300 es\n" : "#version 150\n";
    vfiles[1] = vertex_source;
    GLchar const *ffiles[2];
    ffiles[0] = ctx->gles ? "#version 300 es\n" : "#version 150\n";
    ffiles[1] = fragment_source;
    GL_CHECK(ctx, glShaderSource(sh->vertexID, 2, vfiles, NULL));
    GL_CHECK(ctx, glShaderSource(sh->fragmentID, 2, ffiles, NULL));
    //compile vertex
    GL_CHECK(ctx, glCompileShader(sh->vertexID));
    GLint status;
    glGetShaderiv(sh->vertexID, GL_COMPILE_STATUS, &status);
    PrintInfoLog(ctx, sh->vertexID, glGetShaderInfoLog);
    if(!status) {
        LR_CriticalErrorFunc(ctx, "Shader compilation failed.");
    }
    //compile fragment
    GL_CHECK(ctx, glCompileShader(sh->fragmentID));
    glGetShaderiv(sh->fragmentID, GL_COMPILE_STATUS, &status);
    PrintInfoLog(ctx, sh->fragmentID, glGetShaderInfoLog);
    if(!status) {
        LR_CriticalErrorFunc(ctx, "Shader compilation failed.");
    }
    //attach
    glAttachShader(sh->programID, sh->vertexID);
    glAttachShader(sh->programID, sh->fragmentID);
    //slots
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_POSITION, "vertex_position");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_NORMAL, "vertex_normal");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_COLOR, "vertex_color");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_COLOR2, "vertex_color2");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_TEXTURE1, "vertex_texture1");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_TEXTURE2, "vertex_texture2");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_DIMENSIONS, "vertex_dimensions");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_RIGHT, "vertex_right");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_UP, "vertex_up");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_BONEWEIGHTS, "vertex_boneweights");
    glBindAttribLocation(sh->programID, LRELEMENTSLOT_BONEIDS, "vertex_boneids");
    //link
    GL_CHECK(ctx, glLinkProgram(sh->programID));
    glGetProgramiv(sh->programID, GL_LINK_STATUS, &status);
    PrintInfoLog(ctx, sh->programID, glGetProgramInfoLog);
    if(!status) {
        LR_CriticalErrorFunc(ctx, "Shader link failed.");
    }
    //init samplers
    for(int i = 0; i < LR_MAX_SAMPLERS; i++) {
        sh->samplerLocations[i] = -2;
        sh->samplerHashes[i] = 0;
    }
    //matrix uniforms
    sh->posView = glGetUniformLocation(sh->programID, "View");
    sh->posProjection = glGetUniformLocation(sh->programID, "Projection");
    sh->posViewProjection = glGetUniformLocation(sh->programID, "ViewProjection");
    sh->posWorld = glGetUniformLocation(sh->programID, "World");
    sh->posNormal = glGetUniformLocation(sh->programID, "Normal");
    //material uniforms
    sh->pos_vsMaterial = glGetUniformLocation(sh->programID, "vs_Material");
    sh->pos_fsMaterial = glGetUniformLocation(sh->programID, "fs_Material");
    sh->pos_Lighting = glGetUniformLocation(sh->programID, "Lighting");
    return sh;
}

void LR_Shader_ResetSamplers(LR_Context *ctx, LR_Shader *shader)
{
    for(int i = 0; i < LR_MAX_SAMPLERS; i++) {
        shader->samplerLocations[i] = -2;
        shader->samplerHashes[i] = 0;
    }
}

void LR_Shader_SetSamplerIndex(LR_Context *ctx, LR_Shader *shader, const char *sampler, int hash, int index)
{
    if(shader->samplerHashes[(index - 1)] != hash) {
        GLint loc = shader->samplerLocations[(index - 1)];
        if(loc == -2) {
            loc = glGetUniformLocation(shader->programID, sampler);
            shader->samplerLocations[(index - 1)] =  loc;
        }
        shader->samplerHashes[(index - 1)] = hash;
        if(loc != -1) {
            LR_BindProgram(ctx, shader->programID);
            GL_CHECK(ctx, glUniform1i(loc, index));
        }
    }
}

void LR_Shader_SetCamera(LR_Context *ctx, LR_Shader *shader)
{
    if(shader->cameraVersion == ctx->vp_version) return;
    shader->cameraVersion = ctx->vp_version;
    /* Set Uniforms*/
    LR_BindProgram(ctx, shader->programID);
    if(shader->posViewProjection != -1) glUniformMatrix4fv(shader->posViewProjection, 1, GL_FALSE, (GLfloat*)&ctx->viewprojection);
    if(shader->posProjection != -1) glUniformMatrix4fv(shader->posProjection, 1, GL_FALSE, (GLfloat*)&ctx->projection);
    if(shader->posView != -1) glUniformMatrix4fv(shader->posView, 1, GL_FALSE, (GLfloat*)&ctx->view);
}

void LR_Shader_SetTransform(LR_Context *ctx, LR_Shader *shader, LR_Handle transform)
{
    uint64_t id = ((uint64_t)ctx->currentFrame << 32) | (uint64_t)transform;
    if(shader->currentTransform != id) {
        shader->currentTransform = id;
        if(shader->posWorld != -1) glUniformMatrix4fv(shader->posWorld, 1, GL_FALSE, (GLfloat*)&LRVEC_IDX(&ctx->transforms, LR_Matrix4x4, transform));
        if(shader->posNormal != -1) glUniformMatrix4fv(shader->posNormal, 1, GL_FALSE, (GLfloat*)&LRVEC_IDX(&ctx->transforms, LR_Matrix4x4, transform + 1));
    }
}

LREXPORT LR_ShaderCollection* LR_ShaderCollection_Create(LR_Context *ctx)
{
    LR_ShaderCollection *col = malloc(sizeof(LR_ShaderCollection));
    memset(col, 0, sizeof(LR_ShaderCollection));
    return col;
}

static void AddShader(LR_Context *ctx, int caps, ShaderVariants *variants, LR_Shader *shader)
{
    if(!caps) {
        variants->defShader = shader;
    } else {
        LR_AssertTrue(ctx, variants->capPairsCount < LR_MAX_FEATURE_PAIRS);
        int c = variants->capPairsCount++;
        variants->capPairs[c].capsBits = caps;
        variants->capPairs[c].shader = shader;
    }
}

LREXPORT void LR_ShaderCollection_AddDefaultShader(LR_Context *ctx, LR_ShaderCollection *col, int caps, LR_Shader *shader)
{
    AddShader(ctx, caps, &col->defPair, shader);
}

LREXPORT void LR_ShaderCollection_AddShaderByVertex(LR_Context *ctx, LR_ShaderCollection *col, LR_VertexDeclaration *decl, int caps, LR_Shader *shader)
{
    ShaderVariants *vpair = NULL;
    uint64_t hash = decl->hash;
    /* Find entry */
    for(int i = 0; i < col->vertexSpecificCount; i++) {
        if(col->vertexSpecific[i].vertHash == hash) {
            vpair = &col->vertexSpecific[i].variants;
            break;
        }
    }
    if(!vpair) { /* Add new entry */
        int c = col->vertexSpecificCount++;
        LR_AssertTrue(ctx, c < LR_MAX_VERTEX_PAIRS);
        col->vertexSpecific[c].vertHash = hash;
        vpair = &col->vertexSpecific[c].variants;
    }
    AddShader(ctx, caps, vpair, shader);
}

LR_Shader* LR_ShaderCollection_GetShader(LR_Context *ctx, LR_ShaderCollection *col, LR_VertexDeclaration *decl, int caps)
{
    ShaderVariants *vpair = NULL;
    uint64_t hash = decl->hash;
    /* Find vertex entry */
    for(int i = 0; i < col->vertexSpecificCount; i++) {
        if(col->vertexSpecific[i].vertHash == hash) {
            vpair = &col->vertexSpecific[i].variants;
            break;
        }
    }
    if(!vpair) {
        vpair = &col->defPair;
    }
    /* Find caps entry */
    for(int i = 0; i < vpair->capPairsCount; i++) {
        if(vpair->capPairs[i].capsBits == caps)
            return vpair->capPairs[i].shader;
    }
    return vpair->defShader;
}

void LR_Shader_SetFsMaterial(LR_Context *ctx, LR_Shader *sh, int hash, void *data, int size) 
{
    if(sh->pos_fsMaterial == -1) return;
    if(sh->hash_fsMaterial == hash) return;
    sh->hash_fsMaterial = hash;
    LR_BindProgram(ctx, sh->programID);
    glUniform4fv(sh->pos_fsMaterial, (size / 16), (GLfloat*)data);
}

void LR_Shader_SetVsMaterial(LR_Context *ctx, LR_Shader *sh, int hash, void *data, int size)
{
    if(sh->pos_vsMaterial == -1) return;
    if(sh->hash_vsMaterial == hash) return;
    sh->hash_vsMaterial = hash;
    LR_BindProgram(ctx, sh->programID);
    glUniform4fv(sh->pos_vsMaterial, (size / 16), (GLfloat*)data);
}

void LR_Shader_SetLighting(LR_Context *ctx, LR_Shader *sh, int hash, void *data, int size)
{
    if(sh->pos_Lighting == -1) return;
    if(sh->hash_Lighting == hash && sh->size_Lighting == size) return;
    sh->hash_Lighting = hash;
    sh->size_Lighting = size;
    LR_BindProgram(ctx, sh->programID);
    glUniform4fv(sh->pos_Lighting, (size / 16), (GLfloat*)data);
}

void LR_Shader_SetUniformBlock(LR_Context *ctx, LR_Shader *sh, int hash, const char *name)
{
    if(sh->currentUniformBlock == hash) return;
    sh->currentUniformBlock = hash;
    GLuint index = glGetUniformBlockIndex(sh->programID, name);
    if(index != GL_INVALID_INDEX) {
        glUniformBlockBinding(sh->programID, index, 1);
    }
}

LREXPORT void LR_ShaderCollection_Destroy(LR_Context *ctx, LR_ShaderCollection *col)
{
    free(col);
}