cbuffer FragUniforms : register(b0, space3)
{
    float4 color;
    float  ambientLight;
};

float4 main() : SV_Target
{
    return color * ambientLight;
}

// compile: dxc -T ps_6_0 -E main fragment.hlsl -Fo fragment.dxil