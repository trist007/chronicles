#include <string.h>
#include <math.h>
#include <stdlib.h>
#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"

int *gFrameBuffer;
SDL_Window *gSDLWindow;
SDL_Renderer *gSDLRenderer;
SDL_Texture *gSDLTexture;
static int gDone;
const int WINDOW_WIDTH = 1920 / 2;
const int WINDOW_HEIGHT = 1080 / 2;

bool
update()
{
    SDL_Event e;
    if(SDL_PollEvent(&e))
    {
        if(e.type == SDL_EVENT_QUIT)
        {
            return(false);
        }
        if(e.type == SDL_EVENT_KEY_UP && e.key.key == SDLK_ESCAPE)
        {
            return(false);
        }
    }
    
    char *pix;
    int pitch;
    
    SDL_LockTexture(gSDLTexture, NULL, (void**)&pix, &pitch);
    
    for(int i = 0, sp = 0, dp = 0;
        i < WINDOW_HEIGHT;
        i++, dp += WINDOW_WIDTH, sp += pitch)
        memcpy(pix + sp, gFrameBuffer + dp, WINDOW_WIDTH * 4);
    
    SDL_UnlockTexture(gSDLTexture);
    SDL_RenderTexture(gSDLRenderer, gSDLTexture, NULL, NULL);
    SDL_RenderPresent(gSDLRenderer);
    SDL_Delay(1);
    
    return(true);
}

void
putpixel(int x, int y, int color)
{
    if(x < 0 ||
       y < 0 ||
       x >= WINDOW_WIDTH ||
       y >= WINDOW_HEIGHT)
    {
        return;
    }
    gFrameBuffer[y * WINDOW_WIDTH + x] = color;
}

const unsigned char sprite[] =
{
    0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,
    0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
    0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
    0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,
    0,0,1,1,1,1,1,1,1,1,1,1,1,1,0,0,
    0,0,0,1,1,1,1,1,1,1,1,1,1,0,0,0,
    0,0,0,0,0,0,1,1,1,1,0,0,0,0,0,0
};

void drawsprite(int x, int y, unsigned int color)
{
    int i, j, c, yofs;
    yofs = y * WINDOW_WIDTH + x;
    for (i = 0, c = 0; i < 16; i++)
    {
        for (j = 0; j < 16; j++, c++)
        {
            if (sprite[c])
            {
                gFrameBuffer[yofs + j] = color;
            }
        }
        yofs += WINDOW_WIDTH;
    }
}

void
render(Uint64 aTicks)
{
    for(int i = 0, c = 0;
        i < WINDOW_HEIGHT;
        i++)
    {
        for(int j = 0;
            j < WINDOW_WIDTH;
            j++, c++)
        {
            gFrameBuffer[c] = 0xff000000;
        }
    }
    for (int i = 0; i < 128; i++)
    {
        drawsprite((int)((WINDOW_WIDTH / 2) + sin((aTicks + i * 10) * 0.003459734) * (WINDOW_WIDTH / 2 - 16)),
                   (int)((WINDOW_HEIGHT / 2) + sin((aTicks + i * 10) * 0.003345973) * (WINDOW_HEIGHT / 2 - 16)),
                   ((int)(sin((aTicks * 0.2 + i) * 0.234897) * 127 + 128) << 16) |
                   ((int)(sin((aTicks * 0.2 + i) * 0.123489) * 127 + 128) << 8) |
                   ((int)(sin((aTicks * 0.2 + i) * 0.312348) * 127 + 128) << 0) |
                   0xff000000);
    }
}

void
loop()
{
    if(!update())
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

int main(int argc, char **argv)
{
    if(!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        return(-1);
    }
    
    gFrameBuffer = new int[WINDOW_WIDTH * WINDOW_HEIGHT];
    gSDLWindow = SDL_CreateWindow("SDL3 window", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    gSDLRenderer = SDL_CreateRenderer(gSDLWindow, NULL);
    gSDLTexture = SDL_CreateTexture(gSDLRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING,
                                    WINDOW_WIDTH, WINDOW_HEIGHT);
    if(!gFrameBuffer || !gSDLWindow || !gSDLRenderer || !gSDLTexture)
        return(-1);
    
    gDone = 0;
    
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while(!gDone)
    {
        loop();
    }
#endif
    
    SDL_DestroyTexture(gSDLTexture);
    SDL_DestroyRenderer(gSDLRenderer);
    SDL_DestroyWindow(gSDLWindow);
    SDL_Quit();
    
    return(0);
}