#include "shared.h"

extern const int gWINDOW_WIDTH;
extern const int gWINDOW_HEIGHT;

// NOTE(trist007): on my shaders I need to use column vectors so mul(matrix, vector) is always what
// I want return mul(mvp, float4(pos, 1.0)); so mvp is world space -> camera space -> clip space

// ################################################################################
// Renderer
// ################################################################################

int
RendererInit(GameState *gamestate)
{
    // Create GPUDevice for hardware acceleration
    /*
    SDL_GPUDevice * SDL_CreateGPUDevice(
                                        SDL_GPUShaderFormat format_flags,
                                        bool debug_mode,
                                        const char *name);
*/
    
    SDL_SetHint("SDL_DIRECT3D12_DEBUG", "1");
    gGPUDevice = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV | SDL_GPU_SHADERFORMAT_DXIL | SDL_GPU_SHADERFORMAT_MSL,
                                     true, NULL);
    if(!gGPUDevice)
    {
        SDL_Log("GPU device is NULL: %s\n", SDL_GetError());
        return(-1);
    }
    
    SDL_Log("GPU device ok: %p\n", (void*)gGPUDevice);
    
    SDL_GPUShaderFormat format = SDL_GetGPUShaderFormats(gGPUDevice);
    
    if(format & SDL_GPU_SHADERFORMAT_DXIL)
    {
        SDL_Log("DXIL (D3D12)\n");
        gamestate->graphicsAPI = "DirectX 12";
    }
    else if(format & SDL_GPU_SHADERFORMAT_SPIRV)
    {
        SDL_Log("SPRIV (Vulkan)\n");
        gamestate->graphicsAPI = "Vulkan";
    }
    else if(format & SDL_GPU_SHADERFORMAT_MSL)
    {
        SDL_Log("MSL (Metal)\n");
        gamestate->graphicsAPI = "Metal";
    }
    
    SDL_ClaimWindowForGPUDevice(gGPUDevice, gSDLWindow);
    
    return(0);
}


