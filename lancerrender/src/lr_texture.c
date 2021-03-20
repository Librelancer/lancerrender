#include "lr_texture.h"
#include "lr_context.h"
#include <string.h>
#include <stdlib.h>

static GLenum internalFormats[LRTEXFORMAT_COUNT] = {
    GL_NONE, //INVALID
    GL_RGBA, //Color
    GL_RGB, //Bgr565
    GL_RGB5_A1, //Bgra5551
    GL_RGBA4,  //Bgra4444
    GL_COMPRESSED_RGBA_S3TC_DXT1_EXT, //Dxt1
    GL_COMPRESSED_RGBA_S3TC_DXT3_EXT, //Dxt3
    GL_COMPRESSED_RGBA_S3TC_DXT5_EXT, //Dxt5
    GL_R8, //R8
    GL_DEPTH_COMPONENT, //Depth
};
static GLenum glFormats[LRTEXFORMAT_COUNT] = {
    GL_NONE, //INVALID
    GL_BGRA, //Color
    GL_RGB, //Bgr565
    GL_BGRA, //Bgra5551
    GL_RGBA, //Bgra4444
    GL_NUM_COMPRESSED_TEXTURE_FORMATS, //Dxt1
    GL_NUM_COMPRESSED_TEXTURE_FORMATS, //Dxt3
    GL_NUM_COMPRESSED_TEXTURE_FORMATS, //Dxt5
    GL_RED, //R8
    GL_DEPTH_COMPONENT, //Depth
};
static GLenum glTypes[LRTEXFORMAT_COUNT] = {
    GL_NONE, //INVALID
    GL_UNSIGNED_BYTE, //Color
    GL_UNSIGNED_SHORT_5_6_5, //Bgr565
    GL_UNSIGNED_SHORT_1_5_5_5_REV, //Bgra5551
    GL_UNSIGNED_SHORT_4_4_4_4, //Bgra4444
    GL_UNSIGNED_BYTE, //Dxt1
    GL_UNSIGNED_BYTE, //Dxt3
    GL_UNSIGNED_BYTE, //Dxt5
    GL_UNSIGNED_BYTE, //R8
    GL_FLOAT, //Depth
};

LREXPORT uint64_t LR_Texture_GetTag(LR_Context *ctx, LR_Texture *tex)
{
    return tex->tag;
}

LREXPORT LR_Texture* LR_Texture_Create(LR_Context *ctx, uint64_t tag)
{
    LR_Texture *tex = malloc(sizeof(LR_Texture));
    memset(tex, 0, sizeof(LR_Texture));
    tex->tag = tag;
    return tex;
}

LREXPORT void LR_Texture_Unload(LR_Context *ctx, LR_Texture *tex)
{
    LR_AssertTrue(ctx, tex->resident);
    GL_CHECK(ctx, glDeleteTextures(1, &tex->textureObj));
    tex->textureObj = 0;
    tex->resident = 0;
}

LREXPORT void LR_Texture_Destroy(LR_Context *ctx, LR_Texture *tex)
{
    if(tex->resident) LR_Texture_Unload(ctx, tex);
    free(tex);
}

static int CompressedSize(GLenum fmt, int width, int height)
{
    int sz = 0;
    switch(fmt) {
        case GL_COMPRESSED_RGB_S3TC_DXT1_EXT:
            sz = 8;
            break;
        case GL_COMPRESSED_RGBA_S3TC_DXT3_EXT:
        case GL_COMPRESSED_RGBA_S3TC_DXT5_EXT:
            sz = 16;
            break;
    }
    return ((width + 3) / 4) * ((height + 3) / 4) * sz;
}

