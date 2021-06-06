#include "lr_ubo.h"
#include <stddef.h>
#include "lr_context.h"
#include "lr_errors.h"

static inline int AlignedIndex(int input, int stride, int align)
{
    if(align == 0) return input;
    int offset = input * stride;
    int aOffset = (offset + (align - 1)) & ~(align - 1);
    return aOffset / stride;
}

LREXPORT LR_UniformBuffer *LR_UniformBuffer_Create(LR_Context *ctx, int size, int stride)
{
    LR_AssertTrue(ctx, stride % 16 == 0);
    LR_UniformBuffer *ubo = malloc(sizeof(LR_UniformBuffer));
    glGenBuffers(1, &ubo->gl);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo->gl);
    glBufferData(GL_UNIFORM_BUFFER, size * stride, NULL, GL_STREAM_DRAW);
    int lval = stride % ctx->uboOffsetAlign;
    ubo->align = lval ? ubo->align : 0;
    if(ubo->align < stride && lval != 0) //shouldn't happen
        LR_CriticalErrorFunc(ctx, "UBO alignment error");
    ubo->size = size;
    ubo->stride = stride;
    return ubo;
}

LREXPORT int LR_UniformBuffer_AlignIndex(LR_Context *ctx, LR_UniformBuffer *ubo, int index)
{
    return AlignedIndex(index, ubo->stride, ubo->align);
}

LREXPORT void LR_UniformBuffer_SetData(LR_Context *ctx, LR_UniformBuffer *ubo, void* ptr, int stride, int start, int len)
{
    LR_AssertTrue(ctx, stride == ubo->stride);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo->gl);
    glBufferSubData(GL_UNIFORM_BUFFER, start * stride, len * stride, ptr);
}

LREXPORT void LR_UniformBuffer_Destroy(LR_Context *ctx, LR_UniformBuffer *ubo)
{
    glDeleteBuffers(1, &ubo->gl);
    free(ubo);
}