#ifndef _LR_DYNAMICDRAW_H_
#define _LR_DYNAMICDRAW_H_
#include "lr_context.h"
void LR_DynamicDraw_Process(LR_Context *ctx, LR_DrawCommand *cmd);
void LR_DynamicDraw_Flush(LR_Context *ctx, LR_DynamicDraw *dd);
#endif