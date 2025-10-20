#ifndef LIGHTING_MODEL_GOURAUD
#define LIGHTING_MODEL_GOURAUD 0
#endif

#ifndef LIGHTING_MODEL_LAMBERT
#define LIGHTING_MODEL_LAMBERT 0
#endif

#ifndef LIGHTING_MODEL_PHONG
#define LIGHTING_MODEL_PHONG 0
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 4
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 4
#endif

// Light Info Structures
struct FAmbientLightInfo
{
    float3 Color;           // 12 bytes
    float Intensity;        // 4 bytes
};

struct FDirectionalLightInfo
{
    float3 Direction;       // 12 bytes
    float Intensity;        // 4 bytes
    float3 Color;           // 12 bytes
    float _Padding;         // 4 bytes
};

struct FPointLightInfo
{
    float3 Position;        // 12 bytes
    float Intensity;        // 4 bytes
    float3 Color;           // 12 bytes
    float Radius;           // 4 bytes
    float Falloff;          // 4 bytes
    float3 _Padding;        // 12 bytes
};

struct FSpotLightInfo
{
    float3 Position;        // 12 bytes
    float Intensity;        // 4 bytes
    float3 Direction;       // 12 bytes
    float Radius;           // 4 bytes
    float3 Color;           // 12 bytes
    float InnerConeAngle;   // 4 bytes
    float OuterConeAngle;   // 4 bytes
    float Falloff;          // 4 bytes
    float2 _Padding;        // 8 bytes
};

cbuffer PerObject : register(b0)
{
    row_major float4x4 World;
	row_major float4x4 WorldTransInv;
};

cbuffer PerFrame : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
};

cbuffer Material : register(b2)
{
    float4 Ka;
    float4 Kd;
    float4 Ks;
    float Ns;
    float Ni;
    float D;
    uint MaterialFlags;
    float Time;
};

cbuffer Lighting : register(b3)
{
    FAmbientLightInfo Ambient;
    FDirectionalLightInfo Directional;
    FPointLightInfo PointLights[NUM_POINT_LIGHTS];
    FSpotLightInfo SpotLights[NUM_SPOT_LIGHTS];
    uint NumActivePointLights;
    uint NumActiveSpotLights;
    float2 _LightingPadding;
};

// 추가 정보
cbuffer SceneInfo : register(b4)
{
    float2 ViewportTopLeft;
    float2 ViewportSize;
    float2 SceneRTSize;
    float2 _ScenePadding;
};

// Textures
Texture2D DiffuseTexture : register(t0);
SamplerState TextureSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD;
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
#if LIGHTING_MODEL_GOURAUD
    float4 VertexColor : COLOR;
#endif
};

// Helper Functions
float3 CalculateAmbientLight(FAmbientLightInfo Light)
{
    return Light.Color * Light.Intensity;
}

float CalculateAttenuation(float Distance, float Radius, float Falloff)
{
    float NormalizedDist = saturate(Distance / Radius);
    return pow(1.0 - NormalizedDist, Falloff);
}

float3 CalculateLambertDiffuse(float3 Normal, float3 LightDirection, float3 LightColor, float Intensity)
{
    float NdotL = max(dot(Normal, LightDirection), 0.0);
    return LightColor * Intensity * NdotL;
}

float3 CalculatePointLight(FPointLightInfo Light, float3 WorldPosition, float3 Normal)
{
    float3 LightVector = Light.Position - WorldPosition;
    float Distance = length(LightVector);

    if (Distance > Light.Radius)
    {
        return float3(0, 0, 0);
    }

    float3 LightDir = LightVector / Distance;
    float Attenuation = CalculateAttenuation(Distance, Light.Radius, Light.Falloff);

    return CalculateLambertDiffuse(Normal, LightDir, Light.Color, Light.Intensity) * Attenuation;
}

float3 CalculateSpotLight(FSpotLightInfo Light, float3 WorldPosition, float3 Normal)
{
    float3 LightVector = Light.Position - WorldPosition;
    float Distance = length(LightVector);

    if (Distance > Light.Radius)
    {
        return float3(0, 0, 0);
    }

    float3 LightDirection = normalize(LightVector);

    // Spot Cone Attenuation
    // Light.Direction: SpotLight가 향하는 방향
    // -LightDirection: 픽셀에서 Light로 향하는 방향 (LightDirection과 반대)
    // Cone 각도 체크를 위해 두 방향 벡터의 내적 계산
    float CosAngle = dot(normalize(Light.Direction), -LightDirection);
    float CosInner = cos(Light.InnerConeAngle);
    float CosOuter = cos(Light.OuterConeAngle);

    // Cone 범위 밖이면 조명 없음
    if (CosAngle < CosOuter)
    {
        return float3(0, 0, 0);
    }

    // Inner/Outer Cone 사이에서 부드러운 감쇠 처리
    float SpotAttenuation = saturate((CosAngle - CosOuter) / max(CosInner - CosOuter, 0.0001));
    SpotAttenuation *= SpotAttenuation;

    float DistAttenuation = CalculateAttenuation(Distance, Light.Radius, Light.Falloff);

    return CalculateLambertDiffuse(Normal, LightDirection, Light.Color, Light.Intensity) * DistAttenuation * SpotAttenuation;
}

// Vertex Shader
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT Output;

    // World-View-Projection 변환
    float4 WorldPosition = mul(float4(input.Position, 1.0), World);
    float4 ViewPosition = mul(WorldPosition, View);
    Output.Position = mul(ViewPosition, Projection);

    Output.WorldPos = WorldPosition.xyz;
    Output.Normal = normalize(mul(float4(input.Normal, 0.0), WorldTransInv).xyz);
    Output.TexCoord = input.TexCoord;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: Vertex Shader에서 라이팅 계산
    float3 totalLight = float3(0, 0, 0);

    // Ambient
    totalLight += CalculateAmbientLight(Ambient);

    // TODO: Directional, Point, Spot lights in VS

    Output.VertexColor = float4(totalLight, 1.0);
#endif

    return Output;
}

// Pixel Shader
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
    // Texture Color 샘플링
    float4 BaseColor = DiffuseTexture.Sample(TextureSampler, Input.TexCoord);

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: VS에서 계산한 색상 사용
    return BaseColor * Input.VertexColor;

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: PS에서 Diffuse 라이팅 계산
    float3 TotalLight = CalculateAmbientLight(Ambient);

    // Point Lights
    for (uint i = 0; i < NumActivePointLights; ++i)
    {
        TotalLight += CalculatePointLight(PointLights[i], Input.WorldPos, Input.Normal);
    }

    // Spot Lights
    for (uint j = 0; j < NumActiveSpotLights; ++j)
    {
        TotalLight += CalculateSpotLight(SpotLights[j], Input.WorldPos, Input.Normal);
    }

    return float4(BaseColor.rgb * TotalLight, BaseColor.a);

#elif LIGHTING_MODEL_PHONG
    // Phong Shading: PS에서 Diffuse + Specular 라이팅 계산
    // TODO: Ambient + Diffuse + Specular
    return float4(0, 0, 0, 1);

#else
    // No Lighting Model: Unlit (텍스처만 출력)
    return BaseColor;
#endif
}