void
LoadBackground(Background *b)
{
    const char *basePath = SDL_GetBasePath();
    SDL_Log("cwd: %s", basePath);
    
    // Background
    SDL_Surface *bg = IMG_Load("../chronicles/data/textures/background.png");
    if(!bg)
    {
        SDL_Log("IMG_Load failed: %s", SDL_GetError());
        return;
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
    
    
    b->backgroundTexture               = SDL_CreateGPUTexture(gGPUDevice, &bgTexInfo);
    
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
    bgDst.texture                            = b->backgroundTexture;
    bgDst.w                                  = bgRGBA->w;
    bgDst.h                                  = bgRGBA->h;
    bgDst.d                                  = 1;
    
    SDL_UploadToGPUTexture(bgCp, &bgSrc, &bgDst, false);
    SDL_EndGPUCopyPass(bgCp);
    SDL_SubmitGPUCommandBuffer(bgCmd);
    SDL_ReleaseGPUTransferBuffer(gGPUDevice, bgTb);
    SDL_DestroySurface(bgRGBA);
    
    // Create sampler
    SDL_GPUSamplerCreateInfo samplerInfo             = {};
    samplerInfo.min_filter                           = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter                           = SDL_GPU_FILTER_LINEAR;
    samplerInfo.address_mode_u                       = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v                       = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    b->backgroundSampler                             = SDL_CreateGPUSampler(gGPUDevice, &samplerInfo);
    
    // Load background shader bytecode
    size_t bgvertSize, bgfragSize;
    void *bgvertCode                                 = SDL_LoadFile("../chronicles/shaders/bg_vertex.dxil", &bgvertSize);
    void *bgfragCode                                 = SDL_LoadFile("../chronicles/shaders/bg_fragment.dxil", &bgfragSize);
    SDL_Log("bgvertCode: %p\n", bgvertCode);
    SDL_Log("bgfragCode: %p\n", bgfragCode);
    
    SDL_GPUShaderCreateInfo bgvertInfo               = {};
    bgvertInfo.code                                  = (Uint8*)bgvertCode;
    bgvertInfo.code_size                             = bgvertSize;
    bgvertInfo.entrypoint                            = "main";
    bgvertInfo.format                                = SDL_GPU_SHADERFORMAT_DXIL;
    bgvertInfo.stage                                 = SDL_GPU_SHADERSTAGE_VERTEX;
    bgvertInfo.num_uniform_buffers                   = 0;
    SDL_GPUShader *bgvertShader                      = SDL_CreateGPUShader(gGPUDevice, &bgvertInfo);
    SDL_Log("bgvertShader: %p error: %s", (void*)bgvertShader, SDL_GetError());
    
    
    
    SDL_GPUShaderCreateInfo bgfragInfo               = {};
    bgfragInfo.code                                  = (Uint8*)bgfragCode;
    bgfragInfo.code_size                             = bgfragSize;
    bgfragInfo.entrypoint                            = "main";
    bgfragInfo.format                                = SDL_GPU_SHADERFORMAT_DXIL;
    bgfragInfo.stage                                 = SDL_GPU_SHADERSTAGE_FRAGMENT;
    bgfragInfo.num_uniform_buffers                   = 1;
    bgfragInfo.num_samplers                          = 1;
    bgfragInfo.num_storage_textures                  = 0;
    bgfragInfo.num_storage_buffers                   = 0;
    
    SDL_GPUShader *bgfragShader                      = SDL_CreateGPUShader(gGPUDevice, &bgfragInfo);
    SDL_Log("bgfragShader: %p error: %s", (void*)bgfragShader, SDL_GetError());
    
    //SDL_Log("GPU driver: %s", SDL_GetGPUDeviceDriver(gpuDevice));
    
    
    
    SDL_GPUGraphicsPipelineCreateInfo bgPipeInfo     = {};
    bgPipeInfo.vertex_shader                         = bgvertShader;
    bgPipeInfo.fragment_shader                       = bgfragShader;
    // no vertex input state - fullscreen triangle generated in shader
    // no depth stencil state - background doesn't need depth
    
    SDL_GPUColorTargetDescription bgColorDesc        = {};
    bgColorDesc.format                               = SDL_GetGPUSwapchainTextureFormat(gGPUDevice, gSDLWindow);
    bgPipeInfo.target_info.color_target_descriptions = &bgColorDesc;
    bgPipeInfo.target_info.num_color_targets         = 1;
    
    b->backgroundPipeline = SDL_CreateGPUGraphicsPipeline(gGPUDevice, &bgPipeInfo);
    SDL_Log("bgPipeline: %p", (void*)b->backgroundPipeline);
    if(!b->backgroundPipeline)
    {
        SDL_Log("bgPipeline failed: %s", SDL_GetError());
    }
    
    // can release shaders after pipeline is created
    SDL_ReleaseGPUShader(gGPUDevice, bgvertShader);
    SDL_ReleaseGPUShader(gGPUDevice, bgfragShader);
    
    SDL_free(bgvertCode);
    SDL_free(bgfragCode);
}

void pose_alloc(Pose *pose, Skeleton *skel)
{
    pose->jointCount = skel->jointCount;
    pose->joints     = (Transform*)arenaAlloc(&gArena, (pose->jointCount * sizeof(Transform)));
    pose->matrices   = (float*)arenaAlloc(&gArena, (pose->jointCount * 16 * sizeof(float)));
}

void
LoadRoom(const char *path, Model *m, GameState *gamestate)
{
    load_room(m, path);
    
    // GPU upload - identical to LoadModel
    SDL_GPUBufferCreateInfo vbInfo = {};
    vbInfo.usage                   = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbInfo.size                    = m->mesh.vertCount * sizeof(Vertex);
    m->vertexBuffer                = SDL_CreateGPUBuffer(gGPUDevice, &vbInfo);
    
    SDL_GPUBufferCreateInfo ibInfo = {};
    ibInfo.usage                   = SDL_GPU_BUFFERUSAGE_INDEX;
    ibInfo.size                    = m->mesh.triCount * 3 * sizeof(int);
    m->indexBuffer                 = SDL_CreateGPUBuffer(gGPUDevice, &ibInfo);
    
    SDL_GPUTransferBufferCreateInfo tbInfo = {};
    tbInfo.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbInfo.size                            = vbInfo.size + ibInfo.size;
    SDL_GPUTransferBuffer *tb              = SDL_CreateGPUTransferBuffer(gGPUDevice, &tbInfo);
    
    void *ptr = SDL_MapGPUTransferBuffer(gGPUDevice, tb, false);
    memcpy(ptr, m->mesh.verts, vbInfo.size);
    memcpy((char*)ptr + vbInfo.size, m->mesh.tris, ibInfo.size);
    SDL_UnmapGPUTransferBuffer(gGPUDevice, tb);
    
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gGPUDevice);
    SDL_GPUCopyPass *cp       = SDL_BeginGPUCopyPass(cmd);
    
    SDL_GPUTransferBufferLocation src  = { tb, 0 };
    SDL_GPUBufferRegion dst            = { m->vertexBuffer, 0, vbInfo.size };
    SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    
    SDL_GPUTransferBufferLocation src2 = { tb, vbInfo.size };
    SDL_GPUBufferRegion dst2           = { m->indexBuffer, 0, ibInfo.size };
    SDL_UploadToGPUBuffer(cp, &src2, &dst2, false);
    
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(gGPUDevice, tb);
    
    // Room shaders - no skinning so vertex uniform is just MVP (16 floats)
    SDL_GPUShader *room_vert = CreateShader("../chronicles/shaders/room_vertex.dxil",
                                            SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *room_frag = CreateShader("../chronicles/shaders/room_fragment.dxil",
                                            SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 0);
    
    // Room vertex layout - pos + normal only (no joints/weights)
    SDL_GPUVertexBufferDescription vbDesc = { 0, sizeof(Vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
    SDL_GPUVertexAttribute attrs[] = {
        { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, pos)    },
        { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, offsetof(Vertex, normal) },
        { 2, 0, SDL_GPU_VERTEXELEMENTFORMAT_INT3,   offsetof(Vertex, joints) },
        { 3, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT4, offsetof(Vertex, weights) },
    };
    
    SDL_GPUTextureCreateInfo depthInfo     = {};
    depthInfo.type                         = SDL_GPU_TEXTURETYPE_2D;
    depthInfo.format                       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    depthInfo.usage                        = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depthInfo.width                        = gWINDOW_WIDTH;
    depthInfo.height                       = gWINDOW_HEIGHT;
    depthInfo.layer_count_or_depth         = 1;
    depthInfo.num_levels                   = 1;
    m->depthTexture                        = SDL_CreateGPUTexture(gGPUDevice, &depthInfo);
    
    m->pipeline = CreatePipeline(room_vert, room_frag, &vbDesc, 1, attrs, 4,
                                 SDL_GPU_PRIMITIVETYPE_TRIANGLELIST, true);
    SDL_Log("room pipeline: %p error: %s", (void*)m->pipeline, SDL_GetError());
    
    
    SDL_ReleaseGPUShader(gGPUDevice, room_vert);
    SDL_ReleaseGPUShader(gGPUDevice, room_frag);
    
    // uniform buffer just needs MVP (16 floats) - no skinning
    // m->uniformBuffer = (float*)arenaAlloc(&gArena, 16 * sizeof(float));
    int jointCount = gamestate->model.skeleton.jointCount;
    m->uniformBuffer = (float*)arenaAlloc(&gArena, (16 + jointCount * 16) * sizeof(float));
    mat4_identity(m->uniformBuffer + 16);  // dummy joint
    m->jointCount = jointCount;
}

void
RenderRoom(Model *m, Vec3 *lightPos, SDL_GPUCommandBuffer *cmd,
           SDL_GPUTexture *swapchain, float *vp)
{
    float ambientLight = 0.15f;
    float lightRadius  = 15.0f;
    
    // rotate the room clockwise by 45 degrees
    float angle = -PI32 / 4.0f; // -45 degrees
    float c = cosf(angle);
    float s = sinf(angle);
    
    // Y-axis rotation, column major
    float model[16] = {
        c,  0, -s, 0,
        0,  1, 0, 0,
        s,  0, c, 0,
        0,  0, 0, 1,
    };
    
    // MVP = vp * model  (your note says mul(matrix, vector), column-major)
    float mvp[16];
    mat4_mul(mvp, vp, model);  // whatever your mat4 multiply is named
    
    SDL_GPUColorTargetInfo color = {};
    color.texture                = swapchain;
    color.load_op                = SDL_GPU_LOADOP_CLEAR;   // replaces RenderBackground
    color.clear_color            = {0, 0, 0, 1};
    color.store_op               = SDL_GPU_STOREOP_STORE;
    
    SDL_GPUDepthStencilTargetInfo depth = {};
    depth.texture                       = m->depthTexture;
    depth.load_op                       = SDL_GPU_LOADOP_CLEAR;
    depth.clear_depth                   = 1.0f;
    depth.store_op                      = SDL_GPU_STOREOP_DONT_CARE;
    
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &color, 1, &depth);
    SDL_BindGPUGraphicsPipeline(pass, m->pipeline);
    
    SDL_GPUBufferBinding vb = { m->vertexBuffer, 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    SDL_GPUBufferBinding ib = { m->indexBuffer, 0 };
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    
    // vertex uniform - just VP, no skinning
    //memcpy(m->uniformBuffer, vp, 64);
    //SDL_PushGPUVertexUniformData(cmd, 0, m->uniformBuffer, 64);
    //int uniformSize = (16 + m->jointCount * 16) * sizeof(float);
    //memcpy(m->uniformBuffer, vp, 64);
    //SDL_PushGPUVertexUniformData(cmd, 0, m->uniformBuffer, uniformSize);
    
    SDL_PushGPUVertexUniformData(cmd, 0, mvp, 64);
    
    for(int i = 0; i < m->mesh.primitiveCount; i++)
    {
        Primitive *prim = &m->mesh.primitives[i];
        float r = ((prim->color >>  0) & 0xff) / 255.0f;
        float g = ((prim->color >>  8) & 0xff) / 255.0f;
        float b = ((prim->color >> 16) & 0xff) / 255.0f;
        float col[12] = { r, g, b, 1.0f,
            lightPos->x, lightPos->y, lightPos->z, ambientLight,
            lightRadius, 0.0f, 0.0f, 0.0f };
        SDL_PushGPUFragmentUniformData(cmd, 0, col, sizeof(col));
        SDL_DrawGPUIndexedPrimitives(pass, prim->triCount * 3, 1, prim->triOffset * 3, 0, 0);
    }
    
    SDL_EndGPURenderPass(pass);
}

void
LoadModel(const char *path, Model *m)
{
    load_model(m, path);
    
    pose_alloc(&m->pose, &m->skeleton);
    SDL_Log("primitiveCount after load: %d", m->mesh.primitiveCount);
    SDL_Log("vertCount: %d  triCount: %d", m->mesh.vertCount, m->mesh.triCount);
    
    // Upload mesh to GPU buffers
    
    // vertex buffer
    SDL_GPUBufferCreateInfo vbInfo         = {};
    vbInfo.usage                           = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbInfo.size                            = m->mesh.vertCount * sizeof(Vertex);
    m->vertexBuffer                        = SDL_CreateGPUBuffer(gGPUDevice, &vbInfo);
    
    // index buffer
    SDL_GPUBufferCreateInfo ibInfo         = {};
    ibInfo.usage                           = SDL_GPU_BUFFERUSAGE_INDEX;
    ibInfo.size                            = m->mesh.triCount * 3 * sizeof(int);
    m->indexBuffer                         = SDL_CreateGPUBuffer(gGPUDevice, &ibInfo);
    
    // Upload transfer buffer
    SDL_GPUTransferBufferCreateInfo tbInfo = {};
    tbInfo.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbInfo.size                            = vbInfo.size + ibInfo.size;
    SDL_GPUTransferBuffer *tb              = SDL_CreateGPUTransferBuffer(gGPUDevice, &tbInfo);
    
    void *ptr = SDL_MapGPUTransferBuffer(gGPUDevice, tb, false);
    memcpy(ptr, m->mesh.verts, vbInfo.size);
    memcpy((char*)ptr + vbInfo.size, m->mesh.tris, ibInfo.size);
    SDL_UnmapGPUTransferBuffer(gGPUDevice, tb);
    
    // Record the upload into a command buffer
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gGPUDevice);
    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src = { tb, 0 };
    SDL_GPUBufferRegion dst = { m->vertexBuffer, 0, vbInfo.size };
    SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    
    SDL_GPUTransferBufferLocation src2 = { tb, vbInfo.size };
    SDL_GPUBufferRegion dst2 = { m->indexBuffer, 0, ibInfo.size };
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
    
    SDL_GPUColorTargetDescription colorDesc                = {};
    colorDesc.format                                       = SDL_GetGPUSwapchainTextureFormat(gGPUDevice, gSDLWindow);
    pipeInfo.target_info.color_target_descriptions         = &colorDesc;
    pipeInfo.target_info.num_color_targets                 = 1;
    pipeInfo.target_info.depth_stencil_format              = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    pipeInfo.target_info.has_depth_stencil_target          = true;
    
    SDL_GPUTextureCreateInfo depthInfo                     = {};
    depthInfo.type                                         = SDL_GPU_TEXTURETYPE_2D;
    depthInfo.format                                       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
    depthInfo.usage                                        = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    depthInfo.width                                        = gWINDOW_WIDTH;
    depthInfo.height                                       = gWINDOW_HEIGHT;
    depthInfo.layer_count_or_depth                         = 1;
    depthInfo.num_levels                                   = 1;
    m->depthTexture                                        = SDL_CreateGPUTexture(gGPUDevice, &depthInfo);
    
    SDL_Log("vert: %p  frag: %p", (void*)vertCode, (void*)fragCode);
    if(!vertCode || !fragCode)
    {
        SDL_Log("room shaders are null, aborting");
        return;
    }
    m->pipeline = SDL_CreateGPUGraphicsPipeline(gGPUDevice, &pipeInfo);
    
    // can release shaders after pipeline is created
    SDL_ReleaseGPUShader(gGPUDevice, vertShader);
    SDL_ReleaseGPUShader(gGPUDevice, fragShader);
    
    SDL_free(vertCode);
    SDL_free(fragCode);
}

void
RendererDestroy(Background *b, Model *m, pipelineObjects *po)
{
    SDL_ReleaseGPUBuffer(gGPUDevice, m->vertexBuffer);
    SDL_ReleaseGPUBuffer(gGPUDevice, m->indexBuffer);
    SDL_ReleaseGPUTexture(gGPUDevice, m->depthTexture);
    SDL_ReleaseGPUTexture(gGPUDevice, b->backgroundTexture);
    SDL_ReleaseGPUSampler(gGPUDevice, b->backgroundSampler);
    SDL_ReleaseGPUTransferBuffer(gGPUDevice, po->lp.transferBuffer);
    SDL_ReleaseGPUBuffer(gGPUDevice, po->lp.LineVertexBuffer);     
    SDL_ReleaseGPUGraphicsPipeline(gGPUDevice, po->lp.Pipeline);   
    SDL_ReleaseGPUGraphicsPipeline(gGPUDevice, b->backgroundPipeline);
    SDL_ReleaseGPUGraphicsPipeline(gGPUDevice, m->pipeline);
    SDL_DestroyGPUDevice(gGPUDevice);
}

// ################################################################################
// Background
// ################################################################################

void
RenderBackground(Background *b, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain,
                 Vec3 *lightPos, float lightRadius)
{
    
    SDL_GPUColorTargetInfo bgColor = {};
    bgColor.texture     = swapchain;
    bgColor.load_op     = SDL_GPU_LOADOP_CLEAR;
    bgColor.clear_color = {0,0,0,1};
    bgColor.store_op    = SDL_GPU_STOREOP_STORE;
    
    SDL_GPURenderPass *bgPass = SDL_BeginGPURenderPass(cmd, &bgColor, 1, NULL);
    SDL_BindGPUGraphicsPipeline(bgPass, b->backgroundPipeline);
    SDL_GPUTextureSamplerBinding bgBinding = { b->backgroundTexture, b->backgroundSampler };
    SDL_BindGPUFragmentSamplers(bgPass, 0, &bgBinding, 1);
    
    float ambientLight = 0.3f;
    float bgLightData[5] = {
        ambientLight,
        lightPos->x, lightPos->y, lightPos->z,
        lightRadius
    };
    
    SDL_PushGPUFragmentUniformData(cmd, 0, &ambientLight, sizeof(float));  // here
    
    SDL_DrawGPUPrimitives(bgPass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(bgPass);
}

void
RenderModel(Model *m, Player *p, Vec3 *lightPos, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain, float *mvp)
{
    // Add a point light at the lightPos
    float ambientLight = 0.8f;
    //float lightPos[3] = { 3.3f, 0.0f, 1.0f };
    float lightRadius = 10.0f;
    
    SDL_GPUColorTargetInfo modelColor = {};
    modelColor.texture  = swapchain;
    modelColor.load_op  = SDL_GPU_LOADOP_LOAD;  // keep background
    modelColor.store_op = SDL_GPU_STOREOP_STORE;
    
    SDL_GPUDepthStencilTargetInfo depth = {};
    depth.texture      = m->depthTexture;
    depth.load_op      = SDL_GPU_LOADOP_CLEAR;
    depth.clear_depth  = 1.0f;
    depth.store_op     = SDL_GPU_STOREOP_DONT_CARE;
    
    SDL_GPURenderPass *pass = SDL_BeginGPURenderPass(cmd, &modelColor, 1, &depth);
    SDL_BindGPUGraphicsPipeline(pass, m->pipeline);
    
    SDL_GPUBufferBinding vb = { m->vertexBuffer, 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    SDL_GPUBufferBinding ib = { m->indexBuffer, 0 };
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    
    int jointCount    = m->skeleton.jointCount;
    int uniformSize   = (16 + jointCount * 16) * sizeof(float);
    memcpy(m->uniformBuffer,      mvp,            64);
    memcpy(m->uniformBuffer + 16, m->pose.matrices, jointCount * 64);
    
    SDL_PushGPUVertexUniformData(cmd, 0, m->uniformBuffer, uniformSize);
    
    for(int i = 0; i < m->mesh.primitiveCount; i++)  // or primitiveCount if that's on the struct
    {
        Primitive *prim = &m->mesh.primitives[i];
        float red = ((prim->color >> 0)  & 0xff) / 255.0f;
        float green = ((prim->color >> 8)  & 0xff) / 255.0f;
        float blue = ((prim->color >> 16) & 0xff) / 255.0f;
        float col[9] = { red, green, blue, 1.0f, ambientLight,
            lightPos->x, lightPos->y, lightPos->z, lightRadius };
        SDL_PushGPUFragmentUniformData(cmd, 0, col, sizeof(col));
        SDL_DrawGPUIndexedPrimitives(pass, prim->triCount * 3, 1, prim->triOffset * 3, 0, 0);
    }
    
    SDL_EndGPURenderPass(pass);
}



// ################################################################################
// Draw Functions
// ################################################################################


SDL_GPUShader*
CreateShader(const char *path, SDL_GPUShaderStage stage, int numUniformBuffers, int numSamplers)
{
    size_t size;
    void *code = SDL_LoadFile(path, &size);
    
    SDL_GPUShaderCreateInfo info = {};
    info.code                    = (Uint8*)code;
    info.code_size               = size;
    info.entrypoint              = "main";
    info.format                  = SDL_GPU_SHADERFORMAT_DXIL;
    info.stage                   = stage;
    info.num_uniform_buffers     = numUniformBuffers;
    info.num_samplers            = numSamplers;
    
    SDL_GPUShader *shader = SDL_CreateGPUShader(gGPUDevice, &info);
    SDL_free(code);
    
    SDL_Log("creating shader %s uniforms=%d samplers=%d", path, numUniformBuffers, numSamplers);
    SDL_Log("shader result: %p", (void*)shader);
    
    return(shader);
}

SDL_GPUGraphicsPipeline *CreatePipeline(
                                        SDL_GPUShader *vertShader,
                                        SDL_GPUShader *fragShader,
                                        SDL_GPUVertexBufferDescription *vbDescs, int numVBDescs,
                                        SDL_GPUVertexAttribute *attrs, int numAttrs,
                                        SDL_GPUPrimitiveType topology,
                                        bool depthTest)
{
    SDL_GPUGraphicsPipelineCreateInfo pipeInfo             = {};
    pipeInfo.vertex_shader                                 = vertShader;
    pipeInfo.fragment_shader                               = fragShader;
    pipeInfo.primitive_type                                = topology;
    pipeInfo.vertex_input_state.vertex_buffer_descriptions = vbDescs;
    pipeInfo.vertex_input_state.num_vertex_buffers         = numVBDescs;
    pipeInfo.vertex_input_state.vertex_attributes          = attrs;
    pipeInfo.vertex_input_state.num_vertex_attributes      = numAttrs;
    
    if(depthTest)
    {
        pipeInfo.depth_stencil_state.enable_depth_test  = true;
        pipeInfo.depth_stencil_state.enable_depth_write = true;
        pipeInfo.depth_stencil_state.compare_op         = SDL_GPU_COMPAREOP_LESS;
        pipeInfo.target_info.depth_stencil_format       = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        pipeInfo.target_info.has_depth_stencil_target   = true;
    }
    
    SDL_GPUColorTargetDescription colorDesc = {};
    colorDesc.format = SDL_GetGPUSwapchainTextureFormat(gGPUDevice, gSDLWindow);
    pipeInfo.target_info.color_target_descriptions = &colorDesc;
    pipeInfo.target_info.num_color_targets = 1;
    
    return(SDL_CreateGPUGraphicsPipeline(gGPUDevice, &pipeInfo));
}

void
PushCircle(LinePipeline *lp, Vec3 center, float radius, int segments)
{
    for(int i = 0;
        i < segments;
        i++)
    {
        float a0 = (float)i       / segments * 2.0f * PI32;
        float a1 = (float)(i + 1) / segments * 2.0f * PI32;
        
        Vec3 p0 = { center.x + cosf(a0) * radius,
            center.y + sinf(a0) * radius,
            center.z };
        
        Vec3 p1 = { center.x + cosf(a1) * radius,
            center.y + sinf(a1) * radius,
            center.z };
        
        PushLine(lp, p0, p1);
    }
}

void
PushFilledCircle(LinePipeline *lp, Vec3 center, float radius, int segments)
{
    for(int i = 0;
        i < segments;
        i++)
    {
        float a0 = (float)i       / segments * 2.0f * PI32;
        float a1 = (float)(i + 1) / segments * 2.0f * PI32;
        
        Vec3 p0 = { center.x + cosf(a0) * radius,
            center.y + sinf(a0) * radius,
            center.z };
        
        Vec3 p1 = { center.x + cosf(a1) * radius,
            center.y + sinf(a1) * radius,
            center.z };
        
        // outer edge
        PushLine(lp, p0, p1);
        // spoke from center to edge
        PushLine(lp, center, p0);
    }
}

void
PushLine(LinePipeline *lp, Vec3 a, Vec3 b)
{
    if(lp->LineVertCount + 2 > MAX_DEBUG_LINES * 2)
        return;
    
    lp->LineVerts[lp->LineVertCount++] = {a};
    lp->LineVerts[lp->LineVertCount++] = {b};
}

void
FlushLines(LinePipeline *lp, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain, float *vp)
{
    if(lp->LineVertCount == 0)
        return;
    
    // Upload
    void *ptr                              = SDL_MapGPUTransferBuffer(gGPUDevice, lp->transferBuffer, false);
    memcpy(ptr, lp->LineVerts, lp->LineVertCount * sizeof(LineVertex));
    SDL_UnmapGPUTransferBuffer(gGPUDevice, lp->transferBuffer);
    
    SDL_GPUCopyPass *cp                    = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src      = { lp->transferBuffer, 0 };
    SDL_GPUBufferRegion dst                = { lp->LineVertexBuffer, 0, (Uint32)(lp->LineVertCount * sizeof(LineVertex)) };
    
    SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    
    // Draw
    SDL_GPUColorTargetInfo color           = {};
    color.texture                          = swapchain;
    color.load_op                          = SDL_GPU_LOADOP_LOAD;
    color.store_op                         = SDL_GPU_STOREOP_STORE;
    
    SDL_GPURenderPass *pass                = SDL_BeginGPURenderPass(cmd, &color, 1, NULL);
    
    SDL_BindGPUGraphicsPipeline(pass, lp->Pipeline);
    
    SDL_GPUBufferBinding vb                = { lp->LineVertexBuffer, 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    
    // want vp so the line can exist in 3D world space
    SDL_PushGPUVertexUniformData(cmd, 0, vp, 64);
    
    SDL_DrawGPUPrimitives(pass, lp->LineVertCount, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
    
    lp->LineVertCount = 0;
}

void
CreateLinePipeline(LinePipeline *lp)
{
    SDL_GPUTransferBufferCreateInfo tbInfo = {};
    tbInfo.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbInfo.size                            = MAX_DEBUG_LINES * 2 * sizeof(LineVertex);
    lp->transferBuffer                     = SDL_CreateGPUTransferBuffer(gGPUDevice, &tbInfo);
    
    SDL_GPUShader *vert = CreateShader("../chronicles/shaders/line_vertex.dxil",
                                       SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    
    SDL_GPUShader *frag = CreateShader("../chronicles/shaders/line_fragment.dxil",
                                       SDL_GPU_SHADERSTAGE_FRAGMENT, 0, 0);
    
    SDL_Log("line pipeline: %p error: %s", (void*)lp->Pipeline, SDL_GetError());
    
    SDL_GPUVertexBufferDescription vbDesc = { 0, sizeof(LineVertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
    SDL_GPUVertexAttribute attrs[] = {
        { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, 0 },
    };
    
    lp->Pipeline = CreatePipeline(vert, frag, &vbDesc, 1, attrs, 1, SDL_GPU_PRIMITIVETYPE_LINELIST, false);
    
    SDL_ReleaseGPUShader(gGPUDevice, vert);
    SDL_ReleaseGPUShader(gGPUDevice, frag);
    
    SDL_GPUBufferCreateInfo vbInfo = {};
    vbInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbInfo.size = MAX_DEBUG_LINES * 2 * sizeof(LineVertex);
    lp->LineVertexBuffer = SDL_CreateGPUBuffer(gGPUDevice, &vbInfo);
    
    lp->LineVertCount = 0;
}