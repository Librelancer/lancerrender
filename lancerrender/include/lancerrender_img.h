#ifndef _LR_IMG_H_
#define _LR_IMG_H_
#include <lancerrender.h>
#include <SDL2/SDL.h>
typedef enum {
    LRDDSResult_OK = 0,
    LRDDSResult_INVALIDORCORRUPT = -1,
    LRDDSResult_UNSUPPORTEDFORMAT = -2,
    LRDDSResult_UNEXPECTEDEOF = -3,
    LRDDSResult_UNSUPPORTEDTYPE = -4,
    LRDDSResult_UNSUPPORTEDPIXELFORMAT = -5,
    LRDDSResult_TOOMANYLEVELS = -6
} LRDDSResult;

LRDDSResult LR_DDS_Load(LR_Context *ctx, LR_Texture *tex, SDL_RWops *rw);
#endif