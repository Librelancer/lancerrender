#ifndef _LANCERRENDER_H_
#define _LANCERRENDER_H_

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef LR_BUILDING_DLL
#define LREXPORT __declspec(dllexport)
#else
#define LREXPORT __declspec(dllimport)
#endif
#else
#define LREXPORT
#endif

#include <stdint.h>
/* ENUMERATIONS */
typedef enum LRBLEND {
    LRBLEND_ZERO             = 1,
    LRBLEND_ONE              = 2,
    LRBLEND_SRCCOLOR         = 3,
    LRBLEND_INVSRCCOLOR      = 4,
    LRBLEND_SRCALPHA         = 5,
    LRBLEND_INVSRCALPHA      = 6,
    LRBLEND_DESTALPHA        = 7,
    LRBLEND_INVDESTALPHA     = 8,
    LRBLEND_DESTCOLOR        = 9,
    LRBLEND_INVDESTCOLOR     = 10,
    LRBLEND_SRCALPHASAT      = 11
} LRBLEND; /* Matches D3DBLEND, good for Librelancer */

typedef enum LRCULL {
    LRCULL_NONE,
    LRCULL_CW,
    LRCULL_CCW
} LRCULL;

typedef enum LRERRORTYPE {
    LRERRORTYPE_WARN,
    LRERRORTYPE_CRITICAL
} LRERRORTYPE;

typedef enum LRPRIMTYPE {
    LRPRIMTYPE_TRIANGLELIST,
    LRPRIMTYPE_TRIANGLESTRIP,
    LRPRIMTYPE_LINELIST
} LRPRIMTYPE;

typedef enum LRELEMENTTYPE {
    LRELEMENTTYPE_FLOAT,
    LRELEMENTTYPE_USHORT,
    LRELEMENTTYPE_BYTE
} LRELEMENTTYPE;

typedef enum LRELEMENTSLOT {
    LRELEMENTSLOT_POSITION = 0,
    LRELEMENTSLOT_COLOR = 1,
    LRELEMENTSLOT_NORMAL = 2,
    LRELEMENTSLOT_TEXTURE1 = 3,
    LRELEMENTSLOT_TEXTURE2 = 4,
    LRELEMENTSLOT_DIMENSIONS = 5,
    LRELEMENTSLOT_RIGHT = 6,
    LRELEMENTSLOT_UP = 7,
    LRELEMENTSLOT_BONEIDS = 8,
    LRELEMENTSLOT_BONEWEIGHTS = 9,
    LRELEMENTSLOT_COLOR2 = 10
} LRELEMENTSLOT;

typedef enum LRTEXTYPE {
    LRTEXTYPE_2D,
    LRTEXTYPE_CUBE
} LRTEXTYPE;

typedef enum LRTEXFORMAT {
    LRTEXFORMAT_INVALID = 0,
    LRTEXFORMAT_COLOR = 1,
    LRTEXFORMAT_BGR565 = 2,
    LRTEXFORMAT_BGRA5551 = 3,
    LRTEXFORMAT_BGRA4444 = 4,
    LRTEXFORMAT_DXT1 = 5,
    LRTEXFORMAT_DXT3 = 6,
    LRTEXFORMAT_DXT5 = 7,
    LRTEXFORMAT_R8 = 8,
    LRTEXFORMAT_DEPTH = 9,
    LRTEXFORMAT_COUNT //count
} LRTEXFORMAT;

typedef enum LRTEXFILTER {
    LRTEXFILTER_INVALID,
    LRTEXFILTER_NEAREST,
    LRTEXFILTER_LINEAR
} LRTEXFILTER;

typedef struct {
    float m[16];
} LR_Matrix4x4;

/* TYPES */
typedef unsigned int LR_Handle;
typedef struct LR_Context LR_Context;
typedef struct LR_Geometry LR_Geometry;
typedef struct LR_VertexDeclaration LR_VertexDeclaration;
typedef struct LR_Shader LR_Shader;
typedef struct LR_ShaderCollection LR_ShaderCollection;
typedef struct LR_Material LR_Material;
typedef struct LR_Texture LR_Texture;

typedef struct LR_VertexElement {
    LRELEMENTSLOT slot;
    int elements;
    LRELEMENTTYPE type;
    int normalized;
    int offset;
} LR_VertexElement;

#define LR_RGBA(r,g,b,a) (uint32_t)( \
    ((uint32_t)a & 0xFF) << 24 | \
    ((uint32_t)b & 0xFF) << 16 | \
    ((uint32_t)g & 0xFF) << 8 | \
    ((uint32_t)r & 0xFF) \
) 

typedef void (*LR_ErrorCallback)(LRERRORTYPE, const char*);
typedef void (*LR_TexLoadCallback)(LR_Context*, LR_Texture*);

/* FUNCTIONS */
LREXPORT LR_Context *LR_Init(int gles);
LREXPORT void LR_SetErrorCallback(LR_Context *ctx, LR_ErrorCallback cb);
LREXPORT void LR_Destroy(LR_Context *ctx);

LREXPORT LR_VertexDeclaration* LR_VertexDeclaration_Create(LR_Context *ctx, int stride, int elemCount, LR_VertexElement *elements);
LREXPORT void LR_VertexDeclaration_Free(LR_Context *ctx, LR_VertexDeclaration *decl);

