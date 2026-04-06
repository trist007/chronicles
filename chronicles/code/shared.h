/* date = March 31st 2026 1:58 pm */

#ifndef SHARED_H
#define SHARED_H

#define MAX_DEBUG_LINES 100
#define PI32 3.14159265359f

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

struct LineVertex
{
    Vec3 position;
};

struct LinePipeline
{
    SDL_GPUGraphicsPipeline                 *Pipeline;
    SDL_GPUTransferBuffer                   *transferBuffer;              
    SDL_GPUBuffer                           *LineVertexBuffer;
    LineVertex                              LineVerts[MAX_DEBUG_LINES *  2];
    int                                     LineVertCount;
};

struct pipelineObjects
{
    LinePipeline lp;
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

// ################################################################################
// Skeleton
// ################################################################################

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

// ################################################################################
// Arena
// ################################################################################

struct Arena
{
    uint8_t  *base;
    size_t   size;
    size_t   offset;
};

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

Arena gArena;

// ################################################################################
// Model
// ################################################################################

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
    
    // Storage
    float                     *uniformBuffer;
    float                     *worldMats;
    int                       jointCount;
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

void eval_pose(Pose *pose, Skeleton *skel, float *worldMats)
{
    // world space matrices per joint, temp buffer
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
mat4_lookat(float *m,
            float eyeX,   float eyeY,   float eyeZ,
            float targetX, float targetY, float targetZ,
            float upX,    float upY,    float upZ)
{
    // forward vector (eye to target)
    float fx = targetX - eyeX;
    float fy = targetY - eyeY;
    float fz = targetZ - eyeZ;
    float flen = sqrtf(fx*fx + fy*fy + fz*fz);
    fx /= flen;
    fy /= flen;
    fz /= flen;
    
    // right vector (forward cross up)
    float rx = fy*upZ - fz*upY;
    float ry = fz*upX - fx*upZ;
    float rz = fx*upY - fy*upX;
    float rlen = sqrtf(rx*rx + ry*ry + rz*rz);
    rx /= rlen;
    ry /= rlen;
    rz /= rlen;
    
    // recompute up vector (right cross forward)
    float ux = ry*fz - rz*fy;
    float uy = rz*fx - rx*fz;
    float uz = rx*fy - ry*fx;
    
    m[0]  =  rx;
    m[1]  =  ux;
    m[2]  = -fx;
    m[3]  =  0.0f;
    
    m[4]  =  ry;
    m[5]  =  uy;
    m[6]  = -fy;
    m[7]  =  0.0f;
    
    m[8]  =  rz;
    m[9]  =  uz;
    m[10] = -fz;
    m[11] =  0.0f;
    
    m[12] = -(rx*eyeX + ry*eyeY + rz*eyeZ);
    m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
    m[14] =  (fx*eyeX + fy*eyeY + fz*eyeZ);
    m[15] =  1.0f;
}

void
mat4_ortho(float *m, float left, float right, float bottom, float top, float nearZ, float farZ)
{
    m[0]  = 2.0f / (right - left);
    m[1]  = 0.0f;
    m[2]  = 0.0f;
    m[3]  = 0.0f;
    
    m[4]  = 0.0f;
    m[5]  = 2.0f / (top - bottom);
    m[6]  = 0.0f;
    m[7]  = 0.0f;
    
    m[8]  = 0.0f;
    m[9]  = 0.0f;
    m[10] = -2.0f / (farZ - nearZ);
    m[11] = 0.0f;
    
    m[12] = -(right + left)   / (right - left);
    m[13] = -(top   + bottom) / (top   - bottom);
    m[14] = -(farZ  + nearZ)  / (farZ  - nearZ);
    m[15] = 1.0f;
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

// ################################################################################
// GameState
// ################################################################################

struct Player
{
    float AnimTime;
    Vec3  position;
    Vec3  velocity;
    float yaw;
    bool  isWalking;
    bool  isIdle;
    bool  isSearching;
};

struct GameState
{
    Player player;
    Model model;
    Background bg;
    const char *graphicsAPI;
    pipelineObjects po;
    
    // light
    Vec3 lightPos;
};

// ################################################################################
// Graphics
// ################################################################################


int RendererInit();
void LoadBackground(Background * b);
void pose_alloc(Pose * pose, Skeleton * skel);
void LoadModel(Model * m);
void RendererDestroy(Background * b, Model * m);
void RenderBackground(Background * b, SDL_GPUCommandBuffer * cmd, SDL_GPUTexture * swapchain);
void RenderModel(Model * m, Player * p, Vec3 lightPos, SDL_GPUCommandBuffer * cmd, SDL_GPUTexture * swapchain, float * mvp);
SDL_GPUShader * CreateShader(const char * path, SDL_GPUShaderStage stage, int numUniformBuffers, int numSamplers);
SDL_GPUGraphicsPipeline * CreatePipeline(SDL_GPUShader * vertShader, SDL_GPUShader * fragShader, SDL_GPUVertexBufferDescription * vbDescs, int numVBDescs, SDL_GPUVertexAttribute * attrs, int numAttrs, SDL_GPUPrimitiveType topology, bool depthTest);
void PushLine(LinePipeline * lp, Vec3 a, Vec3 b);
void FlushLines(LinePipeline * lp, SDL_GPUCommandBuffer * cmd, SDL_GPUTexture * swapchain, float * ortho);
void CreateLinePipeline(LinePipeline * lp);


// ################################################################################
// Text
// ################################################################################

struct Text
{
    SDL_GPUTexture          *fontTexture;
    SDL_GPUSampler          *fontSampler;
    SDL_GPUGraphicsPipeline *fontPipeline;
    int                     width;
    int                     height;
};

struct glyph
{
    float u0, v0; // top left UV
    float u1, v1; // bottom right UV
    int width;    // pixel width of this character
    int height;   // pixel height
};

struct font_atlas
{
    SDL_GPUTexture           *texture;
    SDL_GPUSampler           *sampler;
    SDL_GPUGraphicsPipeline  *pipeline;
    SDL_GPUBuffer            *vertexBuffer;
    glyph                    glyphs[128];
    int                      glyphHeight;
    int                      atlasWidth;
    int                      atlasHeight;
};

struct font_vertex
{
    float x, y;
    float  u, v;
};

void LoadFontAtlas(font_atlas *atlas);
void DrawText(font_atlas *atlas, SDL_GPUCommandBuffer *cmd, SDL_GPUTexture *swapchain,
              const char *text, float x, float y, float r, float g, float b);



#endif //SHARED_H