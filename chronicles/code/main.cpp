#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3_image/SDL_image.h"
#include "SDL3_ttf/SDL_ttf.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "shared.h"
#include "graphics.cpp"

// Globals
SDL_Window                      *gSDLWindow;
SDL_GPUDevice                   *gGPUDevice;
SDL_GPUTexture                  *gDepthTexture;
SDL_GPUGraphicsPipeline         *gPipeline;
Arena                           gArena;

static int                      gDone;

const int gWINDOW_WIDTH  = 1920 / 2;
const int gWINDOW_HEIGHT = 1080 / 2;

bool
Update(Player *p)
{
    SDL_Log("update entered");
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
    if(keys[SDL_SCANCODE_LEFT])  p->ModelX -= 0.01f;
    if(keys[SDL_SCANCODE_RIGHT]) p->ModelX += 0.01f;
    if(keys[SDL_SCANCODE_UP])    p->ModelY -= 0.01f;
    if(keys[SDL_SCANCODE_DOWN])  p->ModelY += 0.01f;
    if(keys[SDL_SCANCODE_W])     p->ModelZ -= 0.01f;
    if(keys[SDL_SCANCODE_S])     p->ModelZ += 0.01f;
    if(keys[SDL_SCANCODE_A])     p->ModelRotY -= 0.01f;
    if(keys[SDL_SCANCODE_D])     p->ModelRotY += 0.01f;
    
    SDL_Log("keyboard input passed");
    
    return(true);
}

int
main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        return -1;
    }
    TTF_Init();
    
    gSDLWindow = SDL_CreateWindow("SDL3 window", gWINDOW_WIDTH, gWINDOW_HEIGHT, 0);
    
    // 16MB will need static for BSS as stack limit is 1MB
    static uint8_t perm[16 * 1024 * 1024];
    
    gArena = {};
    arenaInit(&gArena, perm, sizeof(perm));
    
    RendererInit();
    Background b = {};
    LoadBackground(&b);
    Model m = {};
    LoadModel(&m);
    Player p = {};
    Text t = {};
    LoadFont(&t);
    
    pipelineObjects po = {};
    
    LinePipeline lp = {};
    CreateLinePipeline(&lp);
    
    po.lp = &lp;
    
    gDone = 0;
    while (!gDone)
    {
        if(!Update(&p))
            gDone = 1;
        Render(&b, &m, &p, &po, &t);
    }
    RendererDestroy(&b, &m);
    
    SDL_DestroyWindow(gSDLWindow);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}