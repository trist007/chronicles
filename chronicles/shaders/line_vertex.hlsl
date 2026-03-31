cbuffer Uniforms : register(b0, space1)
{
    float4x4 mvp;
};

float4 main(float3 pos : TEXCOORD0) : SV_Position
{
    return mul(float4(pos, 1.0), mvp);
}

// compile: dxc -T vs_6_0 -E main line_vertex.hlsl -Fo line_vertex.dxil