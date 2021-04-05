#include <SDL2/SDL.h>
#include <lancerrender.h>
#include <stdio.h>
#include <math.h>
#include <cglm/cglm.h>  
#include "stb_image.h"
#include <lancerrender_shaderfile.h>

#define SUZANNE_H_DEFINE_DATA
#include "suzanne.h"

static SDL_Window* window;
static int gles = 0;

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
    if(argc > 1 && !strcmp(argv[1], "gles")) {
        printf("Using GLES 3.1 context\n");
        gles = 1;
    }

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

static void Sample_Init()
{
    lrctx = LR_Init(gles);
    texture = LR_Texture_Create(lrctx, 0);
    int x,y,n;
    unsigned char *data = stbi_load("texture.png", &x, &y, &n, 4);
    LR_Texture_Allocate(lrctx, texture, LRTEXTYPE_2D, LRTEXFORMAT_COLOR, x, y);
    LR_Texture_SetRectangle(lrctx, texture, 0, 0, x, y, data);
    stbi_image_free(data);

    monkey = LR_Texture_Create(lrctx, 0);
    data = stbi_load("monkey.png", &x, &y, &n, 4);
    //swap channels
    for(int i = 0; i < x * y; i++) {
        unsigned char *ptr = &data[i * 4];
        unsigned char tmp = ptr[0];
        ptr[0] = ptr[2];
        ptr[2] = tmp;
    }
    LR_Texture_Allocate(lrctx, monkey, LRTEXTYPE_2D, LRTEXFORMAT_COLOR, x, y);
    LR_Texture_SetRectangle(lrctx, monkey, 0, 0, x, y, data);
    stbi_image_free(data);

    LR_ShaderCollection *sh = LR_ShaderCollection_Create(lrctx);
    SDL_RWops* file = SDL_RWFromFile("monkeyshader.shader", "rb");
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
}

#define DEG_TO_RAD(x) ((x) * M_PI / 180.0)

static float rot = 0;

static void Sample_Loop()
{
    int w, h;
    SDL_GetWindowSize(window, &w, &h);
    LR_BeginFrame(lrctx, w, h);
    LR_ClearAll(lrctx, 0.0, 0.0, 0.0, 1.0);
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
    /* draw 3d */
    LR_Handle transform = LR_AllocTransform(lrctx, (LR_Matrix4x4*)world, (LR_Matrix4x4*)normal);
    ltMonkey.lightEnabled = ltToggle ? 1.0 : 0.0;
    LR_Handle lighting = LR_SetLights(lrctx, &ltMonkey, ltToggle ? sizeof(Lighting) : sizeof(float));
    LR_Draw(lrctx, mat, geom, transform, lighting, LRPRIMTYPE_TRIANGLELIST, 0, baseVertex, startIndex, suzanneIndexCount);
    /* finish */
    LR_EndFrame(lrctx);
}

static void Sample_Exit()
{
    LR_Destroy(lrctx);
}
