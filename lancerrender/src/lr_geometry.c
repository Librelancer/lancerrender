#include "lr_geometry.h"
#include <stdlib.h>
#include <string.h>
#include "lr_errors.h"
#include "lr_context.h"
#include "lr_fnv1a.h"

typedef struct LR_StreamingGeometry {
    //LR_Geometry members
    LRGTYPE type;
    LR_VertexDeclaration *decl;
    GLuint vao;
    //private here
    //vertex
    int vertSize;
    int vertStreaming;
    int stride;
    GLuint vertex_buffer;
    void* vertCpubuffer;
    //element
    GLuint element_buffer;
    void *idxcpubuffer;
    int idxSize;
    int idxStreaming;
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
LREXPORT LR_Geometry *LR_StreamingGeometry_Create(LR_Context *ctx, LR_VertexDeclaration *decl, int size, int idxSize)
{
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)malloc(sizeof(LR_StreamingGeometry));
    g->type = LRGTYPE_STREAMING;
    g->vertSize = size;
    g->vertStreaming = 0;
    g->idxStreaming = 0;
    g->stride = decl->stride;
    g->decl = decl;
    GL_CHECK(ctx, glGenVertexArrays(1, &g->vao));
    GL_CHECK(ctx, glGenBuffers(1, &g->vertex_buffer));
    LR_BindVAO(ctx, g->vao);
    glBindBuffer(GL_ARRAY_BUFFER, g->vertex_buffer);
    GL_CHECK(ctx, glBufferData(GL_ARRAY_BUFFER, size * g->stride, NULL, GL_STREAM_DRAW));
    ApplyVertexDeclaration(ctx, decl);
    g->vertCpubuffer = malloc(size * g->stride);
    if(idxSize > 0) {
        GL_CHECK(ctx, glGenBuffers(1, &g->element_buffer));
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->element_buffer);
        GL_CHECK(ctx, glBufferData(GL_ELEMENT_ARRAY_BUFFER, idxSize * 2, NULL, GL_STATIC_DRAW));
        g->idxcpubuffer = malloc(idxSize * sizeof(uint16_t));
    } else {
        g->element_buffer = 0;
    }
    return (LR_Geometry*)g;
}

LREXPORT void LR_StreamingGeometry_SetIndices(LR_Context *ctx, LR_Geometry *geo, uint16_t* indices, int size)
{
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //set buffer if doesn't exist
    LR_AssertTrue(ctx, !g->element_buffer);
    LR_BindVAO(ctx, g->vao);
    GL_CHECK(ctx, glGenBuffers(1, &g->element_buffer));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->element_buffer);
    GL_CHECK(ctx, glBufferData(GL_ELEMENT_ARRAY_BUFFER, size * sizeof(uint16_t), indices, GL_STATIC_DRAW));
}

LREXPORT void* LR_StreamingGeometry_Begin(LR_Context *ctx, LR_Geometry *geo)
{
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //stream
    LR_AssertTrue(ctx, !g->vertStreaming);
    g->vertStreaming = 1;
    return g->vertCpubuffer;
}

LREXPORT uint16_t* LR_StreamingGeometry_BeginIndices(LR_Context *ctx, LR_Geometry *geo)
{
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //stream
    LR_AssertTrue(ctx, !g->idxStreaming);
    g->idxStreaming = 1;
    return g->idxcpubuffer;
}

LREXPORT void* LR_StreamingGeometry_Resize(LR_Context *ctx, LR_Geometry *geo, int newSize)
{
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //resize buffer
    g->vertSize = newSize;
    if(g->vertStreaming) {
        g->vertCpubuffer = realloc(g->vertCpubuffer, newSize * g->stride);
    } else {
        free(g->vertCpubuffer);
        g->vertCpubuffer = malloc(newSize * g->stride);
    }
    glDeleteBuffers(1, &g->vertex_buffer);
    GL_CHECK(ctx, glGenBuffers(1, &g->vertex_buffer));
    LR_BindVAO(ctx, g->vao);
    glBindBuffer(GL_ARRAY_BUFFER, g->vertex_buffer);
    GL_CHECK(ctx, glBufferData(GL_ARRAY_BUFFER, newSize * g->stride, NULL, GL_STREAM_DRAW));
    ApplyVertexDeclaration(ctx, g->decl);
    if(g->vertStreaming) return g->vertCpubuffer;
    else return NULL;
}

LREXPORT uint16_t* LR_StreamingGeometry_ResizeIndices(LR_Context *ctx, LR_Geometry *geo, int newSize)
{
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //resize buffer
    g->idxSize = newSize;
    if(g->idxStreaming) {
        g->idxcpubuffer = realloc(g->idxcpubuffer, newSize * sizeof(uint16_t));
    } else {
        free(g->idxcpubuffer);
        g->idxcpubuffer = malloc(newSize * sizeof(uint16_t));
    }
    glDeleteBuffers(1, &g->element_buffer);
    GL_CHECK(ctx, glGenBuffers(1, &g->element_buffer));
    LR_BindVAO(ctx, g->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->vertex_buffer);
    GL_CHECK(ctx, glBufferData(GL_ELEMENT_ARRAY_BUFFER, newSize * sizeof(uint16_t), NULL, GL_STREAM_DRAW));
    if(g->idxStreaming) return g->idxcpubuffer;
    else return NULL;
}


LREXPORT void LR_StreamingGeometry_Finish(LR_Context *ctx, LR_Geometry *geo, int count)
{
    if(!count) return;
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //stream
    LR_AssertTrue(ctx, g->vertStreaming);
    g->vertStreaming = 0;
    glBindBuffer(GL_ARRAY_BUFFER, g->vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * g->stride, g->vertCpubuffer);
}

LREXPORT void LR_StreamingGeometry_FinishIndices(LR_Context *ctx, LR_Geometry *geo, int count)
{
    if(!count) return;
    //type check
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    //stream
    LR_AssertTrue(ctx, g->idxStreaming);
    g->idxStreaming = 0;
    LR_BindVAO(ctx, g->vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g->element_buffer);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, count * sizeof(uint16_t), g->idxcpubuffer);
}

LREXPORT void LR_StreamingGeometry_Destroy(LR_Context *ctx, LR_Geometry *geo)
{
    LR_AssertTrue(ctx, geo->type == LRGTYPE_STREAMING);
    LR_StreamingGeometry *g = (LR_StreamingGeometry*)geo;
    if(ctx->bound_vao == g->vao) LR_BindVAO(ctx, 0);
    glDeleteVertexArrays(1, &g->vao);
    glDeleteBuffers(1, &g->element_buffer);
    glDeleteBuffers(1, &g->vertex_buffer);
    free(g->vertCpubuffer);
    free(g->idxcpubuffer);
    free(geo);
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