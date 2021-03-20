#include "lr_blockalloc.h"
#include <stdlib.h>
#define ITEM_INITIAL (32)

typedef struct Block Block;
struct Block {
    LR_Handle next;
};

#define BLOCK_PTR(x,y) (Block*)(&((uint8_t*)(x)->data)[(y)])

struct BlockAlloc {
    LR_Handle block_head;
    void *data;
    int dataSize;
    int limit;
    int allocOffset;
    int sizeOfObject;
    bafail failreason;
};

BlockAlloc *blockalloc_Init(int sizeOfObject, int maxAddress)
{
    BlockAlloc *block = malloc(sizeof(BlockAlloc));
    block->sizeOfObject = sizeOfObject;
    block->dataSize = 8 + (sizeOfObject * ITEM_INITIAL);
    block->data = malloc(8 + (sizeOfObject * ITEM_INITIAL));
    block->limit = maxAddress;
    block->block_head = 0;
    block->allocOffset = 8; //aligned non-zero start
    block->failreason = bafail_noerror;
}

LR_Handle blockalloc_Alloc(BlockAlloc *block)
{
    if(!block->data) {
        block->failreason = bafail_realloc;
        return 0; //error
    }
    if(block->block_head) {
        Block *ref = BLOCK_PTR(block, block->block_head);
        LR_Handle tmp = block->block_head;
        block->block_head = ref->next;
        return tmp;
    }
    if(block->allocOffset + block->sizeOfObject >= block->limit) {
        block->failreason = bafail_address;
        return 0; //Error allocating
    }
    if(block->allocOffset + block->sizeOfObject > block->dataSize) {
        block->data = realloc(block->data, block->dataSize * 2);
        if(!block->data) {
            block->failreason = bafail_realloc;
            return 0;
        }
        block->dataSize *= 2;
    }
    LR_Handle retval = block->allocOffset;
    block->allocOffset += block->sizeOfObject;
    return retval;
}

bafail blockalloc_FailReason(BlockAlloc *block)
{
    return block->failreason;
}

void *blockalloc_HandleToPtr(BlockAlloc *block, LR_Handle handle)
{
     if(handle < 1 || handle > (block->dataSize - block->sizeOfObject))
        return NULL; //Invalid pointer
    return (void*)(&((uint8_t*)block->data)[handle]);
}

int blockalloc_Free(BlockAlloc *block, LR_Handle handle)
{
    if(handle < 1 || handle > (block->dataSize - block->sizeOfObject))
        return 0; //Invalid pointer
    LR_Handle blk = block->block_head;
    while(blk) {
        if(blk == handle) return 0; //Double free
        Block *c = BLOCK_PTR(block, blk);
        blk = c->next;
    }
    Block *new = BLOCK_PTR(block, handle);
    LR_Handle tmp = block->block_head;
    block->block_head = handle;
    new->next = tmp;
    return 1;
}

void blockalloc_Destroy(BlockAlloc *block)
{
    free(block->data);
    free(block);
}