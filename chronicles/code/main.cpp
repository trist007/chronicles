#include <string.h>
#include <math.h>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3_image/SDL_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "model.cpp"

int* gFrameBuffer;
SDL_Window* gSDLWindow;
SDL_Renderer* gSDLRenderer;
SDL_Texture* gSDLTexture;
SDL_Texture* gBackground;
static int gDone;

float gAnimTime = 0.0f;
float *gZBuffer;
float gModelX = 0.0f;
float gModelY = -1.09f;
float gModelZ = 3.0f;
float gModelRotY = 0.0f;

Vec3 *gSkinnedPositions;
Vec3 *gSkinnedNormals;

const int WINDOW_WIDTH = 1920 / 2;
const int WINDOW_HEIGHT = 1080 / 2;


bool
update()
{
    SDL_Event e;
    if (SDL_PollEvent(&e))
    {
        if (e.type == SDL_EVENT_QUIT)
        {
            return false;
        }
        if (e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_ESCAPE)
        {
            return false;
        }
    }
    
    const bool *keys = SDL_GetKeyboardState(NULL);
    if(keys[SDL_SCANCODE_LEFT])  gModelX -= 0.01f;
    if(keys[SDL_SCANCODE_RIGHT]) gModelX += 0.01f;
    if(keys[SDL_SCANCODE_UP])    gModelY -= 0.01f;
    if(keys[SDL_SCANCODE_DOWN])  gModelY += 0.01f;
    if(keys[SDL_SCANCODE_W])     gModelZ -= 0.01f;
    if(keys[SDL_SCANCODE_S])     gModelZ += 0.01f;
    if(keys[SDL_SCANCODE_A])     gModelRotY -= 0.01f;
    if(keys[SDL_SCANCODE_D])     gModelRotY += 0.01f;
    
    char* pix;
    int pitch;
    
    SDL_LockTexture(gSDLTexture, NULL, (void**)&pix, &pitch);
    for (int i = 0, sp = 0, dp = 0; i < WINDOW_HEIGHT; i++, dp += WINDOW_WIDTH, sp += pitch)
        memcpy(pix + sp, gFrameBuffer + dp, WINDOW_WIDTH * 4);
    
    SDL_UnlockTexture(gSDLTexture);  
    SDL_RenderTexture(gSDLRenderer, gBackground, NULL, NULL);  // background
    SDL_RenderTexture(gSDLRenderer, gSDLTexture, NULL, NULL);  // framebuffer on top
    SDL_RenderPresent(gSDLRenderer);
    SDL_Delay(1);
    return true;
}

void
render(Uint64 aTicks)
{
    // clear framebuffer
    memset(gFrameBuffer, 0, WINDOW_WIDTH * WINDOW_HEIGHT * 4);
    for(int i = 0; i < WINDOW_WIDTH * WINDOW_HEIGHT; i++)
        gZBuffer[i] = 1e9f;
    
    // advance animation
    gAnimTime = (aTicks * 0.001f);  // convert ms to seconds
    
    // sample anim 0 into pose
    if(gModel.animCount > 1)
        sample_animation(&gModel.animations[1], gAnimTime, &gPose);
    
    // compute skinning matrices
    eval_pose(&gPose, &gModel.skeleton);
    
    // skin the mesh
    skin_mesh(&gModel.mesh, &gPose);
    
    // rasterize triangles using gSkinnedPositions
    float rotY = 0.0f;  // no extra rotation needed, animation drives it
    
    for(int p = 0; p < gModel.mesh.primitiveCount; p++)
    {
        Primitive *prim = &gModel.mesh.primitives[p];
        
        for(int t = prim->triOffset; t < prim->triOffset + prim->triCount; t++)
        {
            int ia = gModel.mesh.tris[t].v[0];
            int ib = gModel.mesh.tris[t].v[1];
            int ic = gModel.mesh.tris[t].v[2];
            
            Vec3 a = project(gSkinnedPositions[ia], gModelRotY);
            Vec3 b = project(gSkinnedPositions[ib], gModelRotY);
            Vec3 c = project(gSkinnedPositions[ic], gModelRotY);
            
            draw_tri(a, b, c, prim->color | 0xff000000);
        }
    }
}