LREXPORT LR_Geometry *LR_StreamingGeometry_Create(LR_Context *ctx, LR_VertexDeclaration *decl, int size);
LREXPORT void LR_StreamingGeometry_SetIndices(LR_Context *ctx, LR_Geometry *geo, unsigned short *indices, int size);
LREXPORT void* LR_StreamingGeometry_Begin(LR_Context *ctx, LR_Geometry *geo);
LREXPORT void LR_StreamingGeometry_Finish(LR_Context *ctx, LR_Geometry *geo, int count);

LREXPORT LR_Geometry *LR_StaticGeometry_Create(LR_Context *ctx, LR_VertexDeclaration *decl);
LREXPORT void LR_StaticGeometry_UploadVertices(LR_Context *ctx, LR_Geometry *geo, void *data, int size, int *out_baseVertex);
LREXPORT void LR_StaticGeometry_UploadIndices(LR_Context *ctx, LR_Geometry *geo, void *data, int size, int *out_startIndex);

/*
*  Called when a texture that has been unloaded is used.
*/
LREXPORT void LR_Texture_SetLoadCallback(LR_Context *ctx, LR_TexLoadCallback cb);
/*
 * Creates a texture object. Does not create a texture object/upload data
 */
LREXPORT LR_Texture* LR_Texture_Create(LR_Context *ctx, uint64_t tag);
LREXPORT uint64_t LR_Texture_GetTag(LR_Context *ctx, LR_Texture *tex);
LREXPORT int LR_Texture_GetWidth(LR_Context *ctx, LR_Texture *tex);
LREXPORT int LR_Texture_GetHeight(LR_Context *ctx, LR_Texture *tex);
/*
 * Allocates texture memory on the GPU for Level 0.
 */
LREXPORT void LR_Texture_Allocate(LR_Context *ctx, LR_Texture *tex, LRTEXTYPE type, LRTEXFORMAT format, int width, int height);
LREXPORT void LR_Texture_SetRectangle(LR_Context *ctx, LR_Texture *tex, int x, int y, int width, int height, void *data);
/* 
 * Clears the GPU memory used by the texture
 * If it is used again, the callback set by LR_Texture_SetLoadCallback will be called with the texture
 */
LREXPORT void LR_Texture_Unload(LR_Context *ctx, LR_Texture *tex);
/*
 * Destroys the LR_Texture object, unloads from GPU memory if needed.
 * Care must be taken to remove this texture from any materials before destroying
 * or you will get a segmentation fault
 */
LREXPORT void LR_Texture_Destroy(LR_Context *ctx, LR_Texture *tex);

LREXPORT LR_Shader *LR_Shader_Create(LR_Context *ctx, const char *vertex_source, const char *fragment_source);
LREXPORT LR_ShaderCollection* LR_ShaderCollection_Create(LR_Context *ctx);
LREXPORT void LR_ShaderCollection_AddDefaultShader(LR_Context *ctx, LR_ShaderCollection *col, int caps, LR_Shader *shader);
LREXPORT void LR_ShaderCollection_AddShaderByVertex(LR_Context *ctx, LR_ShaderCollection *col, LR_VertexDeclaration *decl, int caps, LR_Shader *shader);
LREXPORT void LR_ShaderCollection_Destroy(LR_Context *ctx, LR_ShaderCollection *col);
/* Materials */
LREXPORT LR_Handle LR_Material_Create(LR_Context *ctx);
LREXPORT void LR_Material_SetBlendMode(LR_Context *ctx, LR_Handle material, int blendEnabled, LRBLEND srcblend, LRBLEND destblend);
LREXPORT void LR_Material_SetShaders(LR_Context *ctx, LR_Handle material, LR_ShaderCollection *collection);

/* Frame */
LREXPORT void LR_BeginFrame(LR_Context *ctx, int width, int height);
LREXPORT void LR_EndFrame(LR_Context *ctx);
/* Clears */
LREXPORT void LR_ClearAll(LR_Context *ctx, float red, float green, float blue, float alpha);
LREXPORT void LR_ClearDepth(LR_Context *ctx);
/* Draw State */
LREXPORT void LR_PushViewport(LR_Context *ctx, int x, int y, int width, int height);
LREXPORT void LR_PopViewport(LR_Context *ctx);
LREXPORT void LR_SetCamera(
    LR_Context *ctx, 
    LR_Matrix4x4 *view, 
    LR_Matrix4x4 *projection,
    LR_Matrix4x4 *viewprojection
);
LREXPORT void LR_Scissor(LR_Context *ctx, int x, int y, int width, int height);
LREXPORT void LR_ClearScissor(LR_Context *ctx);
/* Generic Drawing */
LREXPORT LR_Handle LR_AllocTransform(LR_Context *ctx, LR_Matrix4x4 *world, LR_Matrix4x4 *normal);
LREXPORT void LR_Draw(
    LR_Context *ctx,
    LR_Handle material,
    LR_Geometry *geometry,
    LR_Handle transform,
    LRPRIMTYPE primitive,
    int baseVertex,
    int startIndex,
    int indexCount
);
/* 2D Drawing */
LREXPORT void LR_2D_FillRectangle(LR_Context *ctx, int x, int y, int width, int height, uint32_t color);
LREXPORT void LR_2D_FillRectangleColors(LR_Context *ctx, int x, int y, int width, int height, uint32_t tl, uint32_t tr, uint32_t bl, uint32_t br);
LREXPORT void LR_2D_DrawImage(LR_Context *ctx, LR_Texture *tex, int x, int y, int width, int height, uint32_t color);
LREXPORT void LR_2D_DrawLine(LR_Context *ctx, float x1, float y1, float x2, float y2, uint32_t color);
#ifdef __cplusplus
}
#endif

#endif