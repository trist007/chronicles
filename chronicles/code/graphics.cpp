extern const int gWINDOW_WIDTH;
extern const int gWINDOW_HEIGHT;

extern SDL_Window                       *gSDLWindow;
extern SDL_GPUDevice                    *gGPUDevice;
extern SDL_GPUTexture                   *gDepthTexture;
extern SDL_GPUGraphicsPipeline          *gPipeline;
extern Arena                            gArena;

void *arenaAlloc(Arena *, size_t s);

struct Vec2 { float u, v; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

struct Vertex
{
    Vec3 pos;
    Vec3 normal;
    Vec2 uv;
    // skinning
    int   joints[4];
    float weights[4];
};

struct Tri { int v[3]; };

// per joint
struct Joint
{
    float inverseBindMatrix[16];
    int   parent;  // -1 if root
    char  name[64];
    Vec3  defaultTranslation;
    Vec4  defaultRotation;
    Vec3  defaultScale;
};

// animation keyframes
struct Keyframe
{
    float time;
    Vec4  value;  // vec3 for translation/scale, vec4 for rotation quaternion
};

struct AnimChannel
{
    int       jointIndex;
    int       type;        // 0=translation, 1=rotation, 2=scale
    Keyframe *keyframes;
    int       keyframeCount;
};

struct Animation
{
    char        name[64];
    AnimChannel *channels;
    int          channelCount;
    float        duration;
};

struct Primitive
{
    int vertOffset;
    int triOffset;
    int triCount;
    unsigned int color;  // ARGB from material
};

// mesh
struct Mesh
{
    Vertex    *verts;
    int        vertCount;
    Tri       *tris;
    int        triCount;
    Primitive  primitives[16];
    int        primitiveCount;
};

// skeleton
struct Skeleton
{
    Joint *joints;
    int    jointCount;
};

struct Transform
{
    Vec3 translation;
    Vec4 rotation;    // quaternion xyzw
    Vec3 scale;
};

struct Pose
{
    Transform *joints;    // local space transforms
    float     *matrices;  // final skinning matrices, jointCount * 16 floats
    int        jointCount;
};

struct Background
{
    SDL_GPUTexture          *backgroundTexture;
    SDL_GPUSampler          *backgroundSampler;
    SDL_GPUGraphicsPipeline *backgroundPipeline;
    int                     width;
    int                     height;
};

struct Model
{
    // CPU side
    Mesh                      mesh;
    Skeleton                  skeleton;
    Animation                 *animations;
    Pose                      pose;
    int                       animCount;
    
