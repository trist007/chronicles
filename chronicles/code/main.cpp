#define IMGUI_DEFINE_MATH_OPERATORS
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3_image/SDL_image.h"
#include "SDL3_ttf/SDL_ttf.h"

#include "imgui/imgui.cpp"
#include "imgui/imgui_impl_sdl3.cpp"
#include "imgui/imgui_impl_sdlgpu3.cpp"
#include "imgui/imgui_demo.cpp"
#include "imgui/imgui_draw.cpp"
#include "imgui/imgui_tables.cpp"
#include "imgui/imgui_widgets.cpp"


#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

// Globals
const int gWINDOW_WIDTH  = 1920;
const int gWINDOW_HEIGHT = 1080;
SDL_Window                              *gSDLWindow;
SDL_GPUDevice                           *gGPUDevice;
SDL_GPUTexture                          *gDepthTexture;
SDL_GPUGraphicsPipeline                 *gPipeline;
static int                              gDone;
#include "shared.h"
extern Arena                            gArena;
font_atlas                              gAtlas;

#include "text.cpp"
#include "graphics.cpp"
#include "game.cpp"

void
sdl_log_stderr(void *userData, int category, SDL_LogPriority priority, const char *message)
{
    fprintf(stderr, "%s\n", message);
}

int
main(int argc, char** argv)
{
    // redirect SDL_Log to write to stderr
    SDL_SetLogOutputFunction(sdl_log_stderr, NULL);
    static bool running = true;
    
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS))
    {
        return -1;
    }
    
    const int version = SDL_GetVersion();
    
    SDL_Log("SDL version %d.%d.%d\n",
            SDL_VERSIONNUM_MAJOR(version),
            SDL_VERSIONNUM_MINOR(version),
            SDL_VERSIONNUM_MICRO(version));
    
    TTF_Init();
    
    gSDLWindow = SDL_CreateWindow("SDL3 window", gWINDOW_WIDTH, gWINDOW_HEIGHT, 0);
    IMGUI_CHECKVERSION();
    
    // 16MB will need static for BSS as stack limit is 1MB
    static uint8_t perm[16 * 1024 * 1024];
    
    gArena = {};
    arenaInit(&gArena, perm, sizeof(perm));
    
    GameState gamestate = {};
    
    // point light parameters
    gamestate.lightPos = { 4.5f, 0.0f, -5.0f };
    gamestate.ambientLight = 0.8f;
    gamestate.lightRadius = 10.0f;
    
    // camera starting
    gamestate.camera = { -0.65f, 7.0f, 11.0f };
    
    // player spawn location
    gamestate.player.position.x = 0.8f;
    gamestate.player.position.z = 2.0f;
    
    RendererInit(&gamestate);
    
    // ImGui init
    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLGPU(gSDLWindow);
    ImGui_ImplSDLGPU3_InitInfo imguiInfo = {};
    imguiInfo.Device             = gGPUDevice;
    imguiInfo.ColorTargetFormat  = SDL_GetGPUSwapchainTextureFormat(gGPUDevice, gSDLWindow);
    imguiInfo.MSAASamples        = SDL_GPU_SAMPLECOUNT_1;
    ImGui_ImplSDLGPU3_Init(&imguiInfo);
    
    //LoadBackground(&gamestate.bg);
    
    LoadRoom("../chronicles/data/rooms/room05.glb", &gamestate.room, &gamestate);
    LoadModel("../chronicles/data/models/arwin8.glb", &gamestate.model);
    
    int jointCount = gamestate.model.skeleton.jointCount;
    
    gamestate.model.uniformBuffer = (float *)arenaAlloc(&gArena, ((16 + jointCount * 16) * sizeof(float)));
    gamestate.model.worldMats = (float*)arenaAlloc(&gArena, (jointCount * 64));
    
    gAtlas = {};
    LoadFontAtlas(&gAtlas);
    
    CreateLinePipeline(&gamestate.po.lp);
    
    
    gDone = 0;
    Uint64 last = SDL_GetPerformanceCounter();
    while (!gDone)
    {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = (now - last) / (float)SDL_GetPerformanceFrequency();
        last = now;
        
        // NOTE(trist007): if ESCAPE is hit, then set gDone to 1 and quit
        if(!Update(&gamestate.player, dt))
            gDone = 1;
        Render(&gamestate, &gAtlas);
    }
    
    RendererDestroy(&gamestate.bg, &gamestate.model, &gamestate.po);
    
    ImGui_ImplSDLGPU3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    
    SDL_DestroyWindow(gSDLWindow);
    TTF_Quit();
    SDL_Quit();
    
    return(0);
}