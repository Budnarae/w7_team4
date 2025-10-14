
cbuffer PerObject : register(b1)
{
    float4x4 gWorld;
    float4x4 gViewProj;
    // ...
}

// b2: FireBall �Ķ����
cbuffer FireBallCB : register(b2)
{
    float3 gColor;
    float gIntensity; // ��/����
    
    float3 gCenterWS;
    float gRadius; // �߽�/�ݰ�(����)
    
    float4 gCenterClip; // CPU���� mul(float4(gCenterWS,1), ViewProj)
    
    float gProjRadiusNDC; // CPU���� ����� "NDC �ݰ�"
    float gFeather;
    float gHardness;
    float _pad;
};

struct VSIn
{
    float3 pos : POSITION; // ���� ��(���� �߽�) ���ؽ�
    float3 WorldPos : TEXCOORD0;
    // (���Ѵٸ� normal/uv �־ ����)
};

struct VSOut
{
    float4 Position : SV_POSITION; // �� �ȼ��� clip pos
};

VSOut VS_Sphere(VSIn i)
{
    VSOut o;
    // ���� ���� World�� ������/�̵�(R ����)�� ��ġ
    float4 wpos = mul(float4(i.pos, 1.0), gWorld);
    o.Position = mul(wpos, gViewProj);
    return o;
}

// ȭ�� ���� ���� falloff (NDC���� �߽ɱ��� �Ÿ� ���)
// feather: 0~1 (�����ڸ� ��), hardness: 1.5~3 ��õ
float SmoothCircleNDC(float2 ndc, float2 centerNdc, float projRadiusNdc, float feather, float hardness)
{
    // NDC �Ÿ�(0=�߽�, projRadiusNdc=�ܰ�)
    float r = length(ndc - centerNdc);
    float x = r / max(projRadiusNdc, 1e-5);

    float edge0 = 1.0 - saturate(feather); // 0.6~0.8 ����
    float t = saturate((x - edge0) / max(1.0 - edge0, 1e-5));
    return pow(1.0 - t, hardness);
}

float4 PS_Sphere(VSOut i) : SV_Target
{
    // ���� �ȼ� NDC
    float2 ndc = (i.Position.xy / i.Position.w);
    // �߽� NDC
    float2 cndc = (gCenterClip.xy / gCenterClip.w);

    // ���� falloff
    float a = SmoothCircleNDC(ndc, cndc, gProjRadiusNDC, gFeather, gHardness);

    // Additive ��� (���Ĵ� �ǹ� ����)
    return float4(gColor * (gIntensity * a), 0.0);
}