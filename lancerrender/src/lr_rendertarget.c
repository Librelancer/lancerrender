#include "lr_rendertarget.h"
#include "lr_context.h"
#include "lr_texture.h"
#include <stdio.h>

static void _RenderTarget_GLBlit(
    LR_Context *ctx, 
    GLuint fbo0, 
    GLuint fbo1,
    int srcW,
    int srcH,
    int dstW, 
    int dstH
)
{
    GLuint cfbo = ctx->bound_fbo;
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo0);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo1);
    GL_CHECK(ctx, glBlitFramebuffer(0,0, srcW, srcH, 0, 0, dstW, dstH, GL_COLOR_BUFFER_BIT, GL_LINEAR));
    glBindFramebuffer(GL_FRAMEBUFFER, cfbo);
}

LREXPORT void LR_RenderTarget_BlitTargets(LR_Context *ctx, LR_RenderTarget *src, LR_RenderTarget *dst) 
{
    _RenderTarget_GLBlit(ctx, src->gl, dst->gl, src->width, src->height, dst->width, dst->height);
}

LREXPORT void LR_RenderTarget_BlitToScreen(LR_Context *ctx, LR_RenderTarget *src)
{
    _RenderTarget_GLBlit(ctx, src->gl, 0, src->width, src->height, ctx->viewports[ctx->viewportSP].width, ctx->viewports[ctx->viewportSP].height);
}

LREXPORT int LR_RenderTarget_GetWidth(LR_Context *ctx, LR_RenderTarget *rt)
{
    return rt->width;
}

LREXPORT int LR_RenderTarget_GetHeight(LR_Context *ctx, LR_RenderTarget *rt)
{
    return rt->height;
}

static void CheckFramebufferComplete(LR_Context *ctx) {
    const char *errMsg = "unknown framebuffer status error";
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if(status == GL_FRAMEBUFFER_COMPLETE) return;
    #define FBO_CASE(x) case x: \
    errMsg = "Framebuffer creation error: "#x; \
    break;
    switch(status) {
        FBO_CASE(GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT)
        FBO_CASE(GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT)
        FBO_CASE(GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER)
        FBO_CASE(GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER)
        FBO_CASE(GL_FRAMEBUFFER_UNSUPPORTED)
        FBO_CASE(GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE)
        FBO_CASE(GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS)
    }
    #undef FBO_CASE    
    LR_CriticalErrorFunc(ctx, errMsg);
}

LREXPORT LR_RenderTarget *LR_RenderTarget_FromTexture(LR_Context *ctx, LR_Texture *tex, int useDepth)
{
    LR_AssertTrue(ctx, tex);
    LR_AssertTrue(ctx, tex->resident);
    LR_RenderTarget *rt = malloc(sizeof(LR_RenderTarget));
    tex->inFbo = 1;
    GL_CHECK(ctx, glGenFramebuffers(1, &rt->gl));
    rt->texture = tex;
    rt->msaa = 0;
    rt->width = LR_Texture_GetWidth(ctx, tex);
    rt->height = LR_Texture_GetHeight(ctx, tex);
    GLuint cfbo = ctx->bound_fbo;
    //bind
    glBindFramebuffer(GL_FRAMEBUFFER, rt->gl);
    //depth target
    if(useDepth) {
        glGenRenderbuffers(1, &rt->renderBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, rt->renderBuffer);
        glRenderbufferStorage(
            GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, 
            rt->width, rt->height
        );
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->renderBuffer);
    } else {
        rt->renderBuffer = 0;
    }
    //bind color target
    LR_BindTexForModify(ctx, GL_TEXTURE_2D, tex->textureObj);
    GL_CHECK(ctx, glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex->textureObj, 0));
    //reset binding
    CheckFramebufferComplete(ctx);
    glBindFramebuffer(GL_FRAMEBUFFER, cfbo);
    return rt;
}

LREXPORT LR_Texture *LR_RenderTarget_GetTexture(LR_Context *ctx, LR_RenderTarget *rt)
{
    LR_AssertTrue(ctx, !rt->msaa);
    return rt->texture;
}

LREXPORT void LR_RenderTarget_DecoupleTexture(LR_Context *ctx, LR_RenderTarget *rt)
{
    LR_AssertTrue(ctx, !rt->msaa);
    rt->freeTex = 0;
}

LREXPORT LR_RenderTarget *LR_RenderTarget_CreateMultisample(LR_Context *ctx, int width, int height, int useDepth, int samples)
{
    LR_AssertTrue(ctx, samples % 2 == 0 && samples >= 2 && samples <= ctx->maxSamples);
    LR_RenderTarget *rt = malloc(sizeof(LR_RenderTarget));
    rt->msaa = 1;
    rt->freeTex = 1;
    rt->width = width;
    rt->height = height;
    rt->texture = NULL;
    GLuint cfbo = ctx->bound_fbo;
    GL_CHECK(ctx, glGenFramebuffers(1, &rt->gl));
    //bind
    glBindFramebuffer(GL_FRAMEBUFFER, rt->gl);
    //depth target
    if(useDepth) {
        glGenRenderbuffers(1, &rt->renderBuffer);
        glBindRenderbuffer(GL_RENDERBUFFER, rt->renderBuffer);
        GL_CHECK(ctx, glRenderbufferStorageMultisample(
            GL_RENDERBUFFER, samples,
            GL_DEPTH_COMPONENT24, 
            width,
            height        
        ));
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rt->renderBuffer);
    } else {
        rt->renderBuffer = 0;
    }
    //create/bind colour target
    glGenRenderbuffers(1, &rt->msBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, rt->msBuffer);
    GL_CHECK(ctx, glRenderbufferStorageMultisample(
        GL_RENDERBUFFER, samples,
        GL_RGBA, 
        width,
        height        
    ));
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rt->msBuffer);
    //reset binding
    CheckFramebufferComplete(ctx);
    glBindFramebuffer(GL_FRAMEBUFFER, cfbo);
    return rt;
}

LREXPORT LR_RenderTarget *LR_RenderTarget_Create(LR_Context *ctx, int width, int height, LRTEXFORMAT colorFormat, int useDepth)
{
    LR_Texture *colorTex = LR_Texture_Create(ctx, 0);
    LR_Texture_Allocate(ctx, colorTex, LRTEXTYPE_2D, colorFormat, width, height);
    LR_RenderTarget *rt = LR_RenderTarget_FromTexture(ctx, colorTex, useDepth);
    rt->freeTex = 1;
    return rt;
}

LREXPORT void LR_RenderTarget_Destroy(LR_Context *ctx, LR_RenderTarget *rt)
{
    glDeleteFramebuffers(1, &rt->gl);
    if(!rt->texture) {
        glDeleteRenderbuffers(1, &rt->msBuffer);
    } else if(rt->freeTex) {
        LR_Texture_Destroy(ctx, rt->texture);
    }
    if(rt->renderBuffer) {
        glDeleteRenderbuffers(1, &rt->renderBuffer);
    }
    free(rt);
}
