#ifndef _LR_GEOMETRY_H_
#define _LR_GEOMETRY_H_
#include <lancerrender.h>
#include <glad/glad.h>

typedef enum LRGTYPE {
    LRGTYPE_STREAMING = 16,
    LRGTYPE_STATIC = 32
} LRGTYPE;


/* DO NOT ALLOCATE THIS STRUCT
 * USE LR_StreamingGeometry or LR_StaticGeometry
 * This one is simply for type checks + buffer access
 */
struct LR_Geometry {
    LRGTYPE type;
    LR_VertexDeclaration *decl;
    GLuint vao;
};

/* LR_VertexDeclaration */
#define LR_MAXVERTEXELEMENTS (16)
struct LR_VertexDeclaration {
    uint64_t hash;
    int stride;
    int elemCount;
    LR_VertexElement elements[LR_MAXVERTEXELEMENTS];
};

#endif
