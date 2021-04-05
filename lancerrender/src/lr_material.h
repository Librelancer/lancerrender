#ifndef _LR_MATERIAL_H_
#define _LR_MATERIAL_H_
#include <lancerrender.h>
#include "lr_context.h"
typedef struct INT_LR_Material_ INT_LR_Material_;

typedef struct LR_Material {
    uint32_t magic;
    int transparent;
    INT_LR_Material_ *pimpl;
} LR_Material;

void LR_Material_Prepare(LR_Context *ctx, LR_VertexDeclaration* decl, LR_DrawCommand *cmd);
int LR_Material_IsTransparent(LR_Context *ctx, LR_Handle material);

#endif