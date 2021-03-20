#include "lr_geometry.h"
#include <stdlib.h>
#include <string.h>
#include "lr_errors.h"
#include "lr_context.h"

typedef struct LR_StreamingGeometry {
    //LR_Geometry members
    LRGTYPE type;
    LR_VertexDeclaration *decl;
    GLuint vao;
    //private here
    int size;
    int streaming;
    int stride;
    GLuint vertex_buffer;
    GLuint element_buffer;
    void* cpubuffer;
} LR_StreamingGeometry;

typedef struct LR_StaticGeometry {
    //LR_Geometry members
    LRGTYPE type;
    LR_VertexDeclaration *decl;
    GLuint vao;
    //private here
    GLuint vertex_buffer;
    GLuint element_buffer;
    int vertex_size;
    int element_size;
    int vertex_offset;
    int element_offset;
} LR_StaticGeometry;


/* LR_VertexDeclaration */
#define LR_MAXVERTEXELEMENTS (16)
struct LR_VertexDeclaration {
    uint64_t hash;
    int stride;
    int elemCount;
    LR_VertexElement elements[LR_MAXVERTEXELEMENTS];
};

uint64_t LR_VertexDeclaration_GetHash(LR_VertexDeclaration *decl)
{
    return decl->hash;
}

static GLenum GetElementType(LRELEMENTTYPE type)
{
    switch(type) {
        case LRELEMENTTYPE_BYTE:
            return GL_UNSIGNED_BYTE;
        case LRELEMENTTYPE_USHORT:
            return GL_UNSIGNED_SHORT;
        default:
        case LRELEMENTTYPE_FLOAT:
            return GL_FLOAT;
    }
}

static void ApplyVertexDeclaration(LR_Context *ctx, LR_VertexDeclaration *decl)
{
    for(int i = 0; i < decl->elemCount; i++) {
        LR_VertexElement *e = &decl->elements[i];
        GL_CHECK(ctx, glEnableVertexAttribArray(e->slot));
        GL_CHECK(ctx, glVertexAttribPointer(
            (GLuint)e->slot, 
            e->elements,
            GetElementType(e->type),
            e->normalized,
            decl->stride,
            (void*)(intptr_t)e->offset
        ));
    }
}

#define FNV1_PRIME_32 16777619U
#define FNV1_OFFSET_32 2166136261U
static uint32_t fnv1a_32(const void *input, int len)
{
    const unsigned char *data = input;
    const unsigned char *end = data + len;
    uint32_t hash = FNV1_PRIME_32;
    for (; data != end; ++data)
    {
        hash ^= *data;
        hash *= FNV1_PRIME_32;
    }
    return hash;
}

LREXPORT LR_VertexDeclaration* LR_VertexDeclaration_Create(LR_Context *ctx, int stride, int elemCount, LR_VertexElement *elements)
{
    LR_AssertTrue(ctx, elemCount < LR_MAXVERTEXELEMENTS);
    LR_VertexDeclaration *decl = malloc(sizeof(LR_VertexDeclaration));
    decl->stride = stride;
    decl->elemCount = elemCount;
    memcpy(decl->elements, elements, elemCount * sizeof(LR_VertexElement));

    uint32_t elemHash = fnv1a_32(elements, elemCount * sizeof(LR_VertexElement));
    uint32_t props = (stride << 16) | (elemCount & 0xFFFF);
    decl->hash = ((uint64_t)props << 32) | (uint64_t) elemHash;

    return decl;
}
LREXPORT void LR_VertexDeclaration_Free(LR_Context *ctx, LR_VertexDeclaration *decl)
{
    free(decl);
}

/* LR_StreamingGeometry */
LREXPORT LR_Geometry *LR_StreamingGeometry_Create(LR_Context *ctx, LR_VertexDeclaration *decl, int size)
{
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)malloc(sizeof(LR_StreamingGeometry));
    g->type = LRGTYPE_STREAMING;
    g->size = size;
    g->streaming = 0;
    g->stride = decl->stride;
    g->decl = decl;
    GL_CHECK(ctx, glGenVertexArrays(1, &g->vao));
    GL_CHECK(ctx, glGenBuffers(1, &g->vertex_buffer));
    LR_BindVAO(ctx, g->vao);
    glBindBuffer(GL_ARRAY_BUFFER, g->vertex_buffer);
    GL_CHECK(ctx, glBufferData(GL_ARRAY_BUFFER, size * g->stride, NULL, GL_STREAM_DRAW));
    ApplyVertexDeclaration(ctx, decl);
    g->cpubuffer = malloc(size * g->stride);
    g->element_buffer = 0;
    return (LR_Geometry*)g;
}

LREXPORT void LR_StreamingGeometry_SetIndices(LR_Context *ctx, LR_Geometry *geo, unsigned short *indices, int size)
{
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //set buffer if doesn't exist
    LR_AssertTrue(ctx, !g->element_buffer);
    LR_BindVAO(ctx, g->vao);
    GL_CHECK(ctx, glGenBuffers(1, & g->element_buffer));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->element_buffer);
    GL_CHECK(ctx, glBufferData(GL_ELEMENT_ARRAY_BUFFER, size * 2, indices, GL_STATIC_DRAW));
}