    // GPU side
    SDL_GPUBuffer             *vertexBuffer;
    SDL_GPUBuffer             *indexBuffer;
    SDL_GPUTexture            *depthTexture;  // though this arguably belongs in Renderer
    SDL_GPUGraphicsPipeline   *pipeline; // same, could belong in Renderer
};

struct Player
{
    float AnimTime  = 0.0f;
    float ModelX    = 0.0f;
    float ModelY    = -1.09f;
    float ModelZ    = 3.0f;
    float ModelRotY = 0.0f;
};

inline float min(float a, float b)
{
    return(a < b ? a: b);
}

inline float max(float a, float b)
{
    return(a > b ? a: b);
}

inline int min(int a, int b)
{
    return(a < b ? a: b);
}

inline int max(int a, int b)
{
    return(a > b ? a: b);
}

inline float min3(float a, float b, float c)
{
    return(min(min(a, b), c));
}

inline float max3(float a, float b, float c)
{
    return(max(max(a, b), c));
}

void mat4_identity(float *m)
{
    memset(m, 0, 64);
    m[0] = m[5] = m[10] = m[15] = 1.0f;
}

void mat4_copy(float *dst, const float *src)
{
    memcpy(dst, src, 64);
}

void quat_to_mat4(const Vec4 q, float *m)
{
    float x=q.x, y=q.y, z=q.z, w=q.w;
    m[ 0]=1-2*(y*y+z*z); m[ 1]=  2*(x*y+z*w); m[ 2]=  2*(x*z-y*w); m[ 3]=0;
    m[ 4]=  2*(x*y-z*w); m[ 5]=1-2*(x*x+z*z); m[ 6]=  2*(y*z+x*w); m[ 7]=0;
    m[ 8]=  2*(x*z+y*w); m[ 9]=  2*(y*z-x*w); m[10]=1-2*(x*x+y*y); m[11]=0;
    m[12]=0;             m[13]=0;             m[14]=0;             m[15]=1;
}

void mat4_mul(float *out, const float *a, const float *b)
{
    float tmp[16] = {};
    for(int i = 0; i < 4; i++)
        for(int j = 0; j < 4; j++)
        for(int k = 0; k < 4; k++)
        tmp[j*4+i] += a[k*4+i] * b[j*4+k];
    memcpy(out, tmp, 64);
}

void trs_to_mat4(const Transform *t, float *m)
{
    float r[16];
    quat_to_mat4(t->rotation, r);
    
    // apply scale to rotation columns then set translation
    m[ 0]=r[ 0]*t->scale.x; m[ 1]=r[ 1]*t->scale.x; m[ 2]=r[ 2]*t->scale.x; m[ 3]=0;
    m[ 4]=r[ 4]*t->scale.y; m[ 5]=r[ 5]*t->scale.y; m[ 6]=r[ 6]*t->scale.y; m[ 7]=0;
    m[ 8]=r[ 8]*t->scale.z; m[ 9]=r[ 9]*t->scale.z; m[10]=r[10]*t->scale.z; m[11]=0;
    m[12]=t->translation.x; m[13]=t->translation.y; m[14]=t->translation.z;  m[15]=1;
}

Vec4 quat_lerp(Vec4 a, Vec4 b, float t)
{
    // ensure shortest path
    float dot = a.x*b.x + a.y*b.y + a.z*b.z + a.w*b.w;
    if(dot < 0)
    {
        b.x=-b.x; b.y=-b.y; b.z=-b.z; b.w=-b.w;
    }
    float it = 1.0f - t;
    Vec4 r = { it*a.x+t*b.x, it*a.y+t*b.y, it*a.z+t*b.z, it*a.w+t*b.w };
    // normalize
    float len = sqrtf(r.x*r.x + r.y*r.y + r.z*r.z + r.w*r.w);
    r.x/=len; r.y/=len; r.z/=len; r.w/=len;
    return r;
}

Vec3 vec3_lerp(Vec3 a, Vec3 b, float t)
{
    return { a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t, a.z+(b.z-a.z)*t };
}

void mat4_mul_vec3(const float *m, Vec3 v, Vec3 *out)
{
    out->x = m[0]*v.x + m[4]*v.y + m[ 8]*v.z + m[12];
    out->y = m[1]*v.x + m[5]*v.y + m[ 9]*v.z + m[13];
    out->z = m[2]*v.x + m[6]*v.y + m[10]*v.z + m[14];
}

void read_float_n(cgltf_accessor *acc, int index, float *out, int n)
{
    cgltf_accessor_read_float(acc, index, out, n);
}

void read_uint(cgltf_accessor *acc, int index, unsigned int *out)
{
    cgltf_accessor_read_uint(acc, index, out, 1);
}


int find_joint_index(cgltf_data *data, cgltf_node *node)
{
    if(!data->skins) return -1;
    cgltf_skin *skin = &data->skins[0];
    for(int i = 0; i < (int)skin->joints_count; i++)
        if(skin->joints[i] == node) return i;
    return -1;
}

void extract_skeleton(cgltf_data *data, Skeleton *skel)
{
    if(!data->skins_count) return;
    cgltf_skin *skin = &data->skins[0];
    
    skel->jointCount = (int)skin->joints_count;
    skel->joints = (Joint*)arenaAlloc(&gArena, (skel->jointCount * sizeof(Joint)));
    
    for(int i = 0; i < skel->jointCount; i++)
    {
        Joint *j = &skel->joints[i];
        cgltf_node *node = skin->joints[i];
        
        strncpy(j->name, node->name ? node->name : "unnamed", 63);
        
        // inverse bind matrix
        if(skin->inverse_bind_matrices)
            cgltf_accessor_read_float(skin->inverse_bind_matrices, i, j->inverseBindMatrix, 16);
        else
            mat4_identity(j->inverseBindMatrix);
        
        // store default local transform from node
        if(node->has_translation)
        {
            j->defaultTranslation = {node->translation[0], node->translation[1], node->translation[2]};
        }
        else
        {
            j->defaultTranslation = {0,0,0};
        }
        
        if(node->has_rotation)
        {
            j->defaultRotation = {node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]};
        }
        else
        {
            j->defaultRotation = {0,0,0,1};
        }
        
        if(node->has_scale)
        {
            j->defaultScale = {node->scale[0], node->scale[1], node->scale[2]};
        }
        else
        {
            j->defaultScale = {1,1,1};
        }
        
        // find parent
        j->parent = -1;
        if(node->parent)
            j->parent = find_joint_index(data, node->parent);
    }
}

