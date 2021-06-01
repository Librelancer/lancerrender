#ifndef _LR_TEXTURE_H_
#define _LR_TEXTURE_H_
#include <lancerrender.h>
#include "lr_errors.h"
#include <glad/glad.h>

struct LR_Texture {
    uint64_t tag;
    LRTEXTYPE textureType;
    LRTEXFORMAT textureFormat;
    int resident;
    int width;
    int height;
    int maxLevel;
    int currMaxLevel;
    LRTEXFILTER setFilter;
    int setAnisotropy;
    GLenum target;
    GLuint textureObj;
    int inFbo;
};

int LR_Texture_EnsureLoaded(LR_Context *ctx, LR_Texture *tex);
void LR_Texture_SetFilter(LR_Context *ctx, LR_Texture *tex, LRTEXFILTER filter);
#endif