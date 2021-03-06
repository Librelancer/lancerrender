#include <SDL.h>
#include <lancerrender.h>
#include <stdio.h>
#include <math.h>
#include <cglm/cglm.h>  
#include "stb_image.h"
#include <lancerrender_shaderfile.h>
#include <lancerrender_img.h>
#define SUZANNE_H_DEFINE_DATA
#include "suzanne.h"

static SDL_Window* window;
static int gles = 0;
static int useMsaa = 0;

static void Sample_Init();
static void Sample_Loop();
static void Sample_Exit();

static int ltToggle = 1;


int main(int argc, char **argv)
{
    SDL_Init(SDL_INIT_VIDEO);
    window = SDL_CreateWindow( 
        "LancerRender Test", 
        SDL_WINDOWPOS_UNDEFINED, 
        SDL_WINDOWPOS_UNDEFINED, 
        640, 480, 
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );
    for(int i = 1; i < argc; i++) {
        if(!strcmp(argv[i], "gles")) {
            printf("Using GLES 3.1 context\n");
            gles = 1;
        }
        if(!strcmp(argv[i], "2x")) {
            printf("Using 2x MSAA\n");
            useMsaa = 2;
        }
        if(!strcmp(argv[i], "4x")) {
            printf("Using 4x MSAA\n");
            useMsaa = 4;
        }
    }

    /* create RGBA window */
    /* required for MSAA resolves to work correctly on GLES */
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 8);
    
    if(gles) {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    } else {
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    }

    SDL_GL_CreateContext(window);
    Sample_Init();
    int quit = 0;
    SDL_Event e;
    while(!quit) {
        while( SDL_PollEvent( &e ) != 0 )
        {
            //User requests quit
            if( e.type == SDL_QUIT )
            {
                quit = 1;
            }
            if(e.type == SDL_KEYDOWN) {
                if(e.key.keysym.scancode == SDL_SCANCODE_SPACE) {
                    ltToggle = !ltToggle;
                }
            }
        }
        Sample_Loop();
        SDL_GL_SwapWindow(window);
    }
    Sample_Exit();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}

LR_Context *lrctx;
LR_Texture *texture;
LR_Texture *monkey;
LR_Texture *alphaPng;
LR_Texture *alphaBgra;
LR_Texture *dxt5tex;
LR_RenderTarget *rt;
LR_RenderTarget *msaa;

LR_Handle billboardMaterial;
LR_DynamicDraw *billboards;

LR_Handle mat;
LR_Geometry *geom;
LR_VertexDeclaration *decl;

int baseVertex;
int startIndex;

typedef struct {
	float lightEnabled; //vec4 1
	float ambientR;
	float ambientG;
	float ambientB;
	float lightR; //vec4 2
	float lightG;
	float lightB;
	float lightX;
	float lightY; //vec4 3
	float lightZ;
} Lighting;

Lighting ltMonkey;

int pathEndsSlash = 0;
char *basePath = "./";

char *ResolvePath(const char *path)
{
    char path2[1024];
    if(pathEndsSlash) {
        snprintf(path2, 1023, "%s%s", basePath, path);
    } else {
        snprintf(path2, 1023, "%s/%s", basePath, path);
    }
    return SDL_strdup(path2);
}

static LR_Texture *LoadFileStb(const char *path)
{
    char *path2 = ResolvePath(path);
    LR_Texture *tx = LR_Texture_Create(lrctx, 0);
    int x,y,n;
    unsigned char *data = stbi_load(path2, &x, &y, &n, 4);
    SDL_free(path2);
    //swap channels
    for(int i = 0; i < x * y; i++) {
        unsigned char *ptr = &data[i * 4];
        unsigned char tmp = ptr[0];
        ptr[0] = ptr[2];
        ptr[2] = tmp;
    }
    LR_Texture_Allocate(lrctx, tx, LRTEXTYPE_2D, LRTEXFORMAT_BGRA8888, x, y);
    LR_Texture_SetRectangle(lrctx, tx, 0, 0, x, y, data);
    stbi_image_free(data);
    return tx;
}

static LR_Texture *LoadFileBgra5551(const char *path)
{
    char *path2 = ResolvePath(path);
    LR_Texture *tx = LR_Texture_Create(lrctx, 0);
    FILE *f = fopen(path2, "rb");
    SDL_free(path2);
    int x, y;
    fread(&x, sizeof(int), 1, f);
    fread(&y, sizeof(int), 1, f);
    void *data = malloc(x * y * 2);
    fread(data, 2, x * y, f);
    fclose(f);
    LR_Texture_Allocate(lrctx, tx, LRTEXTYPE_2D, LRTEXFORMAT_ARGB1555, x, y);
    LR_Texture_SetRectangle(lrctx, tx, 0, 0, x, y, data);
    free(data);
    return tx;
}

