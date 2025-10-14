cbuffer PerObject : register(b1)
{
    row_major float4x4 gWorld;
    row_major float4x4 gViewProj;
}

cbuffer FireBallCB : register(b2)
{
    float3 gColor;
    float gIntensity;
    float3 gCenterWS;
    float gRadius;
    float4 gCenterClip; // CPU: mul(float4(gCenterWS,1), ViewProj)
    float gProjRadiusNDC;
    float gFeather;
    float gHardness;
    float _pad;
}

// DepthTex/Sampler�� �� ���� ����(��� ������ ������ �ص� ����)

// ---- IO ----
struct VS_INPUT
{
    float3 Position : POSITION;
};
struct PS_INPUT
{
    float4 Position : SV_POSITION; // �����Ϳ�
    float2 Ndc : TEXCOORD0; // �츮�� ����ؼ� �ѱ�� NDC
};

PS_INPUT VS_Sphere(VS_INPUT i)
{
    PS_INPUT o;
    float4 wpos = mul(float4(i.Position, 1.0), gWorld);
    float4 clip = mul(wpos, gViewProj);
    o.Position = clip;
    o.Ndc = clip.xy / clip.w; // �� NDC�� VS���� ����� �ѱ�
    return o;
}

// ȭ�� ���� ���� falloff
float SmoothCircleNDC(float2 ndc, float2 centerNdc, float projRadiusNdc, float feather, float hardness)
{
    float r = length(ndc - centerNdc);
    float x = r / max(projRadiusNdc, 1e-5);
    //float edge0 = 1.0 - saturate(feather);
    //float t = saturate((x - edge0) / max(1.0 - edge0, 1e-5));
    return pow(1.0 - x, hardness);
}

float4 PS_Sphere(PS_INPUT i) : SV_Target
{
    float2 cndc = (gCenterClip.xy / gCenterClip.w);
    float a = SmoothCircleNDC(i.Ndc, cndc, gProjRadiusNDC, gFeather, gHardness);
    return float4(gColor * (gIntensity * a), 1.0);
}

PS_INPUT mainVS(VS_INPUT i)
{
    return VS_Sphere(i);
}

float4 mainPS(PS_INPUT i) : SV_Target
{
    return PS_Sphere(i);
    //return float4(10.0, 10.0, 10.0, 0.0);
}