int path_to_type(cgltf_animation_path_type path)
{
    switch(path)
    {
        case cgltf_animation_path_type_translation: return 0;
        case cgltf_animation_path_type_rotation:    return 1;
        case cgltf_animation_path_type_scale:       return 2;
        default: return -1;
    }
}

void extract_animations(cgltf_data *data, Model *m)
{
    m->animCount  = (int)data->animations_count;
    m->animations = (Animation*)arenaAlloc(&gArena, (m->animCount * sizeof(Animation)));
    
    for(int a = 0; a < m->animCount; a++)
    {
        cgltf_animation *src  = &data->animations[a];
        Animation       *anim = &m->animations[a];
        
        strncpy(anim->name, src->name ? src->name : "unnamed", 63);
        anim->channelCount = (int)src->channels_count;
        anim->channels     = (AnimChannel*)arenaAlloc(&gArena, (anim->channelCount * sizeof(AnimChannel)));
        anim->duration     = 0.0f;
        
        for(int c = 0; c < anim->channelCount; c++)
        {
            cgltf_animation_channel *src_ch = &src->channels[c];
            cgltf_animation_sampler *samp   = src_ch->sampler;
            AnimChannel             *ch     = &anim->channels[c];
            
            ch->type       = path_to_type(src_ch->target_path);
            ch->jointIndex = find_joint_index(data, src_ch->target_node);
            
            int count        = (int)samp->input->count;
            ch->keyframeCount = count;
            ch->keyframes    = (Keyframe*)arenaAlloc(&gArena, (count * sizeof(Keyframe)));
            
            for(int k = 0; k < count; k++)
            {
                cgltf_accessor_read_float(samp->input,  k, &ch->keyframes[k].time,    1);
                cgltf_accessor_read_float(samp->output, k, &ch->keyframes[k].value.x,
                                          ch->type == 1 ? 4 : 3);  // rotation is vec4, others vec3
                
                if(ch->keyframes[k].time > anim->duration)
                    anim->duration = ch->keyframes[k].time;
            }
        }
    }
}

