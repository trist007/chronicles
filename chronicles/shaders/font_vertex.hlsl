cbuffer Uniforms : register(b0, space1)
{
    float2 pos;
    float2 size;
    float2 screenSize;
};

static float2 uvs[6] = {
    {0,0}, {1,0}, {1,1},
    {0,0}, {1,1}, {0,1}
};

static float2 verts[6] = {
    {0,0}, {1,0}, {1,1},
    {0,0}, {1,1}, {0,1}
};

struct Output
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

Output main(uint id : SV_VertexID)
{
    float2 v = verts[id] * size + pos;
    // convert pixel coords to clip space
    float2 clip = (v / screenSize) * 2.0 - 1.0;
    clip.y = -clip.y;
    
    Output o;
    o.pos = float4(clip, 0, 1);
    o.uv  = uvs[id];
    return o;
}

// compile: dxc -T vs_6_0 -E main font_vertex.hlsl -Fo font_vertex.dxil