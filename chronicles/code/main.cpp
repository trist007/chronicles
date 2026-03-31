#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3_image/SDL_image.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "graphics.cpp"

SDL_Window*          gSDLWindow;
static int           gDone;

const int WINDOW_WIDTH  = 1920 / 2;
const int WINDOW_HEIGHT = 1080 / 2;



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
loop(Renderer *r, Background *b, Model *m, Player *p, SDL_Window *w)
{
    if (!update(p))
    {
        gDone = 1;
    }
    else
    {
        Render(r, b, m, p, w);
    }
}

int
main(int argc, char** argv)
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        return -1;
    }
    
    gSDLWindow = SDL_CreateWindow("SDL3 window", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    
    Renderer r = {};
    RendererInit(&r, gSDLWindow);
    Background b = {};
    LoadBackground(&r, &b, gSDLWindow);
    Model m = {};
    LoadModel(&r, &m, gSDLWindow);
    Player p = {};
    
    gDone = 0;
    while (!gDone)
    {
        loop(&r, &b, &m, &p, gSDLWindow);
        
    }
    RendererDestroy(&r, &b, &m);
    
    SDL_DestroyWindow(gSDLWindow);
    SDL_Quit();
    
    return 0;
}