static LR_Texture *LoadFileDDS(const char *path)
{
    char *path2 = ResolvePath(path);
    LR_Texture *tx = LR_Texture_Create(lrctx, 0);
    SDL_RWops *file = SDL_RWFromFile(path2, "rb");
    free(path2);
    LRDDSResult result = LR_DDS_Load(lrctx, tx, file);
    if(result < 0) {
        printf("Error loading DDS file %s (%d)\n", path, result);
        abort();
    }
    SDL_RWclose(file);
    return tx;
}

typedef struct {
    float eee;
    float pad2; //must be 16 bytes
    float pad3;
    float pad4;
} BubbleParams;

LR_UniformBuffer *ubo;

static void Sample_Init()
{
    /* base path */
    basePath = SDL_GetBasePath();
    if(!basePath) basePath = SDL_strdup("./");
    {
        int ilen = strlen(basePath);
        pathEndsSlash = (basePath[ilen - 1] == '/' || basePath[ilen - 1] == '\\');
    }
    /* init */
    lrctx = LR_Init(gles);
    printf("Renderer: %s\n", LR_GetString(lrctx, LRSTRING_APIRENDERER));
    printf("Api Version: %s\n", LR_GetString(lrctx, LRSTRING_APIVERSION));
    printf("Max Anisotropy: %d\n", LR_GetMaxAnisotropy(lrctx));
    printf("Max Samples: %d\n", LR_GetMaxSamples(lrctx));
    LR_ContextFlags flags;
    LR_GetContextFlags(lrctx, &flags);
    for(int i = 0; i < flags.nflags; i++) {
        printf("Renderer Flag: %s\n", flags.flags[i]);
    }
    texture = LoadFileStb("texture.png");
    monkey = LoadFileStb("monkey.png");
    alphaPng = LoadFileStb("alphatex.png");
    alphaBgra = LoadFileBgra5551("alphatex.bgra5551");
    dxt5tex = LoadFileDDS("dxt5.dds");
    msaa = NULL;
    rt = LR_RenderTarget_Create(lrctx, 128, 128, LRTEXFORMAT_BGRA8888, 1);
    LR_ShaderCollection *sh = LR_ShaderCollection_Create(lrctx);
    char *monkeyPath = ResolvePath("monkeyshader.shader");
    SDL_RWops* file = SDL_RWFromFile(monkeyPath, "rb");
    SDL_free(monkeyPath);
    LR_ShaderCollection_DefaultShadersFromFile(lrctx, sh, file);
    SDL_RWclose(file);

    mat = LR_Material_Create(lrctx);
    LR_Material_SetShaders(lrctx, mat, sh); //shaders
    LR_Material_SetBlendMode(lrctx, mat, 0, 0, 0); //opaque
    //texture
    LR_Material_SetSamplerName(lrctx, mat, 0, "texsampler");
    LR_Material_SetSamplerTex(lrctx, mat, 0, monkey);
    //lighting params (std140 layout)
    Lighting lt = {
       .lightEnabled = 1,
       .ambientR = 0.227, .ambientG = 0.1686, .ambientB = 0.271,
       .lightR = 1.0, .lightG = 1.0, .lightB = 1.0,
       .lightX = 0.0 , .lightY = 0.0, .lightZ = 1.0
    };
    ltMonkey = lt;
    LR_VertexElement elements[] = {
        { .slot = LRELEMENTSLOT_POSITION, .type = LRELEMENTTYPE_FLOAT, .elements = 3, .normalized = 0, .offset = 0 },
        { .slot = LRELEMENTSLOT_NORMAL, .type = LRELEMENTTYPE_FLOAT, .elements = 3, .normalized = 0, .offset = 3 * sizeof(float) },
        { .slot = LRELEMENTSLOT_TEXTURE1, .type = LRELEMENTTYPE_FLOAT, .elements = 2, .normalized = 0, .offset = 6 * sizeof(float) }
    };
    decl = LR_VertexDeclaration_Create(lrctx, 8 * sizeof(float), 3, elements);
    geom = LR_StaticGeometry_Create(lrctx, decl);
    LR_StaticGeometry_UploadVertices(lrctx, geom, suzanneVertices, suzanneVertexCount, &baseVertex);
    LR_StaticGeometry_UploadIndices(lrctx, geom, suzanneIndices, suzanneIndexCount, &startIndex);


    LR_ShaderCollection *sh2 = LR_ShaderCollection_Create(lrctx);
    char *billboardPath = ResolvePath("billboard.shader");
    SDL_RWops* file2 = SDL_RWFromFile(billboardPath, "rb");
    SDL_free(billboardPath);
    LR_ShaderCollection_DefaultShadersFromFile(lrctx, sh2, file2);
    SDL_RWclose(file2);

    billboardMaterial = LR_Material_Create(lrctx);
    LR_Material_SetShaders(lrctx, billboardMaterial, sh2);
    LR_Material_SetBlendMode(lrctx, billboardMaterial, 1, LRBLEND_SRCALPHA, LRBLEND_INVSRCALPHA);
    LR_Material_SetSamplerName(lrctx, billboardMaterial, 1, "tex0");
    LR_Material_SetCull(lrctx, billboardMaterial, LRCULL_NONE);

    LR_VertexElement bbElem[] = {
        { .slot = LRELEMENTSLOT_POSITION, .type = LRELEMENTTYPE_FLOAT, .elements = 3, .normalized = 0, .offset = 0 },
        { .slot = LRELEMENTSLOT_DIMENSIONS, .type = LRELEMENTTYPE_FLOAT, .elements = 3, .normalized = 0, .offset = 3 * sizeof(float) },
        { .slot = LRELEMENTSLOT_TEXTURE1, .type = LRELEMENTTYPE_FLOAT, .elements = 2, .normalized = 0, .offset = 6 * sizeof(float) },
        { .slot = LRELEMENTSLOT_COLOR, .type = LRELEMENTTYPE_BYTE, .elements = 4, .normalized = 1, .offset = 8 * sizeof(float) }
    };

    LR_VertexDeclaration *bdecl = LR_VertexDeclaration_Create(lrctx, 9 * sizeof(float), 4, bbElem);

    uint16_t idxTemplate[] = { 
        0, 1, 2, 1, 3, 2
    };

    billboards = LR_DynamicDraw_Create(lrctx, bdecl, billboardMaterial, 4, 6, idxTemplate);
    LR_DynamicDraw_SetSamplerIndex(lrctx, billboards, 1);

    ubo = LR_UniformBuffer_Create(lrctx, 1, sizeof(BubbleParams));
    BubbleParams o = {
        .eee = 0.75f
    };
    LR_UniformBuffer_SetData(lrctx, ubo, &o, sizeof(BubbleParams), 0, 1);
    LR_Material_SetUniformBlock(lrctx, mat, "Bubble");
}

