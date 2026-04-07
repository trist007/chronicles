#include "shared.h"

void
LoadFontAtlas(font_atlas *atlas)
{
    TTF_Font *font                                         = TTF_OpenFont("../chronicles/fonts/liberation-mono.ttf", 24);
    
    // First pass - measure total atlas width and height
    int atlasWidth                                         = 0;
    int atlasHeight                                        = 0;
    
    for(int i                                              = 32; i < 127; i++)
    {
        char str[2]                                        = { (char)i, 0 };
        int w, h;
        TTF_GetStringSize(font, str, 0, &w, &h);
        atlasWidth  += w;
        atlasHeight                                        = h > atlasHeight ? h : atlasHeight;
    }
    
    atlas->atlasWidth                                      = atlasWidth;
    atlas->atlasHeight                                     = atlasHeight;
    atlas->glyphHeight                                     = atlasHeight;
    
    // Allocate CPU surface for the atlas
    SDL_Surface *atlasSurface                              = SDL_CreateSurface(atlasWidth, atlasHeight, SDL_PIXELFORMAT_ABGR8888);
    SDL_FillSurfaceRect(atlasSurface, NULL, 0x00000000);  // clear to transparent
    
    // Second pass - render each glyph and blit into atlas surface
    SDL_Color white                                        = {255, 255, 255, 255};
    int xOffset                                            = 0;
    
    for(int i                                              = 32; i < 127; i++)
    {
        char str[2]                                        = { (char)i, 0 };
        SDL_Surface *glyph                                 = TTF_RenderText_Blended(font, str, 0, white);
        SDL_Surface *glyphRGBA                             = SDL_ConvertSurface(glyph, SDL_PIXELFORMAT_ABGR8888);
        SDL_DestroySurface(glyph);
        
        // Blit into atlas
        SDL_Rect dst                                       = { xOffset, 0, glyphRGBA->w, glyphRGBA->h };
        SDL_BlitSurface(glyphRGBA, NULL, atlasSurface, &dst);
        
        // Store UV coordinates
        atlas->glyphs[i].u0                                = (float)xOffset       / atlasWidth;
        atlas->glyphs[i].v0                                = 0.0f;
        atlas->glyphs[i].u1                                = (float)(xOffset + glyphRGBA->w) / atlasWidth;
        atlas->glyphs[i].v1                                = 1.0f;
        atlas->glyphs[i].width                             = glyphRGBA->w;
        atlas->glyphs[i].height                            = glyphRGBA->h;
        
        xOffset += glyphRGBA->w;
        SDL_DestroySurface(glyphRGBA);
    }
    
    TTF_CloseFont(font);
    
    // Upload atlas surface to GPU texture
    Uint32 dataSize                                        = atlasWidth * atlasHeight * 4;
    
    SDL_GPUTextureCreateInfo texInfo                       = {};
    texInfo.type                                           = SDL_GPU_TEXTURETYPE_2D;
    texInfo.format                                         = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    texInfo.usage                                          = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    texInfo.width                                          = atlasWidth;
    texInfo.height                                         = atlasHeight;
    texInfo.layer_count_or_depth                           = 1;
    texInfo.num_levels                                     = 1;
    atlas->texture                                         = SDL_CreateGPUTexture(gGPUDevice, &texInfo);
    
    SDL_GPUTransferBufferCreateInfo tbInfo                 = {};
    tbInfo.usage                                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbInfo.size                                            = dataSize;
    SDL_GPUTransferBuffer *tb                              = SDL_CreateGPUTransferBuffer(gGPUDevice, &tbInfo);
    void *ptr                                              = SDL_MapGPUTransferBuffer(gGPUDevice, tb, false);
    memcpy(ptr, atlasSurface->pixels, dataSize);
    SDL_UnmapGPUTransferBuffer(gGPUDevice, tb);
    
    SDL_GPUCommandBuffer *cmd                              = SDL_AcquireGPUCommandBuffer(gGPUDevice);
    SDL_GPUCopyPass      *cp                               = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTextureTransferInfo src                         = {};
    src.transfer_buffer                                    = tb;
    SDL_GPUTextureRegion dstRegion                         = {};
    dstRegion.texture                                      = atlas->texture;
    dstRegion.w                                            = atlasWidth;
    dstRegion.h                                            = atlasHeight;
    dstRegion.d                                            = 1;
    SDL_UploadToGPUTexture(cp, &src, &dstRegion, false);
    SDL_EndGPUCopyPass(cp);
    SDL_SubmitGPUCommandBuffer(cmd);
    SDL_ReleaseGPUTransferBuffer(gGPUDevice, tb);
    SDL_DestroySurface(atlasSurface);
    
    // Sampler
    SDL_GPUSamplerCreateInfo samplerInfo                   = {};
    samplerInfo.min_filter                                 = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter                                 = SDL_GPU_FILTER_LINEAR;
    samplerInfo.address_mode_u                             = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v                             = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    atlas->sampler                                         = SDL_CreateGPUSampler(gGPUDevice, &samplerInfo);
    
    // Pipeline - reuse your font shaders with blending
    SDL_GPUShader *vert                                    = CreateShader("../chronicles/shaders/font_vertex.dxil",
                                                                          SDL_GPU_SHADERSTAGE_VERTEX, 1, 0);
    SDL_GPUShader *frag                                    = CreateShader("../chronicles/shaders/font_fragment.dxil",
                                                                          SDL_GPU_SHADERSTAGE_FRAGMENT, 1, 1); // 1 uniform, 1 sampler
    
    SDL_GPUColorTargetDescription colorDesc                = {};
    colorDesc.format                                       = SDL_GetGPUSwapchainTextureFormat(gGPUDevice, gSDLWindow);
    colorDesc.blend_state.enable_blend                     = true;
    colorDesc.blend_state.src_color_blendfactor            = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    colorDesc.blend_state.dst_color_blendfactor            = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorDesc.blend_state.color_blend_op                   = SDL_GPU_BLENDOP_ADD;
    colorDesc.blend_state.src_alpha_blendfactor            = SDL_GPU_BLENDFACTOR_ONE;
    colorDesc.blend_state.dst_alpha_blendfactor            = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    colorDesc.blend_state.alpha_blend_op                   = SDL_GPU_BLENDOP_ADD;
    
    SDL_GPUGraphicsPipelineCreateInfo pipeInfo             = {};
    pipeInfo.vertex_shader                                 = vert;
    pipeInfo.fragment_shader                               = frag;
    pipeInfo.primitive_type                                = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeInfo.target_info.color_target_descriptions         = &colorDesc;
    pipeInfo.target_info.num_color_targets                 = 1;
    
    // max 256 characters per draw call
    SDL_GPUBufferCreateInfo vbInfo                         = {};
    vbInfo.usage                                           = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbInfo.size                                            = 256 * 6 * sizeof(font_vertex);
    atlas->vertexBuffer                                    = SDL_CreateGPUBuffer(gGPUDevice, &vbInfo);
    
    SDL_GPUVertexBufferDescription vbDesc                  = { 0, sizeof(font_vertex), SDL_GPU_VERTEXINPUTRATE_VERTEX, 0 };
    SDL_GPUVertexAttribute attrs[]                         = {
        { 0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(font_vertex, x) },  // position
        { 1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(font_vertex, u) },  // uv
    };
    
    pipeInfo.vertex_input_state.vertex_buffer_descriptions = &vbDesc;
    pipeInfo.vertex_input_state.num_vertex_buffers         = 1;
    pipeInfo.vertex_input_state.vertex_attributes          = attrs;
    pipeInfo.vertex_input_state.num_vertex_attributes      = 2;
    
    atlas->pipeline                                        = SDL_CreateGPUGraphicsPipeline(gGPUDevice, &pipeInfo);
    
    SDL_ReleaseGPUShader(gGPUDevice, vert);
    SDL_ReleaseGPUShader(gGPUDevice, frag);
}

