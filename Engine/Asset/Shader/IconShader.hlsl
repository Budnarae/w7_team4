// Icon용 셰이더
// Icon은 광원, 노멀 등에 영향을 받지 않음
// LightComponent의 Icon은 광원의 색깔에 따라서만 색상이 변하는 기능이 있음
cbuffer constants : register(b0)
{
    row_major float4x4 world;
}

cbuffer PerFrame : register(b1)
{
    row_major float4x4 View; // View Matrix Calculation of MVP Matrix
    row_major float4x4 Projection; // Projection Matrix Calculation of MVP Matrix
};

cbuffer LightData : register(b2)
{
    float3 LightColor;
    float LightIntensity;
};

Texture2D DiffuseTexture : register(t0);
SamplerState SamplerWrap : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float2 tex : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
    float3 normal : TEXCOORD0;
    float2 tex : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    float4 tmp = float4(input.position, 1.0f);
    tmp = mul(tmp, world);
    tmp = mul(tmp, View);
    tmp = mul(tmp, Projection);
    output.position = tmp;
    output.tex = input.tex;

    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    float4 texColor = DiffuseTexture.Sample(SamplerWrap, input.tex);
    texColor.rgb *= LightColor * LightIntensity;

    clip(texColor.a - 0.1f);
    return texColor;
}