LREXPORT void* LR_StreamingGeometry_Begin(LR_Context *ctx, LR_Geometry *geo)
{
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //stream
    LR_AssertTrue(ctx, !g->streaming);
    g->streaming = 1;
    return g->cpubuffer;
}

LREXPORT void LR_StreamingGeometry_Finish(LR_Context *ctx, LR_Geometry *geo, int count)
{
    if(!count) return;
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //stream
    LR_AssertTrue(ctx, g->streaming);
    g->streaming = 0;
    glBindBuffer(GL_ARRAY_BUFFER, g->vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * g->stride, g->cpubuffer);
}

#define STATIC_INITIAL_CAPACITY (512)
/* LR_StaticGeometry */
LREXPORT LR_Geometry *LR_StaticGeometry_Create(LR_Context *ctx, LR_VertexDeclaration *decl)
{
    LR_StaticGeometry *g = (LR_StaticGeometry*)malloc(sizeof(LR_StaticGeometry));
    g->type = LRGTYPE_STATIC;
    g->vertex_size = STATIC_INITIAL_CAPACITY * decl->stride;
    g->element_size = STATIC_INITIAL_CAPACITY * 2;
    g->decl = decl;
    g->vertex_offset = 0;
    g->element_offset = 0;
    GL_CHECK(ctx, glGenVertexArrays(1, &g->vao));
    GL_CHECK(ctx, glGenBuffers(1, &g->vertex_buffer));
    GL_CHECK(ctx, glGenBuffers(1, &g->element_buffer));
    LR_BindVAO(ctx, g->vao);
    glBindBuffer(GL_ARRAY_BUFFER, g->vertex_buffer);
    GL_CHECK(ctx, glBufferData(GL_ARRAY_BUFFER, g->vertex_size, NULL, GL_STATIC_DRAW));
    ApplyVertexDeclaration(ctx, decl);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->element_buffer);
    GL_CHECK(ctx, glBufferData(GL_ELEMENT_ARRAY_BUFFER, g->element_size, NULL, GL_STATIC_DRAW));
    return (LR_Geometry*)g;
}

static int EnsureBufferSize(LR_Context *ctx, GLuint *bufferID, int *currentSize, int required)
{
    if(*currentSize > required) return 0;
    GLuint newBuffer;
    int newSize = *currentSize;
    while(newSize <= required) {
        newSize *= 2;
    }
    glGenBuffers(1, &newBuffer);
    glBindBuffer(GL_COPY_READ_BUFFER, *bufferID);
    glBindBuffer(GL_COPY_WRITE_BUFFER, newBuffer);
    glBufferData(GL_COPY_WRITE_BUFFER, newSize, NULL, GL_STATIC_DRAW);
    GL_CHECK(ctx, glCopyBufferSubData(GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER, 0, 0, *currentSize));
    *currentSize = newSize;
    glDeleteBuffers(1, bufferID);
    *bufferID = newBuffer;
    return 1;
}

LREXPORT void LR_StaticGeometry_UploadVertices(LR_Context *ctx, LR_Geometry *geo, void *data, int size, int *out_baseVertex)
{
    *out_baseVertex = 0;
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STATIC);
    LR_StaticGeometry *g = (LR_StaticGeometry*)geo;
    //buffer re-allocated
    if(EnsureBufferSize(ctx, &g->vertex_buffer, &g->vertex_size, g->vertex_offset + (size * g->decl->stride))) {
        LR_BindVAO(ctx, g->vao);
        glBindBuffer(GL_ARRAY_BUFFER, g->vertex_buffer);
        ApplyVertexDeclaration(ctx, g->decl);
    }
    glBindBuffer(GL_ARRAY_BUFFER, g->vertex_buffer);
    *out_baseVertex = g->vertex_offset / g->decl->stride;
    GL_CHECK(ctx, glBufferSubData(GL_ARRAY_BUFFER, g->vertex_offset, size * g->decl->stride, data));
    g->vertex_offset += (size * g->decl->stride);
}

LREXPORT void LR_StaticGeometry_UploadIndices(LR_Context *ctx, LR_Geometry *geo, void *data, int size, int *out_startIndex)
{
    *out_startIndex = 0;
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STATIC);
    LR_StaticGeometry *g = (LR_StaticGeometry*)geo;
    if(EnsureBufferSize(ctx, &g->element_buffer, &g->element_size, g->element_size + size * 2)) {
        LR_BindVAO(ctx, g->vao);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->element_buffer);
    }
    glBindBuffer(GL_ARRAY_BUFFER, g->element_buffer); //don't overwrite element binding
    *out_startIndex = g->element_offset / 2;
    GL_CHECK(ctx, glBufferSubData(GL_ARRAY_BUFFER, g->element_offset, size * 2, data));
    g->element_offset += size * 2;
}