void
loop()
{
    if (!update())
    {
        gDone = 1;
#ifdef __EMSCRIPTEN__
        emscripten_cancel_main_loop();
#endif
    }
    else
    {
        render(SDL_GetTicks());
    }
}

int
main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        return -1;
    }
    
    gFrameBuffer = new int[WINDOW_WIDTH * WINDOW_HEIGHT];
    gSDLWindow = SDL_CreateWindow("SDL3 window", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    gSDLRenderer = SDL_CreateRenderer(gSDLWindow, NULL);
    gSDLTexture = SDL_CreateTexture(gSDLRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);
    SDL_SetTextureBlendMode(gSDLTexture, SDL_BLENDMODE_BLEND);
    
    if (!gFrameBuffer || !gSDLWindow || !gSDLRenderer || !gSDLTexture)
        return -1;
    
    // Add this after the check
    SDL_Surface *bg = IMG_Load("../chronicles/data/textures/background.png");
    if(!bg)
    {
        SDL_Log("IMG_Load failed: %s", SDL_GetError());
        return -1;
    }
    gBackground = SDL_CreateTextureFromSurface(gSDLRenderer, bg);
    SDL_DestroySurface(bg);
    if(!gBackground)
    {
        SDL_Log("CreateTextureFromSurface failed: %s", SDL_GetError());
        return -1;
    }
    
    load_model("../chronicles/data/models/arwin8.glb");
    SDL_Log("verts: %d  tris: %d", gModel.mesh.vertCount, gModel.mesh.triCount);
    
    SDL_Log("anim count: %d", gModel.animCount);
    for(int i = 0; i < gModel.animCount; i++)
        SDL_Log("anim %d: %s duration=%.2f channels=%d", 
                i, gModel.animations[i].name, 
                gModel.animations[i].duration,
                gModel.animations[i].channelCount);
    
    SDL_Log("joint count: %d", gModel.skeleton.jointCount);
    for(int i = 0; i < gModel.skeleton.jointCount; i++)
        SDL_Log("joint %d: %s parent=%d", 
                i, gModel.skeleton.joints[i].name,
                gModel.skeleton.joints[i].parent);
    
    for(int i = 0; i < 10; i++)
    {
        Vertex *v = &gModel.mesh.verts[i];
        SDL_Log("vert %d: joints=%d,%d,%d,%d weights=%.2f,%.2f,%.2f,%.2f",
                i,
                v->joints[0], v->joints[1], v->joints[2], v->joints[3],
                v->weights[0], v->weights[1], v->weights[2], v->weights[3]);
    }
    
    static bool logged = false;
    if(!logged)
    {
        for(int i = 0; i < 5; i++)
        {
            float *m = gModel.skeleton.joints[i].inverseBindMatrix;
            SDL_Log("joint %d %s ibm:", i, gModel.skeleton.joints[i].name);
            SDL_Log("  %.2f %.2f %.2f %.2f", m[0],m[1],m[2],m[3]);
            SDL_Log("  %.2f %.2f %.2f %.2f", m[4],m[5],m[6],m[7]);
            SDL_Log("  %.2f %.2f %.2f %.2f", m[8],m[9],m[10],m[11]);
            SDL_Log("  %.2f %.2f %.2f %.2f", m[12],m[13],m[14],m[15]);
        }
        logged = true;
    }
    
    debug_model_bounds(&gModel.mesh);
    pose_init(&gPose, &gModel.skeleton);
    
    gSkinnedPositions = new Vec3[gModel.mesh.vertCount];
    gSkinnedNormals = new Vec3[gModel.mesh.vertCount];
    
    gZBuffer = new float[WINDOW_WIDTH * WINDOW_HEIGHT];
    
    gDone = 0;
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!gDone)
    {
        loop();
    }
#endif
    
    SDL_DestroyTexture(gSDLTexture);
    SDL_DestroyRenderer(gSDLRenderer);
    SDL_DestroyWindow(gSDLWindow);
    SDL_Quit();
    
    return 0;
}