#define DEG_TO_RAD(x) ((x) * M_PI / 180.0)

static float rot = 0;

typedef struct {
    float x; float y; float z;
    float width; float height; float rot;
    float u; float v; uint32_t color;
} BillboardVertex;




static void AddBillboard(vec3 center, float rotate, uint32_t color)
{
    #define BSIZE (2.0f)
    #define V(_w,_h,_u,_v) { .x = center[0], .y = center[1], .z = center[2], .width = _w, .height = _h, .rot = rotate, .u = _u, .v = _v, .color = color }
    BillboardVertex verts[] = {
        V(BSIZE * -0.5f, BSIZE * -0.5f, 0, 0),
        V(BSIZE * 0.5f, BSIZE * -0.5f, 1, 0),
        V(BSIZE * -0.5f, BSIZE * 0.5f, 0, 1),
        V(BSIZE * 0.5f, BSIZE * 0.5f, 1, 1)
    };
    #undef V
    #undef BSIZE
    LR_DynamicDraw_Draw(lrctx, billboards, verts, 4 * sizeof(float) * 9, texture, fabs(center[2]));
}

static float t;
static void Sample_Loop()
{
    int w, h;
    t += 0.01;
    SDL_GetWindowSize(window, &w, &h);
    if(useMsaa && (!msaa || LR_RenderTarget_GetWidth(lrctx, msaa) != w || LR_RenderTarget_GetHeight(lrctx, msaa) != h)) {
        if(msaa) LR_RenderTarget_Destroy(lrctx, msaa);
        msaa = LR_RenderTarget_CreateMultisample(lrctx, w, h, 1, useMsaa); //user picked
    }
    LR_BeginFrame(lrctx, w, h);
    LR_SetRenderTarget(lrctx, msaa ? msaa : NULL);
    LR_ClearAll(lrctx, 0.0, 0.0, 0.0, 1.0);
    /* draw rendertarget */
    LR_SetRenderTarget(lrctx, rt);
    LR_ClearAll(lrctx, 0.392, 0.584, 0.929, 1.0);
    LR_SetRenderTarget(lrctx, msaa);
    /* set camera */
    mat4 view = GLM_MAT4_IDENTITY_INIT;
    mat4 projection = GLM_MAT4_IDENTITY_INIT;
    mat4 viewprojection = GLM_MAT4_IDENTITY_INIT;

    vec3 middle = GLM_VEC3_ZERO_INIT;
    vec3 far = GLM_VEC3_ZERO_INIT;
    far[2] = 8.0; //8 units away on Z axis

    glm_perspective(DEG_TO_RAD(35), (float)w / (float)h, 0.01, 10000, projection);
    glm_lookat(far, middle, GLM_YUP, view);
    glm_mul(projection, view, viewprojection);
    LR_SetCamera(lrctx, (LR_Matrix4x4*)view, (LR_Matrix4x4*)projection, (LR_Matrix4x4*)viewprojection);
    /* create object matrix */
    mat4 world = GLM_MAT4_IDENTITY_INIT;
    mat4 normal = GLM_MAT4_IDENTITY_INIT;
    vec3 yaxis = GLM_VEC3_ZERO_INIT;
    yaxis[1] = 1.0;
    rot -= 0.015;
    glm_rotate(world, rot, yaxis);
    glm_mat4_inv(world, normal);
    glm_mat4_transpose(normal);
    /* draw 2d */
    LR_2D_FillRectangleColors(lrctx, 0,0, w, h, 
        LR_RGBA(255, 0, 0, 255),
        LR_RGBA(0, 255, 0, 255),
        LR_RGBA(0, 0, 255, 255),
        LR_RGBA(255, 255, 255, 255)
    );
    LR_2D_DrawLine(lrctx, 0, 0, w, h, LR_RGBA(255,255,0,255));
    LR_2D_FillRectangle(lrctx, 100, 100, 100, 100, LR_RGBA(0xFF, 0, 0xFF, 0xFF));
    LR_2D_DrawImage(lrctx, texture, 200, 100, 0, 0, 0xFFFFFFFF);
    LR_2D_FillRectangle(lrctx, 100, 200, 100, 100, LR_RGBA(0x0, 0xFF, 0x00, 0xFF));
    //alpha
    LR_2D_DrawImage(lrctx, alphaPng, 400, 100, 0, 0, 0xFFFFFFFF);
    LR_2D_DrawImage(lrctx, alphaBgra, 400, 200, 0, 0, 0xFFFFFFFF);
    //dds
    LR_2D_DrawImage(lrctx, dxt5tex, 0, 0, 0, 0, 0xFFFFFFFF);
    //rt
    LR_2D_DrawImage(lrctx, LR_RenderTarget_GetTexture(lrctx, rt), 400, 300, 0, 0, 0xFFFFFFFF);
    /* draw 3d */
    BubbleParams p = { 
        .eee = t
    };
    LR_UniformBuffer_SetData(lrctx, ubo, &p, sizeof(BubbleParams), 0, 1);
    LR_UniformBufferBinding monkeyBinding = {
        .buffer = ubo,
        .start = 0,
        .count = 1
    };
    LR_Handle transform = LR_AllocTransform(lrctx, (LR_Matrix4x4*)world, (LR_Matrix4x4*)normal);
    ltMonkey.lightEnabled = ltToggle ? 1.0 : 0.0;
    LR_Handle lighting = LR_SetLights(lrctx, &ltMonkey, ltToggle ? sizeof(Lighting) : sizeof(float));
    LR_Draw(lrctx, mat, geom, &monkeyBinding, transform, lighting, LRPRIMTYPE_TRIANGLELIST, 0, baseVertex, startIndex, suzanneIndexCount);

    vec3 pos = { 3, 0, -2 };
    AddBillboard(pos, sin(t), LR_RGBA(0xFF,0xFF,0x00,0xBA));
    pos[0] = -2; pos[1] = 0.2; pos[2] = -1;
    AddBillboard(pos, tan(t), LR_RGBA(0x00, 0x00, 0xFF, 0xBA));
    pos[0] = -2; pos[1] = -0.5; pos[2] = -1.2;
    AddBillboard(pos, cos(t), LR_RGBA(0xFF, 0x00, 0x00, 0xBA));
    /* finish */
    if(msaa) {
        LR_SetRenderTarget(lrctx, NULL);
        LR_RenderTarget_BlitToScreen(lrctx, msaa);
    }
    LR_EndFrame(lrctx);
}

static void Sample_Exit()
{
    LR_Destroy(lrctx);
}