void
DrawText(font_atlas *atlas, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain,
         const char *text, float x, float y, float red, float green, float blue)
{
    int len                                = 0;
    while(text[len]) len++;
    
    if(len == 0) return;
    
    // Build quads on CPU - 6 verts per character
    font_vertex *verts                     = (font_vertex *)alloca(len * 6 * sizeof(font_vertex));
    int vertCount                          = 0;
    
    float cursorX                          = x;
    float cursorY                          = y;
    
    for(int i                              = 0;
        i < len;
        i++)
    {
        int c                              = (int)text[i];
        if(c < 32 || c > 126) continue;
        
        glyph *g                           = &atlas->glyphs[c];
        
        // convert pixel coords to normalized device coords -1 to 1
        float x0                           = (cursorX                / gWINDOW_WIDTH)  * 2.0f - 1.0f;
        float y0                           = 1.0f - (cursorY / gWINDOW_HEIGHT) * 2.0f;
        float x1                           = ((cursorX + g->width)   / gWINDOW_WIDTH)  * 2.0f - 1.0f;
        float y1                           = 1.0f - ((cursorY + g->height) / gWINDOW_HEIGHT) * 2.0f;
        
        // triangle 1
        verts[vertCount++]                 = { x0, y0, g->u0, g->v0 };  // top left
        verts[vertCount++]                 = { x1, y0, g->u1, g->v0 };  // top right
        verts[vertCount++]                 = { x0, y1, g->u0, g->v1 };  // bottom left
        
        // triangle 2
        verts[vertCount++]                 = { x1, y0, g->u1, g->v0 };  // top right
        verts[vertCount++]                 = { x1, y1, g->u1, g->v1 };  // bottom right
        verts[vertCount++]                 = { x0, y1, g->u0, g->v1 };  // bottom left
        
        cursorX += g->width;
    }
    
    // Upload verts
    SDL_GPUTransferBufferCreateInfo tbInfo = {};
    tbInfo.usage                           = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbInfo.size                            = vertCount * sizeof(font_vertex);
    SDL_GPUTransferBuffer *tb              = SDL_CreateGPUTransferBuffer(gGPUDevice, &tbInfo);
    void *ptr                              = SDL_MapGPUTransferBuffer(gGPUDevice, tb, false);
    memcpy(ptr, verts, vertCount * sizeof(font_vertex));
    SDL_UnmapGPUTransferBuffer(gGPUDevice, tb);
    
    SDL_GPUCopyPass *cp                    = SDL_BeginGPUCopyPass(cmd);
    SDL_GPUTransferBufferLocation src      = { tb, 0 };
    SDL_GPUBufferRegion dst                = { atlas->vertexBuffer, 0, (Uint32)(vertCount * sizeof(font_vertex)) };
    SDL_UploadToGPUBuffer(cp, &src, &dst, false);
    SDL_EndGPUCopyPass(cp);
    SDL_ReleaseGPUTransferBuffer(gGPUDevice, tb);
    
    // Draw
    SDL_GPUColorTargetInfo color           = {};
    color.texture                          = swapchain;
    color.load_op                          = SDL_GPU_LOADOP_LOAD;
    color.store_op                         = SDL_GPU_STOREOP_STORE;
    
    SDL_GPURenderPass *pass                = SDL_BeginGPURenderPass(cmd, &color, 1, NULL);
    SDL_BindGPUGraphicsPipeline(pass, atlas->pipeline);
    
    // Set color
    float col[4]                           = { red, green, blue, 1.0f };
    SDL_PushGPUFragmentUniformData(cmd, 0, col, sizeof(col));
    
    SDL_GPUTextureSamplerBinding binding   = { atlas->texture, atlas->sampler };
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
    
    SDL_GPUBufferBinding vb                = { atlas->vertexBuffer, 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    
    SDL_DrawGPUPrimitives(pass, vertCount, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}