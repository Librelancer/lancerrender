#ifndef _LR_UBO_H_
#define _LR_UBO_H_
#include <lancerrender.h>
#include <glad/glad.h>

struct LR_UniformBuffer {
    GLuint gl;
    int stride;
    int size;
    int align;
};
#endif