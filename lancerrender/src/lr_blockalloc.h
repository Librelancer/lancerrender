#ifndef _BLOCKALLOC_H_
#define _BLOCKALLOC_H_
#include <lancerrender.h>
typedef struct BlockAlloc BlockAlloc;

typedef enum {
    bafail_noerror,
    bafail_address,
    bafail_realloc
} bafail;

BlockAlloc* blockalloc_Init(int sizeOfObject, int maxAddress);
LR_Handle blockalloc_Alloc(BlockAlloc *block);
void* blockalloc_HandleToPtr(BlockAlloc *block, LR_Handle handle);
bafail blockalloc_FailReason(BlockAlloc *block);
int blockalloc_Free(BlockAlloc *block, LR_Handle handle);
void blockalloc_Destroy(BlockAlloc *block);
#endif