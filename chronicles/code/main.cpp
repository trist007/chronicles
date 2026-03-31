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

//int*                 gFrameBuffer;
SDL_Window*          gSDLWindow;

// NOTE(trist007): Software Rendering
/*
SDL_Renderer*        gSDLRenderer; // need it just for the background
SDL_Texture*         gSDLTexture;
SDL_Texture*         gBackground;
float                *gZBuffer;
*/

// NOTE(trist007): Hardware Acceleration
SDL_GPUDevice*           gGPUDevice;
SDL_GPUTexture*          gDepthTexture;
SDL_GPUGraphicsPipeline  *gPipeline;
//extern Model             gModel;
SDL_GPUBuffer        *gVertexBuffer;
SDL_GPUBuffer        *gIndexBuffer;


// background
SDL_GPUTexture*          gBackgroundTexture;
SDL_GPUSampler*          gBackgroundSampler;
SDL_GPUGraphicsPipeline* gBackgroundPipeline;
SDL_GPUBuffer*           gBackgroundVertexBuffer;

static int           gDone;

float gAnimTime  = 0.0f;
float gModelX    = 0.0f;
float gModelY    = -1.09f;
float gModelZ    = 3.0f;
float gModelRotY = 0.0f;

//Vec3  *gSkinnedPositions;
//Vec3  *gSkinnedNormals;

const int WINDOW_WIDTH  = 1920 / 2;
const int WINDOW_HEIGHT = 1080 / 2;


bool
update()
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
    if(keys[SDL_SCANCODE_LEFT])  gModelX -= 0.01f;
    if(keys[SDL_SCANCODE_RIGHT]) gModelX += 0.01f;
    if(keys[SDL_SCANCODE_UP])    gModelY -= 0.01f;
    if(keys[SDL_SCANCODE_DOWN])  gModelY += 0.01f;
    if(keys[SDL_SCANCODE_W])     gModelZ -= 0.01f;
    if(keys[SDL_SCANCODE_S])     gModelZ += 0.01f;
    if(keys[SDL_SCANCODE_A])     gModelRotY -= 0.01f;
    if(keys[SDL_SCANCODE_D])     gModelRotY += 0.01f;
    
    SDL_Log("keyboard input passed");
    return(true);
    
    // NOTE(trist007): Software Rendering
    /*
    char* pix;
    int pitch;
    
    SDL_LockTexture(gSDLTexture, NULL, (void**)&pix, &pitch);
    for (int i = 0, sp = 0, dp = 0; i < WINDOW_HEIGHT; i++, dp += WINDOW_WIDTH, sp += pitch)
        memcpy(pix + sp, gFrameBuffer + dp, WINDOW_WIDTH * 4);
    
    SDL_UnlockTexture(gSDLTexture);  
    SDL_RenderTexture(gSDLRenderer, gSDLTexture, NULL, NULL);  // framebuffer on top
    SDL_RenderPresent(gSDLRenderer);
    SDL_Delay(1);
    return true;
    */
}

