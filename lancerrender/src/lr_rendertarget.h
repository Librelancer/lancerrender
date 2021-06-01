#ifndef _LR_RENDERTARGET_H_
#define _LR_RENDERTARGET_H_

#include <lancerrender.h>
#include <glad/glad.h>

struct LR_RenderTarget {
    GLuint gl; //framebuffer obj
    GLuint renderBuffer; //depth
    GLuint msBuffer; //multisample buffer
    LR_Texture *texture;
    int freeTex;
    int msaa;
    int width;
    int height;
};

#endif