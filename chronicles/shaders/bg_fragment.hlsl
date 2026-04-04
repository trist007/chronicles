Texture2D tex : register(t0, space2);
SamplerState samp : register(s0, space2);

cbuffer Uniforms : register(b0, space3)
{
    float ambientLight; // 0.0 = pitch black, 1.0 = full brightness
};

struct PSInput
{
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};

float4 main(PSInput input) : SV_Target
{
    return tex.Sample(samp, input.uv) * ambientLight;
}

// compile: dxc -T ps_6_0 -E main bg_fragment.hlsl -Fo bg_fragment.dxil