void
render(Uint64 aTicks)
{
    SDL_Log("render started");
    // CPU still does animation sampling + eval_pose() to get pose->matrices
    // but no skin_mesh() call as that is now in the vertex shader
    
    gAnimTime = (SDL_GetTicks() * 0.001f);
    if(gModel.animCount > 1)
        sample_animation(&gModel.animations[1], gAnimTime, &gPose);
    eval_pose(&gPose, &gModel.skeleton);
    
    SDL_Log("eval pose");
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gGPUDevice);
    SDL_GPUTexture *swapchain;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, gSDLWindow, &swapchain, NULL, NULL);
    
    SDL_Log("beginning bg pass");
    SDL_Log("bgBackgroundPipeline: %p", (void*)gBackgroundPipeline);
    SDL_Log("gBackgroundTexture: %p", (void*)gBackgroundTexture);
    SDL_Log("gBackgroundSampler: %p", (void*)gBackgroundSampler);
    SDL_Log("swapchain: %p", (void*)swapchain);
    SDL_GPUColorTargetInfo bgColor = {};
    bgColor.texture = swapchain;
    bgColor.load_op = SDL_GPU_LOADOP_CLEAR;
    bgColor.clear_color = {0,0,0,1};
    bgColor.store_op = SDL_GPU_STOREOP_STORE;
    
    // draw background first - no depth buffer
    SDL_GPURenderPass *bgPass = SDL_BeginGPURenderPass(cmd, &bgColor, 1, NULL);
    SDL_BindGPUGraphicsPipeline(bgPass, gBackgroundPipeline);
    SDL_GPUTextureSamplerBinding bgBinding = { gBackgroundTexture, gBackgroundSampler };
    SDL_BindGPUFragmentSamplers(bgPass, 0, &bgBinding, 1);
    SDL_DrawGPUPrimitives(bgPass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(bgPass);
    SDL_Log("ending bg pass");
    
    SDL_Log("starting model pass");
    // model pass - LOAD to keep background
    SDL_GPUColorTargetInfo modelColor = {};
    modelColor.texture  = swapchain;
    modelColor.load_op  = SDL_GPU_LOADOP_LOAD;
    modelColor.store_op = SDL_GPU_STOREOP_STORE;
    
    // depth target (replaces gZBuffer)
    SDL_GPUDepthStencilTargetInfo depth = {};
    depth.texture = gDepthTexture;
    depth.load_op = SDL_GPU_LOADOP_CLEAR;
    depth.clear_depth = 1.0f;
    depth.store_op = SDL_GPU_STOREOP_DONT_CARE;
    
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &modelColor, 1, &depth);
    SDL_BindGPUGraphicsPipeline(pass, gPipeline);
    SDL_Log("ending model pass");
    
    SDL_GPUBufferBinding vb = { gVertexBuffer, 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    SDL_GPUBufferBinding ib = { gIndexBuffer, 0 };
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    
    // build MVP
    float proj[16], view[16], model[16], vp[16], mvp[16];
    mat4_perspective(proj, 1.0f, (float)WINDOW_WIDTH / WINDOW_HEIGHT, 0.1f, 100.0f);
    mat4_translation(view, -gModelX, -gModelY, -gModelZ);
    mat4_rotation_y(model, gModelRotY);
    mat4_mul(vp, proj, view);
    mat4_mul(mvp, vp, model);
    
    // push mvp + skin matrices together
    int jointCount = gModel.skeleton.jointCount;
    int uniformSize = (16 + jointCount * 16) * sizeof(float);
    float *uniforms = (float*)alloca(uniformSize);
    memcpy(uniforms,      mvp,              64);
    memcpy(uniforms + 16, gPose.matrices,   jointCount * 64);
    SDL_PushGPUVertexUniformData(cmd, 0, uniforms, uniformSize);
    
    // draw each primitive with its color
    if(gModel.mesh.primitiveCount <= 0)
        return;
    for(int p = 0;
        p < gModel.mesh.primitiveCount;
        p++)
    {
        Primitive *prim = &gModel.mesh.primitives[p];
        // push per primitive color as fragment uniform
        float r = ((prim->color >> 0) & 0xff) / 255.0f;
        float g = ((prim->color >> 8) & 0xff) / 255.0f;
        float b = ((prim->color >> 16) & 0xff) / 255.0f;
        
        float col[4] = { r, g, b, 1.0f };
        SDL_PushGPUFragmentUniformData(cmd, 0, col, sizeof(col));
        SDL_DrawGPUIndexedPrimitives(pass, prim->triCount * 3, 1, prim->triOffset * 3, 0, 0);
    }
    
    SDL_EndGPURenderPass(pass);
    SDL_SubmitGPUCommandBuffer(cmd);
}

