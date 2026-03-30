extern const int WINDOW_WIDTH;
extern const int WINDOW_HEIGHT;
extern int *gFrameBuffer;
extern float *gZBuffer;
extern float gModelX, gModelY, gModelZ;

struct Vec2 { float u, v; };
struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };

extern Vec3 *gSkinnedPositions;
extern Vec3 *gSkinnedNormals;


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

// full model
struct Model
{
    Mesh      mesh;
    Skeleton  skeleton;
    Animation *animations;
    int        animCount;
};

Model gModel;

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

Pose gPose;

void pose_init(Pose *pose, Skeleton *skel)
{
    pose->jointCount = skel->jointCount;
    pose->joints     = (Transform*)malloc(pose->jointCount * sizeof(Transform));
    pose->matrices   = (float*)malloc(pose->jointCount * 16 * sizeof(float));
    memset(pose->matrices, 0, pose->jointCount * 16 * sizeof(float));
    
    // default to identity transforms
    for(int i = 0; i < pose->jointCount; i++)
    {
        pose->joints[i].translation = skel->joints[i].defaultTranslation;
        pose->joints[i].rotation    = skel->joints[i].defaultRotation;
        pose->joints[i].scale       = skel->joints[i].defaultScale;
    }
}

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
    mesh->verts = (Vertex*)malloc(totalVerts * sizeof(Vertex));
    mesh->tris  = (Tri*)malloc(totalTris * sizeof(Tri));
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
    skel->joints = (Joint*)malloc(skel->jointCount * sizeof(Joint));
    
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

void extract_animations(cgltf_data *data, Model *model)
{
    model->animCount  = (int)data->animations_count;
    model->animations = (Animation*)malloc(model->animCount * sizeof(Animation));
    
    for(int a = 0; a < model->animCount; a++)
    {
        cgltf_animation *src  = &data->animations[a];
        Animation       *anim = &model->animations[a];
        
        strncpy(anim->name, src->name ? src->name : "unnamed", 63);
        anim->channelCount = (int)src->channels_count;
        anim->channels     = (AnimChannel*)malloc(anim->channelCount * sizeof(AnimChannel));
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
            ch->keyframes    = (Keyframe*)malloc(count * sizeof(Keyframe));
            
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

bool load_model(const char *path)
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
    
    extract_mesh(data, &gModel.mesh);
    extract_skeleton(data, &gModel.skeleton);
    extract_animations(data, &gModel);
    
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
    float *worldMats = (float*)alloca(skel->jointCount * 64);
    
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

void skin_mesh(Mesh *mesh, Pose *pose)
{
    if(!pose->matrices)
    {
        SDL_Log("pose->matrices is null");
        return;
    }
    
    for(int i = 0; i < mesh->vertCount; i++)
    {
        Vertex *v   = &mesh->verts[i];
        Vec3    pos = {0,0,0};
        Vec3    nor = {0,0,0};
        
        for(int j = 0; j < 4; j++)
        {
            float w = v->weights[j];
            if(w == 0.0f) continue;
            
            float *m = &pose->matrices[v->joints[j] * 16];
            Vec3 p, n;
            mat4_mul_vec3(m, v->pos,    &p);
            mat4_mul_vec3(m, v->normal, &n);  // note: normals need inverse transpose for correctness but this is fine for uniform scale
            pos.x += p.x*w; pos.y += p.y*w; pos.z += p.z*w;
            nor.x += n.x*w; nor.y += n.y*w; nor.z += n.z*w;
        }
        
        gSkinnedPositions[i] = pos;
        gSkinnedNormals[i]   = nor;
    }
}

Vec3 project(Vec3 v, float rotY)
{
    float cosR = cosf(rotY), sinR = sinf(rotY);
    float rx = v.x * cosR - v.z * sinR;
    float rz = v.x * sinR + v.z * cosR;
    //v.x = rx;
    //v.z = rz;
    //v.z += 5.0f;
    v.x = rx + gModelX;
    v.z = rz + gModelZ;
    v.y = -v.y + gModelY;
    float px = (v.x / v.z) * (WINDOW_HEIGHT / 2) + WINDOW_WIDTH  / 2;
    float py = (v.y / v.z) * (WINDOW_HEIGHT / 2) + WINDOW_HEIGHT / 2;
    return {px, py, v.z};
}

void draw_tri(Vec3 a, Vec3 b, Vec3 c, unsigned int color)
{
    int minx = max(0,              (int)min3(a.x, b.x, c.x));
    int maxx = min(WINDOW_WIDTH-1, (int)max3(a.x, b.x, c.x));
    int miny = max(0,              (int)min3(a.y, b.y, c.y));
    int maxy = min(WINDOW_HEIGHT-1,(int)max3(a.y, b.y, c.y));
    
    for(int y = miny; y <= maxy; y++)
    {
        for(int x = minx; x <= maxx; x++)
        {
            float w0 = ((b.x-a.x)*(y-a.y) - (b.y-a.y)*(x-a.x));
            float w1 = ((c.x-b.x)*(y-b.y) - (c.y-b.y)*(x-b.x));
            float w2 = ((a.x-c.x)*(y-c.y) - (a.y-c.y)*(x-c.x));
            if(w0 >= 0 && w1 >= 0 && w2 >= 0)
            {
                float area = w0+w1+w2;
                float z = (w0*a.z + w1*b.z + w2*c.z) / area;
                int idx = y*WINDOW_WIDTH + x;
                if(z < gZBuffer[idx])
                {
                    gZBuffer[idx] = z;
                    gFrameBuffer[idx] = color | 0xff000000;
                }
            }
        }
    }
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