LREXPORT void LR_Texture_Allocate(LR_Context *ctx, LR_Texture *tex, LRTEXTYPE type, LRTEXFORMAT format, int width, int height)
{
    if(tex->textureObj) {
        LR_UnbindTex(ctx, tex->textureObj);
        GL_CHECK(ctx, glDeleteTextures(1, &tex->textureObj));
    }
    glGenTextures(1, &tex->textureObj);
    tex->resident = 1;
    tex->width = width;
    tex->height = height;
    tex->textureFormat = format;
    tex->textureType = type;
    tex->target = (type == LRTEXTYPE_2D ? GL_TEXTURE_2D : GL_TEXTURE_CUBE_MAP);
    tex->currMaxLevel = -1;
    tex->maxLevel = 0;
    tex->setFilter = 0;
    LR_AssertTrue(ctx, format > LRTEXFORMAT_INVALID && format < LRTEXFORMAT_COUNT);
    GLenum internalFormat = internalFormats[format], glFormat  = glFormats [format], glType = glTypes[format];
    LR_BindTexForModify(ctx, tex->target, tex->textureObj);
    if(type == LRTEXTYPE_2D) {
        if(glFormat == GL_NUM_COMPRESSED_TEXTURE_FORMATS) {
            int imageSize = CompressedSize(internalFormat, width, height);
            GL_CHECK(ctx, glCompressedTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, imageSize, 0));
        } else {
            GL_CHECK(ctx, glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, glFormat, glType, 0));
        }
    } else if (type == LRTEXTYPE_CUBE) {
        
    }
}

LREXPORT void LR_Texture_SetRectangle(LR_Context *ctx, LR_Texture *tex, int x, int y, int width, int height, void *data)
{
    LR_AssertTrue(ctx, tex->resident);
    LR_AssertTrue(ctx, tex->textureType == LRTEXTYPE_2D);
    LR_BindTexForModify(ctx, tex->target, tex->textureObj);
    if(tex->textureFormat == LRTEXFORMAT_R8) glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    GL_CHECK(ctx, glTexSubImage2D(GL_TEXTURE_2D, 0, x, y, width, height, glFormats[tex->textureFormat], glTypes[tex->textureFormat], data));
    if(tex->textureFormat == LRTEXFORMAT_R8) glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

LREXPORT int LR_Texture_GetWidth(LR_Context *ctx, LR_Texture *tex)
{
    if(!LR_Texture_EnsureLoaded(ctx, tex)) {
        LR_CriticalErrorFunc(ctx, "Texture not resident for LR_Texture_GetWidth");
        return 0;
    }
    return tex->width;
}

LREXPORT int LR_Texture_GetHeight(LR_Context *ctx, LR_Texture *tex)
{
    if(!LR_Texture_EnsureLoaded(ctx, tex)) {
        LR_CriticalErrorFunc(ctx, "Texture not resident for LR_Texture_GetHeight");
        return 0;
    }
    return tex->height;
}

int LR_Texture_EnsureLoaded(LR_Context *ctx, LR_Texture *tex)
{
    if(tex->resident) return 1;
    LR_ReloadTex(ctx, tex);
    return tex->resident;
}

void LR_Texture_SetFilter(LR_Context *ctx, LR_Texture *tex, LRTEXFILTER filter)
{
    LR_AssertTrue(ctx, tex->resident);
    int needSetLevel = tex->maxLevel != tex->currMaxLevel;
    if(needSetLevel) {
        LR_BindTexForModify(ctx, tex->target, tex->textureObj);
        glTexParameteri(tex->target, GL_TEXTURE_MAX_LEVEL, tex->maxLevel);
        tex->maxLevel = tex->currMaxLevel;
    }
    if(needSetLevel ||
        tex->setFilter != filter ||
        tex->setAnisotropy != ctx->anisotropy) {
        LR_BindTexForModify(ctx, tex->target, tex->textureObj);
        tex->setFilter = filter;
        GLenum minfilter;
        GLenum magfilter;
        if(ctx->maxAnisotropy) {
            glTexParameterf(tex->target, GL_TEXTURE_MAX_ANISOTROPY_EXT, (float)ctx->anisotropy);
        }
        if(filter == LRTEXFILTER_LINEAR) {
            if(tex->maxLevel > 0)
                minfilter = GL_LINEAR_MIPMAP_LINEAR;
            else
                minfilter = GL_LINEAR;
            magfilter = GL_LINEAR;
        } else if (filter == LRTEXFILTER_NEAREST) {
            minfilter = GL_NEAREST;
            magfilter = GL_NEAREST;
        } else {
            LR_CriticalErrorFunc(ctx, "Invalid LRTEXFILTER enumeration passed");
            return;
        }
        glTexParameteri(tex->target, GL_TEXTURE_MIN_FILTER, minfilter);
        glTexParameteri(tex->target, GL_TEXTURE_MAG_FILTER, magfilter);
    }
}