void extract_mesh(cgltf_data *data, Mesh *mesh)
{
    // first pass: count total verts and tris across all primitives
    int totalVerts = 0;
    int totalTris  = 0;
    for(int p = 0; p < (int)data->meshes[0].primitives_count; p++)
    {
        cgltf_primitive *prim = &data->meshes[0].primitives[p];
        totalVerts += (int)prim->attributes[0].data->count;
        totalTris  += (int)prim->indices->count / 3;
    }
    
    mesh->vertCount = totalVerts;
    mesh->triCount  = totalTris;
    mesh->verts = (Vertex*)arenaAlloc(&gArena, (totalVerts * sizeof(Vertex)));
    mesh->tris  = (Tri*)arenaAlloc(&gArena, (totalTris * sizeof(Tri)));
    memset(mesh->verts, 0, totalVerts * sizeof(Vertex));
    
    int vertOffset = 0;
    int triOffset  = 0;
    
    // second pass: extract each primitive
    for(int p = 0; p < (int)data->meshes[0].primitives_count; p++)
    {
        cgltf_primitive *prim = &data->meshes[0].primitives[p];
        
        cgltf_accessor *posAcc    = NULL;
        cgltf_accessor *normAcc   = NULL;
        cgltf_accessor *uvAcc     = NULL;
        cgltf_accessor *jointAcc  = NULL;
        cgltf_accessor *weightAcc = NULL;
        
        for(int i = 0; i < (int)prim->attributes_count; i++)
        {
            cgltf_attribute *attr = &prim->attributes[i];
            switch(attr->type)
            {
                case cgltf_attribute_type_position: posAcc    = attr->data; break;
                case cgltf_attribute_type_normal:   normAcc   = attr->data; break;
                case cgltf_attribute_type_texcoord: uvAcc     = attr->data; break;
                case cgltf_attribute_type_joints:   jointAcc  = attr->data; break;
                case cgltf_attribute_type_weights:  weightAcc = attr->data; break;
                default: break;
            }
        }
        
        int primVerts = (int)posAcc->count;
        
        for(int i = 0; i < primVerts; i++)
        {
            Vertex *v = &mesh->verts[vertOffset + i];
            if(posAcc)    read_float_n(posAcc,    i, &v->pos.x,      3);
            if(normAcc)   read_float_n(normAcc,   i, &v->normal.x,   3);
            if(uvAcc)     read_float_n(uvAcc,     i, &v->uv.u,       2);
            if(weightAcc) read_float_n(weightAcc, i, &v->weights[0], 4);
            if(jointAcc)
            {
                unsigned int j[4] = {};
                cgltf_accessor_read_uint(jointAcc, i, j, 4);
                v->joints[0]=j[0]; v->joints[1]=j[1];
                v->joints[2]=j[2]; v->joints[3]=j[3];
            }
        }
        
        int primTris = (int)prim->indices->count / 3;
        for(int i = 0; i < primTris; i++)
        {
            unsigned int a, b, c;
            read_uint(prim->indices, i*3+0, &a);
            read_uint(prim->indices, i*3+1, &b);
            read_uint(prim->indices, i*3+2, &c);
            // offset indices by vertOffset so they point to the right verts
            mesh->tris[triOffset + i] = {{(int)a + vertOffset,
                    (int)b + vertOffset,
                    (int)c + vertOffset}};
        }
        unsigned int color = 0xffffffff;
        if(prim->material)
        {
            cgltf_pbr_metallic_roughness *pbr = &prim->material->pbr_metallic_roughness;
            int r = (int)(pbr->base_color_factor[0] * 255);
            int g = (int)(pbr->base_color_factor[1] * 255);
            int b = (int)(pbr->base_color_factor[2] * 255);
            color = (b << 16) | (g << 8) | r;
        }
        
        SDL_Log("primitive %d: r=%d g=%d b=%d", p,
                (color >> 16) & 0xff,
                (color >>  8) & 0xff,
                (color >>  0) & 0xff);
        
        mesh->primitives[p] = { vertOffset, triOffset, primTris, color };
        mesh->primitiveCount++;
        
        // Update 
        vertOffset += primVerts;
        triOffset  += primTris;
    }
}

bool load_model(Model *m, const char *path)
{
    cgltf_options options = {};
    cgltf_data   *data    = NULL;
    
    if(cgltf_parse_file(&options, path, &data) != cgltf_result_success)
    {
        SDL_Log("cgltf_parse_file failed: %s", path);
        return false;
    }
    if(cgltf_load_buffers(&options, data, path) != cgltf_result_success)
    {
        SDL_Log("cgltf_load_buffers failed");
        cgltf_free(data);
        return false;
    }
    
    extract_mesh(data, &m->mesh);
    extract_skeleton(data, &m->skeleton);
    extract_animations(data, m);
    
    SDL_Log("mesh count: %d", (int)data->meshes_count);
    for(int i = 0; i < (int)data->meshes_count; i++)
        SDL_Log("mesh %d: %d primitives", i, (int)data->meshes[i].primitives_count);
    
    cgltf_free(data);
    return true;
}

