#ifndef _LR_VECTOR_H_
#define _LR_VECTOR_H_
/*
* Provides simple implementation of a growing buffer based on malloc/realloc
*/
#include "lr_errors.h"
#include <stdlib.h>

typedef struct LR_Vector {
    void *ptr;
    int currIdx;
    int size;
    int szOf;
} LR_Vector;


#define LRVEC_TYPECHECK(ctx,vec,type) LR_AssertTrue(ctx, (vec)->szOf == sizeof(type))

#define LRVEC_ADD(ctx,vec,type,count) do { \
    LRVEC_TYPECHECK(ctx,vec,type); \
    if((vec)->currIdx + (count) >= (vec)->size) { \
        (vec)->size *= 2; \
        (vec)->ptr = realloc((vec)->ptr, (vec)->size * (vec)->szOf); \
    } \
    (vec)->currIdx += (count); \
} while (0)

#define LRVEC_IDX(vec,type,idx) ((type*)((vec)->ptr))[(idx)]

#define LRVEC_ADD_VAL(ctx,vec,type,val) do { \
    int __vIdx = (vec)->currIdx; \
    LRVEC_ADD(ctx,vec,type,1); \
    LRVEC_IDX(vec,type,__vIdx) = (val); \
} while (0)


#define LRVEC_INIT(vec,type,initialSize) do { \
    (vec)->currIdx = 0; \
    (vec)->ptr = malloc((initialSize) * sizeof(type)); \
    (vec)->size = (initialSize); \
    (vec)->szOf = sizeof(type); \
} while (0)

#define LRVEC_FREE(ctx,vec,type) do { \
     LRVEC_TYPECHECK(ctx,vec,type); \
     free((vec)->ptr); \
} while (0)

#endif
