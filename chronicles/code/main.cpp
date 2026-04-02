#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "SDL3/SDL.h"
#include "SDL3/SDL_main.h"
#include "SDL3_image/SDL_image.h"
#include "SDL3_ttf/SDL_ttf.h"

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

// Globals
const int gWINDOW_WIDTH  = 1920 /       2;
const int gWINDOW_HEIGHT = 1080 /       2;
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
    if(keys[SDL_SCANCODE_UP])    p->ModelZ -= 0.01f;
    if(keys[SDL_SCANCODE_DOWN])  p->ModelZ += 0.01f;
    if(keys[SDL_SCANCODE_W])     p->ModelZ -= 0.01f;
    if(keys[SDL_SCANCODE_S])     p->ModelZ += 0.01f;
    if(keys[SDL_SCANCODE_A])     p->ModelRotY -= 0.01f;
    if(keys[SDL_SCANCODE_D])     p->ModelRotY += 0.01f;
    
    SDL_Log("keyboard input passed");
    
    return(true);
}

void
DebugText(GameState *gamestate)
{
    
}

void
Render(GameState *gamestate, pipelineObjects *po, font_atlas *f)
{
    SDL_Log("Render started");
    gamestate->player.AnimTime = SDL_GetTicks() * 0.001f;
    if(gamestate->model.animCount > 1)
        sample_animation(&gamestate->model.animations[1],
                         gamestate->player.AnimTime, &gamestate->model.pose);
    eval_pose(&gamestate->model.pose, &gamestate->model.skeleton);
    SDL_Log("eval pose finished");
    
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gGPUDevice);
    SDL_GPUTexture *swapchain;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, gSDLWindow, &swapchain, NULL, NULL);
    
    SDL_Log("render bg started");
    RenderBackground(&gamestate->bg, cmd, swapchain);
    SDL_Log("render bg ended");
    SDL_Log("render model started");
    
    // build model * view * projection mvp
    float proj[16], view[16], model[16], vp[16], mvp[16];
    float trans[16], rot[16];
    
    float aspect = (float)gWINDOW_WIDTH / gWINDOW_HEIGHT;
    
    // orthographic mode
    //mat4_ortho(proj, -aspect, aspect, -1.0f, 1.0f, 0.1f, 100.0f);
    // perspective mode
    mat4_perspective(proj, 45.0f * (3.14159f / 180.0f),
                     (float)gWINDOW_WIDTH / gWINDOW_HEIGHT, 0.1f, 100.0f);
    
    // fixed camera
    //mat4_translation(view, 0.0f, 0.0f, -5.0f); // fixed camera pulled back
    // alone in the dark mode
    mat4_lookat(view,
                -0.65f, 7.0f, 14.0f, // camera position
                -0.65f, 0.0f, 4.65f, // look target
                0.0f,   1.0f, 0.0f);   // up vector
    
    // combine proj and view
    mat4_mul(vp, proj, view);
    
    // player - translate then rotate
    mat4_translation(trans, gamestate->player.ModelX,
                     gamestate->player.ModelY, gamestate->player.ModelZ); // model moves
    mat4_rotation_y(rot, gamestate->player.ModelRotY);
    mat4_mul(model, trans, rot);
    mat4_mul(mvp, vp, model);
    RenderModel(&gamestate->model, &gamestate->player, cmd, swapchain, mvp);
    
    /*
    // enemies
    for(int i = 0;
        i < enemyCount;
        i++)
    {
        mat4_translation(trans, enemies[i].x, enemies[i].y, enemies[i].z);
        mat4_rotation_y(rot, enemies[i].rotY);
        mat4_mul(model, trans, rot);
        mat4_mul(mvp, vp, model);
        RenderModel(&enemyModel, ..., cmd, swapchain, mvp);
        }
    */
    
    // Debug text
    char buf[256];
    snprintf(buf, sizeof(buf), "x: %.2f", gamestate->player.ModelX);
    DrawText(f, cmd, swapchain, buf, 10.0f, 10.0f, 1.0f, 0.0f, 0.0f);
    snprintf(buf, sizeof(buf), "y: %.2f", gamestate->player.ModelY);
    DrawText(f, cmd, swapchain, buf, 10.0f, 30.0f, 1.0f, 0.0f, 0.0f);
    snprintf(buf, sizeof(buf), "z: %.2f", gamestate->player.ModelZ);
    DrawText(f, cmd, swapchain, buf, 10.0f, 50.0f, 1.0f, 0.0f, 0.0f);
    snprintf(buf, sizeof(buf), "Graphics AP: %s", gamestate->graphicsAPI);
    DrawText(f, cmd, swapchain, buf, 10.0f, 70.0f, 1.0f, 0.0f, 0.0f);
    
    SDL_Log("render model ended");
    
    
    PushLine(po->lp,
             { 0.9f, 0.0f, 0.3f },
             {-0.9f, 0.0f,-0.7f }
             );
    
    float ortho[16];
    mat4_ortho(ortho, 0, gWINDOW_WIDTH, gWINDOW_HEIGHT, 0, -1.0f, 1.0f);
    FlushLines(po->lp, cmd, swapchain, ortho);
    
    SDL_SubmitGPUCommandBuffer(cmd);
    
    pose_reset(&gamestate->model.pose, &gamestate->model.skeleton);
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
    
    GameState gamestate = {};
    RendererInit(&gamestate);
    
    LoadBackground(&gamestate.bg);
    LoadModel(&gamestate.model);
    
    gAtlas = {};
    LoadFontAtlas(&gAtlas);
    
    pipelineObjects po = {};
    
    LinePipeline lp = {};
    CreateLinePipeline(&lp);
    
    po.lp = &lp;
    
    gDone = 0;
    while (!gDone)
    {
        // NOTE(trist007): if ESCAPE is hit, then set gDone to 1 and quit
        if(!Update(&gamestate.player))
            gDone = 1;
        Render(&gamestate, &po, &gAtlas);
    }
    
    RendererDestroy(&gamestate.bg, &gamestate.model);
    
    SDL_DestroyWindow(gSDLWindow);
    TTF_Quit();
    SDL_Quit();
    
    return 0;
}