void sample_animation(Animation *anim, float time, Pose *pose)
{
    // clamp/wrap time
    if(time > anim->duration) time = fmodf(time, anim->duration);
    
    for(int c = 0; c < anim->channelCount; c++)
    {
        AnimChannel *ch = &anim->channels[c];
        if(ch->jointIndex < 0) continue;
        
        Transform *joint = &pose->joints[ch->jointIndex];
        
        // find surrounding keyframes
        int k = 0;
        while(k < ch->keyframeCount - 1 && ch->keyframes[k+1].time < time)
            k++;
        
        int   k0 = k;
        int   k1 = (k + 1 < ch->keyframeCount) ? k + 1 : k;
        float t0 = ch->keyframes[k0].time;
        float t1 = ch->keyframes[k1].time;
        float t  = (t1 > t0) ? (time - t0) / (t1 - t0) : 0.0f;
        
        Vec4 v0 = ch->keyframes[k0].value;
        Vec4 v1 = ch->keyframes[k1].value;
        
        if(ch->jointIndex < 0 || ch->jointIndex >= pose->jointCount) continue;
        
        Transform *target = &pose->joints[ch->jointIndex];
        
        switch(ch->type)
        {
            case 0:  // translation
            target->translation = vec3_lerp({v0.x,v0.y,v0.z}, {v1.x,v1.y,v1.z}, t);
            break;
            case 1:  // rotation
            target->rotation = quat_lerp(v0, v1, t);
            break;
            case 2:  // scale
            target->scale = vec3_lerp({v0.x,v0.y,v0.z}, {v1.x,v1.y,v1.z}, t);
            break;
        }
    }
}

void eval_pose(Pose *pose, Skeleton *skel)
{
    // world space matrices per joint, temp buffer
    float *worldMats = (float*)arenaAlloc(&gArena, (skel->jointCount * 64));
    
    for(int i = 0; i < skel->jointCount; i++)
    {
        if(!pose->joints)
        {
            SDL_Log("pose->joints is null");
            return;
        }
        
        float local[16];
        trs_to_mat4(&pose->joints[i], local);
        
        if(skel->joints[i].parent < 0)
        {
            // root joint
            memcpy(&worldMats[i*16], local, 64);
        }
        else
        {
            // parent must already be computed — cgltf joints are
            // ordered parent before child so this is safe
            mat4_mul(&worldMats[i*16], &worldMats[skel->joints[i].parent*16], local);
        }
        
        // skinning matrix = worldMat * inverseBindMatrix
        mat4_mul(&pose->matrices[i*16], &worldMats[i*16], skel->joints[i].inverseBindMatrix);
    }
}

void mat4_perspective(float *m, float fov, float aspect, float znear, float zfar)
{
    float f = 1.0f / tanf(fov * 0.5f);
    memset(m, 0, 64);
    m[0]  = f / aspect;
    m[5]  = f;
    m[10] = (zfar + znear) / (znear - zfar);
    m[11] = -1.0f;
    m[14] = (2.0f * zfar * znear) / (znear - zfar);
}

void mat4_translation(float *m, float x, float y, float z)
{
    mat4_identity(m);
    m[12] = x;
    m[13] = y;
    m[14] = z;
}

void mat4_rotation_y(float *m, float angle)
{
    mat4_identity(m);
    m[0]  =  cosf(angle);
    m[2]  =  sinf(angle);
    m[8]  = -sinf(angle);
    m[10] =  cosf(angle);
}

void
debug_model_bounds(Mesh *mesh)
{
    float minx=1e9,miny=1e9,minz=1e9;
    float maxx=-1e9,maxy=-1e9,maxz=-1e9;
    for(int i = 0; i < mesh->vertCount; i++)
    {
        Vec3 v = mesh->verts[i].pos;
        if(v.x < minx) minx=v.x; if(v.x > maxx) maxx=v.x;
        if(v.y < miny) miny=v.y; if(v.y > maxy) maxy=v.y;
        if(v.z < minz) minz=v.z; if(v.z > maxz) maxz=v.z;
    }
    SDL_Log("bounds x[%.2f, %.2f] y[%.2f, %.2f] z[%.2f, %.2f]", 
            minx,maxx, miny,maxy, minz,maxz);
}

