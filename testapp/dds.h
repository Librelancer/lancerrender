#ifndef _DDS_H_
#define _DDS_H_
#include <lancerrender.h>
#include <SDL2/SDL.h>
typedef enum {
    DDSResult_OK = 0,
    DDSResult_BADMAGIC = -1,
    DDSResult_UNSUPPORTEDFORMAT = -2,
    DDSResult_UNEXPECTEDEOF = -3,
    DDSResult_UNSUPPORTEDTYPE = -4,
    DDSResult_UNSUPPORTEDPIXELFORMAT = -5,
} DDSResult;
DDSResult DDSLoad(LR_Context *ctx, LR_Texture *tex, SDL_RWops *rw);
#endif