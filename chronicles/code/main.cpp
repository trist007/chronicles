#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3_image/SDL_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

struct Arena
{
    uint8_t  *base;
    size_t   size;
    size_t   offset;
};

// Globals
SDL_Window                      *gSDLWindow;
SDL_GPUDevice                   *gGPUDevice;
SDL_GPUTexture                  *gDepthTexture;
SDL_GPUGraphicsPipeline         *gPipeline;
Arena                           gArena;

static int                      gDone;

const int gWINDOW_WIDTH  = 1920 / 2;
const int gWINDOW_HEIGHT = 1080 / 2;

void
arenaInit(Arena *a, void *buf, size_t size)
{
    a->base   = (uint8_t *)buf;
    a->size   = size;
    a->offset = 0;
}

void *
arenaAlloc(Arena *a, size_t s)
{
    if(a->offset + s > a->size)
        return NULL;
    void *ptr = a->base + a->offset;
    a->offset += s;
    
    return ptr;
}

#include "graphics.cpp"

bool
update(Player *p)
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

void
loop(Background *b, Model *m, Player *p, SDL_Window *w)
{
    if (!update(p))
    {
        gDone = 1;
    }
    else
    {
        Render(b, m, p, w);
    }
}

int
main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        return -1;
    }
    
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
    
    gDone = 0;
    while (!gDone)
    {
        loop(&b, &m, &p, gSDLWindow);
        
    }
    RendererDestroy(&b, &m);
    
    SDL_DestroyWindow(gSDLWindow);
    SDL_Quit();
    
    return 0;
}