// NOTE(trist007): Software Rendering
/*
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

*/

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
    
    //gFrameBuffer = new int[WINDOW_WIDTH * WINDOW_HEIGHT];
    gSDLWindow = SDL_CreateWindow("SDL3 window", WINDOW_WIDTH, WINDOW_HEIGHT, 0);
    
    // NOTE(trist007): Software rendering
    //gSDLRenderer = SDL_CreateRenderer(gSDLWindow, NULL);
    //gSDLTexture = SDL_CreateTexture(gSDLRenderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, WINDOW_WIDTH, WINDOW_HEIGHT);
    //SDL_SetTextureBlendMode(gSDLTexture, SDL_BLENDMODE_BLEND);
    
    
    // Create GPUDevice for hardware acceleration
    
    gGPUDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
                                     true, NULL);
    
    if(!gGPUDevice)
    {
        SDL_Log("GPU device is NULL: %s\n", SDL_GetError());
        return(-1);
    }
    SDL_Log("GPU device ok: %p\n", (void*)gGPUDevice);
    
    SDL_ClaimWindowForGPUDevice(gGPUDevice, gSDLWindow);
    
    
    // NOTE(trist007): null check for Software Rendering
    //if (!gFrameBuffer || !gSDLWindow || !gSDLRenderer || !gSDLTexture)
    //return -1;
    
    const char *basePath = SDL_GetBasePath();
    SDL_Log("cwd: %s", basePath);
    
    // Background
    SDL_Surface *bg = IMG_Load("../chronicles/data/textures/background.png");
    if(!bg)
    {
        SDL_Log("IMG_Load failed: %s", SDL_GetError());
        return -1;
    }
    
    SDL_Surface *bgRGBA = SDL_ConvertSurface(bg, SDL_PIXELFORMAT_ABGR8888);
    SDL_DestroySurface(bg);
    
    SDL_GPUTextureCreateInfo bgTexInfo = {};
    bgTexInfo.type                     = SDL_GPU_TEXTURETYPE_2D;
    bgTexInfo.format                   = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    bgTexInfo.usage                    = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    bgTexInfo.width                    = bgRGBA->w;
    bgTexInfo.height                   = bgRGBA->h;
    bgTexInfo.layer_count_or_depth     = 1;
    bgTexInfo.num_levels               = 1;
    
    
    gBackgroundTexture                 = SDL_CreateGPUTexture(gGPUDevice, &bgTexInfo);
    
    // upload via transfer buffer
    Uint32 bgSize                            = bgRGBA->w * bgRGBA->h * 4;
    SDL_GPUTransferBufferCreateInfo bgTbInfo = {};
    bgTbInfo.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    bgTbInfo.size                            = bgSize;
    SDL_GPUTransferBuffer *bgTb              = SDL_CreateGPUTransferBuffer(gGPUDevice, &bgTbInfo);
    
    
    void *bgPtr                              = SDL_MapGPUTransferBuffer(gGPUDevice, bgTb, false);
    memcpy(bgPtr, bgRGBA->pixels, bgSize);
    SDL_UnmapGPUTransferBuffer(gGPUDevice, bgTb);
    
    SDL_GPUCommandBuffer *bgCmd              = SDL_AcquireGPUCommandBuffer(gGPUDevice);
    SDL_GPUCopyPass *bgCp                    = SDL_BeginGPUCopyPass(bgCmd);
    
    SDL_GPUTextureTransferInfo bgSrc         = {};
    bgSrc.transfer_buffer                    = bgTb;
    bgSrc.offset                             = 0;
    
    SDL_GPUTextureRegion bgDst               = {};
    bgDst.texture                            = gBackgroundTexture;
    bgDst.w                                  = bgRGBA->w;
    bgDst.h                                  = bgRGBA->h;
    bgDst.d                                  = 1;
    
    SDL_UploadToGPUTexture(bgCp, &bgSrc, &bgDst, false);
    SDL_EndGPUCopyPass(bgCp);
    SDL_SubmitGPUCommandBuffer(bgCmd);
    SDL_ReleaseGPUTransferBuffer(gGPUDevice, bgTb);
    SDL_DestroySurface(bgRGBA);
    
    // Create sampler
    SDL_GPUSamplerCreateInfo samplerInfo = {};
    samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    gBackgroundSampler = SDL_CreateGPUSampler(gGPUDevice, &samplerInfo);
    
    // Load background shader bytecode
    size_t bgvertSize, bgfragSize;
    void *bgvertCode = SDL_LoadFile("../chronicles/shaders/bg_vertex.dxil", &bgvertSize);
    void *bgfragCode = SDL_LoadFile("../chronicles/shaders/bg_fragment.dxil", &bgfragSize);
    SDL_Log("bgvertCode: %p\n", bgvertCode);
    SDL_Log("bgfragCode: %p\n", bgfragCode);
    
    SDL_GPUShaderCreateInfo bgvertInfo = {};
    bgvertInfo.code                    = (Uint8*)bgvertCode;
    bgvertInfo.code_size               = bgvertSize;
    bgvertInfo.entrypoint              = "main";
    bgvertInfo.format                  = SDL_GPU_SHADERFORMAT_DXIL;
    bgvertInfo.stage                   = SDL_GPU_SHADERSTAGE_VERTEX;
    bgvertInfo.num_uniform_buffers     = 0;
    SDL_GPUShader *bgvertShader        = SDL_CreateGPUShader(gGPUDevice, &bgvertInfo);
    SDL_Log("bgvertShader: %p error: %s", (void*)bgvertShader, SDL_GetError());
    
    
    
    SDL_GPUShaderCreateInfo bgfragInfo = {};
    bgfragInfo.code                    = (Uint8*)bgfragCode;
    bgfragInfo.code_size               = bgfragSize;
    bgfragInfo.entrypoint              = "main";
    bgfragInfo.format                  = SDL_GPU_SHADERFORMAT_DXIL;
    bgfragInfo.stage                   = SDL_GPU_SHADERSTAGE_FRAGMENT;
    bgfragInfo.num_uniform_buffers     = 0;
    bgfragInfo.num_samplers            = 1;
    bgfragInfo.num_storage_textures    = 0;
    bgfragInfo.num_storage_buffers     = 0;
    bgfragInfo.num_uniform_buffers     = 0;
    
    SDL_GPUShader *bgfragShader        = SDL_CreateGPUShader(gGPUDevice, &bgfragInfo);
    SDL_Log("bgfragShader: %p error: %s", (void*)bgfragShader, SDL_GetError());
    
    SDL_Log("GPU driver: %s", SDL_GetGPUDeviceDriver(gGPUDevice));
    
    
    
    SDL_GPUGraphicsPipelineCreateInfo bgPipeInfo = {};
    bgPipeInfo.vertex_shader                     = bgvertShader;
    bgPipeInfo.fragment_shader                   = bgfragShader;
    // no vertex input state - fullscreen triangle generated in shader
    // no depth stencil state - background doesn't need depth
    
    SDL_GPUColorTargetDescription bgColorDesc    = {};
    bgColorDesc.format                           = SDL_GetGPUSwapchainTextureFormat(gGPUDevice, gSDLWindow);
    bgPipeInfo.target_info.color_target_descriptions = &bgColorDesc;
    bgPipeInfo.target_info.num_color_targets     = 1;
    
    gBackgroundPipeline = SDL_CreateGPUGraphicsPipeline(gGPUDevice, &bgPipeInfo);
    SDL_Log("bgPipeline: %p", (void*)gBackgroundPipeline);
    if(!gBackgroundPipeline)
    {
        SDL_Log("bgPipeline failed: %s", SDL_GetError());
    }
    
    
    // can release shaders after pipeline is created
    SDL_ReleaseGPUShader(gGPUDevice, bgvertShader);
    SDL_ReleaseGPUShader(gGPUDevice, bgfragShader);
    
    load_model("../chronicles/data/models/arwin8.glb");
    SDL_Log("primitiveCount after load: %d", gModel.mesh.primitiveCount);
    SDL_Log("vertCount: %d  triCount: %d", gModel.mesh.vertCount, gModel.mesh.triCount);
    
    // Upload mesh to GPU buffers
    
    // vertex buffer
    SDL_GPUBufferCreateInfo vbInfo         = {};
    vbInfo.usage                           = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbInfo.size                            = gModel.mesh.vertCount * sizeof(Vertex);
    gVertexBuffer                          = SDL_CreateGPUBuffer(gGPUDevice, &vbInfo);
    
    // index buffer
    SDL_GPUBufferCreateInfo ibInfo         = {};
    ibInfo.usage                           = SDL_GPU_BUFFERUSAGE_INDEX;
    ibInfo.size                            = gModel.mesh.triCount * 3 * sizeof(int);
    gIndexBuffer                           = SDL_CreateGPUBuffer(gGPUDevice, &ibInfo);
    
    // Upload transfer buffer
    SDL_GPUTransferBufferCreateInfo tbInfo = {};
    tbInfo.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbInfo.size                            = vbInfo.size + ibInfo.size;
    SDL_GPUTransferBuffer *tb              = SDL_CreateGPUTransferBuffer(gGPUDevice, &tbInfo);
    
    void *ptr = SDL_MapGPUTransferBuffer(gGPUDevice, tb, false);
    memcpy(ptr, gModel.mesh.verts, vbInfo.size);
    memcpy((char*)ptr + vbInfo.size, gModel.mesh.tris, ibInfo.size);
    SDL_UnmapGPUTransferBuffer(gGPUDevice, tb);
    
    // Record the upload into a command buffer
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gGPUDevice);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = { tb, 0 };
    SDL_GPUBufferRegion dst = { gVertexBuffer, 0, vbInfo.size };
    SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    
    SDL_GPUTransferBufferLocation src2 = { tb, vbInfo.size };
    SDL_GPUBufferRegion dst2 = { gIndexBuffer, 0, ibInfo.size };
    SDL_UploadToGPUBuffer(cp, &src2, &dst2, false);
    
    // repeat for index buffer at offset vbInfo.size
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(gGPUDevice, tb);
    
    // Load shader bytecode
    size_t vertSize, fragSize;
    void *vertCode = SDL_LoadFile("../chronicles/shaders/vertex.dxil", &vertSize);
    void *fragCode = SDL_LoadFile("../chronicles/shaders/fragment.dxil", &fragSize);
    
    SDL_GPUShaderCreateInfo vertInfo = {};
    vertInfo.code                    = (Uint8*)vertCode;
    vertInfo.code_size               = vertSize;
    vertInfo.entrypoint              = "main";
    vertInfo.format                  = SDL_GPU_SHADERFORMAT_DXIL;
    vertInfo.stage                   = SDL_GPU_SHADERSTAGE_VERTEX;
    vertInfo.num_uniform_buffers     = 1;
    SDL_GPUShader *vertShader        = SDL_CreateGPUShader(gGPUDevice, &vertInfo);
    
    
    SDL_GPUShaderCreateInfo fragInfo = {};
    fragInfo.code                    = (Uint8*)fragCode;
    fragInfo.code_size               = fragSize;
    fragInfo.entrypoint              = "main";
    fragInfo.format                  = SDL_GPU_SHADERFORMAT_DXIL;
    fragInfo.stage                   = SDL_GPU_SHADERSTAGE_FRAGMENT;
    fragInfo.num_uniform_buffers     = 1;
    SDL_GPUShader *fragShader        = SDL_CreateGPUShader(gGPUDevice, &fragInfo);
    
    // Describe Vertex layout
    SDL_GPUVertexBufferDescription vbDesc = { 0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
    SDL_GPUVertexAttribute attrs[] = {
        { 0,0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, pos)     },
        { 1,0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, normal)  },
        { 2,0, SDL_GPU_VERTEXELEMENTFORMAT_INT4,   offsetof(Vertex, joints)  },
        { 3,0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex, weights) },
    };
    
    SDL_GPUGraphicsPipelineCreateInfo pipeInfo             = {};
    pipeInfo.vertex_shader                                 = vertShader;
    pipeInfo.fragment_shader                               = fragShader;
    pipeInfo.vertex_input_state.vertex_buffer_descriptions = &vbDesc;
    pipeInfo.vertex_input_state.num_vertex_buffers         = 1;
    pipeInfo.vertex_input_state.vertex_attributes          = attrs;
    pipeInfo.vertex_input_state.num_vertex_attributes      = 4;
    
    // enable depth testing to replace gZBuffer
    pipeInfo.depth_stencil_state.enable_depth_test         = true;
    pipeInfo.depth_stencil_state.enable_depth_write        = true;
    pipeInfo.depth_stencil_state.compare_op                = SDL_GPU_COMPAREOP_LESS;
    
    SDL_GPUTextureCreateInfo depthInfo                     = {};
    depthInfo.type                                         = SDL_GPU_TEXTURETYPE_2D;
    depthInfo.format                                       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    depthInfo.usage                                        = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depthInfo.width                                        = WINDOW_WIDTH;
    depthInfo.height                                       = WINDOW_HEIGHT;
    depthInfo.layer_count_or_depth                         = 1;
    depthInfo.num_levels                                   = 1;
    gDepthTexture                                          = SDL_CreateGPUTexture(gGPUDevice, &depthInfo);
    
    gPipeline = SDL_CreateGPUGraphicsPipeline(gGPUDevice, &pipeInfo);
    
    // can release shaders after pipeline is created
    SDL_ReleaseGPUShader(gGPUDevice, vertShader);
    SDL_ReleaseGPUShader(gGPUDevice, fragShader);
    
    debug_model_bounds(&gModel.mesh);
    
    pose_init(&gPose, &gModel.skeleton);
    
    gDone = 0;
#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(loop, 0, 1);
#else
    while (!gDone)
    {
        loop();
        
    }
#endif
    
    // NOTE(trist007): Software Rendering
    //SDL_DestroyTexture(gSDLTexture);
    //SDL_DestroyRenderer(gSDLRenderer);
    
    SDL_ReleaseGPUBuffer(gGPUDevice, gVertexBuffer);
    SDL_ReleaseGPUBuffer(gGPUDevice, gIndexBuffer);
    SDL_ReleaseGPUTexture(gGPUDevice, gDepthTexture);
    SDL_DestroyGPUDevice(gGPUDevice);
    
    SDL_DestroyWindow(gSDLWindow);
    SDL_Quit();
    
    return 0;
}