int
RendererInit()
{
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
    //SDL_Log("bgvertCode: %p\n", bgvertCode);
    //SDL_Log("bgfragCode: %p\n", bgfragCode);
    
    SDL_GPUShaderCreateInfo bgvertInfo               = {};
    bgvertInfo.code                                  = (Uint8*)bgvertCode;
    bgvertInfo.code_size                             = bgvertSize;
    bgvertInfo.entrypoint                            = "main";
    bgvertInfo.format                                = SDL_GPU_SHADERFORMAT_DXIL;
    bgvertInfo.stage                                 = SDL_GPU_SHADERSTAGE_VERTEX;
    bgvertInfo.num_uniform_buffers                   = 0;
    SDL_GPUShader *bgvertShader                      = SDL_CreateGPUShader(gGPUDevice, &bgvertInfo);
    //SDL_Log("bgvertShader: %p error: %s", (void*)bgvertShader, SDL_GetError());
    
    
    
    SDL_GPUShaderCreateInfo bgfragInfo               = {};
    bgfragInfo.code                                  = (Uint8*)bgfragCode;
    bgfragInfo.code_size                             = bgfragSize;
    bgfragInfo.entrypoint                            = "main";
    bgfragInfo.format                                = SDL_GPU_SHADERFORMAT_DXIL;
    bgfragInfo.stage                                 = SDL_GPU_SHADERSTAGE_FRAGMENT;
    bgfragInfo.num_uniform_buffers                   = 0;
    bgfragInfo.num_samplers                          = 1;
    bgfragInfo.num_storage_textures                  = 0;
    bgfragInfo.num_storage_buffers                   = 0;
    bgfragInfo.num_uniform_buffers                   = 0;
    
    SDL_GPUShader *bgfragShader                      = SDL_CreateGPUShader(gGPUDevice, &bgfragInfo);
    //SDL_Log("bgfragShader: %p error: %s", (void*)bgfragShader, SDL_GetError());
    
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
LoadModel(Model *m)
{
    load_model(m, "../chronicles/data/models/arwin8.glb");
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
    
    m->pipeline = SDL_CreateGPUGraphicsPipeline(gGPUDevice, &pipeInfo);
    
    // can release shaders after pipeline is created
    SDL_ReleaseGPUShader(gGPUDevice, vertShader);
    SDL_ReleaseGPUShader(gGPUDevice, fragShader);
    
    SDL_free(vertCode);
    SDL_free(fragCode);
}

void
RendererDestroy(Background *b, Model *m)
{
    SDL_ReleaseGPUBuffer(gGPUDevice, m->vertexBuffer);
    SDL_ReleaseGPUBuffer(gGPUDevice, m->indexBuffer);
    SDL_ReleaseGPUTexture(gGPUDevice, m->depthTexture);
    SDL_ReleaseGPUTexture(gGPUDevice, b->backgroundTexture);
    SDL_ReleaseGPUSampler(gGPUDevice, b->backgroundSampler);
    SDL_ReleaseGPUGraphicsPipeline(gGPUDevice, b->backgroundPipeline);
    SDL_ReleaseGPUGraphicsPipeline(gGPUDevice, m->pipeline);
    SDL_DestroyGPUDevice(gGPUDevice);
}

void
RenderBackground(Background *b, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain)
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
    SDL_DrawGPUPrimitives(bgPass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(bgPass);
}

void
RenderModel(Model *m, Player *p, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain)
{
    SDL_Log("Inside RenderModel");
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
    SDL_Log("begin gpurenderpass");
    
    SDL_GPUBufferBinding vb = { m->vertexBuffer, 0 };
    SDL_BindGPUVertexBuffers(pass, 0, &vb, 1);
    SDL_GPUBufferBinding ib = { m->indexBuffer, 0 };
    SDL_BindGPUIndexBuffer(pass, &ib, SDL_GPU_INDEXELEMENTSIZE_32BIT);
    
    // build MVP
    float proj[16], view[16], model[16], vp[16], mvp[16];
    mat4_perspective(proj, 1.0f, (float)gWINDOW_WIDTH / gWINDOW_HEIGHT, 0.1f, 100.0f);
    mat4_translation(view, -p->ModelX, -p->ModelY, -p->ModelZ);
    mat4_rotation_y(model, p->ModelRotY);
    mat4_mul(vp, proj, view);
    mat4_mul(mvp, vp, model);
    
    int jointCount    = m->skeleton.jointCount;
    int uniformSize   = (16 + jointCount * 16) * sizeof(float);
    float *uniforms   = (float*)alloca(uniformSize);
    memcpy(uniforms,      mvp,            64);
    memcpy(uniforms + 16, m->pose.matrices, jointCount * 64);
    SDL_PushGPUVertexUniformData(cmd, 0, uniforms, uniformSize);
    
    for(int i = 0; i < m->mesh.primitiveCount; i++)  // or primitiveCount if that's on the struct
    {
        Primitive *prim = &m->mesh.primitives[i];
        float red = ((prim->color >> 0)  & 0xff) / 255.0f;
        float green = ((prim->color >> 8)  & 0xff) / 255.0f;
        float blue = ((prim->color >> 16) & 0xff) / 255.0f;
        float col[4] = { red, green, blue, 1.0f };
        SDL_PushGPUFragmentUniformData(cmd, 0, col, sizeof(col));
        SDL_DrawGPUIndexedPrimitives(pass, prim->triCount * 3, 1, prim->triOffset * 3, 0, 0);
    }
    
    SDL_EndGPURenderPass(pass);
}

void pose_init(Pose *pose, Skeleton *skel)
{
    pose->jointCount = skel->jointCount;
    pose->joints     = (Transform*)arenaAlloc(&gArena, (pose->jointCount * sizeof(Transform)));
    pose->matrices   = (float*)arenaAlloc(&gArena, (pose->jointCount * 16 * sizeof(float)));
    memset(pose->matrices, 0, pose->jointCount * 16 * sizeof(float));
    
    // default to identity transforms
    for(int i = 0; i < pose->jointCount; i++)
    {
        pose->joints[i].translation = skel->joints[i].defaultTranslation;
        pose->joints[i].rotation    = skel->joints[i].defaultRotation;
        pose->joints[i].scale       = skel->joints[i].defaultScale;
    }
}

void pose_reset(Pose *pose, Skeleton *skel)
{
    memset(pose->matrices, 0, pose->jointCount * 16 * sizeof(float));
    for(int i = 0; i < pose->jointCount; i++)
    {
        pose->joints[i].translation = skel->joints[i].defaultTranslation;
        pose->joints[i].rotation    = skel->joints[i].defaultRotation;
        pose->joints[i].scale       = skel->joints[i].defaultScale;
    }
}

void
Render(Background *b, Model *m, Player *p, SDL_Window *w)
{
    SDL_Log("Render started");
    p->AnimTime = SDL_GetTicks() * 0.001f;
    if(m->animCount > 1)
        sample_animation(&m->animations[1], p->AnimTime, &m->pose);
    eval_pose(&m->pose, &m->skeleton);
    SDL_Log("eval pose finished");
    
    SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gGPUDevice);
    SDL_GPUTexture *swapchain;
    SDL_WaitAndAcquireGPUSwapchainTexture(cmd, w, &swapchain, NULL, NULL);
    
    SDL_Log("render bg started");
    RenderBackground(b, cmd, swapchain);
    SDL_Log("render bg ended");
    SDL_Log("render model started");
    RenderModel(m, p, cmd, swapchain);
    SDL_Log("render model ended");
    
    SDL_SubmitGPUCommandBuffer(cmd);
    
    pose_reset(&m->pose, &m->skeleton);
}
