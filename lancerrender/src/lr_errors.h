#ifndef _LR_ERRORS_H_
#define _LR_ERRORS_H_
void LR_CriticalErrorFunc(LR_Context *ctx, const char *msg);
void LR_WarningFunc(LR_Context *ctx, const char *msg);
void LR_GLCheckError(LR_Context *ctx, const char *loc);
#define LX_STRINGIFY(x) #x
#define LX_TOSTRING(x) LX_STRINGIFY(x)

#define LR_AssertTrue(ctx,cond) \
do { if(!(cond)) \
        LR_CriticalErrorFunc((ctx), "Assertion Failure: " #cond " @ " __FILE__ ":" LX_TOSTRING(__LINE__)); } \
        while (0)
            
#define GL_CHECK(ctx, stmt) do { \
            stmt; \
            LR_GLCheckError((ctx),  #stmt " @ " __FILE__ ":" LX_TOSTRING(__LINE__)); \
} while (0)

#endif
