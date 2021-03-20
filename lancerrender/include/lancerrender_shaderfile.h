#ifndef _LANCERRENDER_SHADERFILE_H_
#define _LANCERRENDER_SHADERFILE_H_
#include <lancerrender.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <SDL2/SDL.h>

LREXPORT void LR_ShaderCollection_DefaultShadersFromFile(LR_Context *ctx, LR_ShaderCollection *col, SDL_RWops *rw);
LREXPORT void LR_ShaderCollection_AddShadersFromFile(LR_Context *ctx, LR_ShaderCollection *col, LR_VertexDeclaration *decl, SDL_RWops *rw);


#ifdef __cplusplus
}
#endif
#endif