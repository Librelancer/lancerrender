/* lr_sort.c is here in case a replacement for qsort is deemed necessary */
#include "lr_context.h"
#include <stdlib.h>

static int LR_CmpCommand(const void *a, const void *b)
{
    uint64_t k1 = ((LR_DrawCommand*)a)->key;
    uint64_t k2 = ((LR_DrawCommand*)b)->key;
    /* invert check so largest Z comes earliest */
    if (k1 > k2) return -1; 
    if(k1 < k2) return 1;
    return 0;
}

void LR_CmdSort(LR_Context *ctx)
{
    if(ctx->commandPtr <= 1) return;
    qsort(ctx->commands, ctx->commandPtr, sizeof(LR_DrawCommand), LR_CmpCommand);
}
