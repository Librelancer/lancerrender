#ifndef _LR_2D_H_
#define _LR_2D_H_

typedef struct LR_2D LR_2D;
LR_2D *LR_2D_Init();
void LR_Flush2D(LR_Context *ctx);
void LR_2D_Destroy(LR_Context *ctx, LR_2D *r2d);

#endif