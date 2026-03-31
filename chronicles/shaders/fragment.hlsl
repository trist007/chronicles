cbuffer FragUniforms : register(b0, space3)
{
    float4 color;
};

float4 main() : SV_Target
{
    return color;
}

// compile: dxc -T ps_6_0 -E main fragment.hlsl -Fo fragment.dxil