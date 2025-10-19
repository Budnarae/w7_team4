#ifndef LIGHTING_MODEL_GOURAUD
#define LIGHTING_MODEL_GOURAUD 0
#endif

#ifndef LIGHTING_MODEL_LAMBERT
#define LIGHTING_MODEL_LAMBERT 0
#endif

#ifndef LIGHTING_MODEL_PHONG
#define LIGHTING_MODEL_PHONG 0
#endif

#ifndef LIGHT_TYPE_POINT
#define LIGHT_TYPE_POINT 0
#endif

#ifndef LIGHT_TYPE_SPOT
#define LIGHT_TYPE_SPOT 0
#endif

cbuffer PerObject : register(b0)
{
    row_major float4x4 World;
};

cbuffer PerFrame : register(b1)
{
    row_major float4x4 View;
    row_major float4x4 Projection;
};

// Unified Light Properties (160 bytes)
cbuffer LightProperties : register(b2)
{
	// InverseViewProj Matrix
	row_major float4x4 InverseViewProj;  // 64 bytes

	// Position + Intensity
	float3 LightPosition;       // 12 bytes
	float Intensity;            // 4 bytes

	// Color + Radius
	float3 LightColor;          // 12 bytes
	float Radius;               // 4 bytes

	// RadiusFalloff + Viewport
	float RadiusFalloff;        // 4 bytes
	float _Padding0;            // 4 bytes
	float2 ViewportTopLeft;     // 8 bytes

	// ViewportSize + SceneRTSize
	float2 ViewportSize;        // 8 bytes
	float2 SceneRTSize;         // 8 bytes

	// SpotLight 전용 필드 (PointLight에서는 미사용)
	float3 LightDirection;      // 12 bytes
	float InnerConeAngle;       // 4 bytes

	float OuterConeAngle;       // 4 bytes
	float3 _Padding1;           // 12 bytes
};

// Scene Depth Texture
Texture2D SceneDepthTexture : register(t0);
SamplerState DepthSampler : register(s0);

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
    float4 ScreenPos : TEXCOORD2;
#if LIGHTING_MODEL_GOURAUD
    float4 VertexColor : COLOR;
#endif
};

// Vertex Shader
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT Output;

    float4 WorldPosition = mul(float4(input.Position, 1.0), World);
    Output.WorldPos = WorldPosition.xyz;
    Output.Normal = normalize(mul(float4(input.Normal, 0.0), World).xyz);

    float4 viewPos = mul(WorldPosition, View);
    Output.Position = mul(viewPos, Projection);

    Output.ScreenPos = Output.Position;

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: Vertex Shader에서 라이팅 계산
    // TODO: 여기서 정점 라이팅 계산 후 Output.VertexColor에 저장
    Output.VertexColor = float4(0, 0, 0, 1);
#endif

    return Output;
}

// World Position 재구성
float3 ReconstructWorldPosition(float2 ViewportUV, float Depth)
{
    float2 ScreenPosition = ViewportUV * 2.0 - 1.0;
    ScreenPosition.y = -ScreenPosition.y;

    float4 ClipPosition = float4(ScreenPosition, Depth, 1.0);
    float4 WorldPosition = mul(ClipPosition, InverseViewProj);
    WorldPosition.xyz /= WorldPosition.w;

    return WorldPosition.xyz;
}

// SpotLight Cone Attenuation 계산
float CalculateSpotLightConeAttenuation(float3 LightToSurfaceDir)
{
    float CosAngle = dot(LightDirection, LightToSurfaceDir);
    float CosInner = cos(InnerConeAngle);
    float CosOuter = cos(OuterConeAngle);

    float InvCosConeDifference = 1.0 / max(CosInner - CosOuter, 0.0001);
    float AttenuationMask = saturate((CosAngle - CosOuter) * InvCosConeDifference);

    return AttenuationMask * AttenuationMask;
}

// Pixel Shader
float4 mainPS(PS_INPUT Input) : SV_TARGET
{
#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: VS에서 계산한 색상 사용
    // TODO: VS에서 계산한 VertexColor 활용
    return Input.VertexColor;

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: PS에서 Diffuse 라이팅 계산
    // TODO: Ambient + Diffuse 라이팅 계산
    // float3 normal = normalize(Input.Normal);
    // float3 lightDir = normalize(LightPosition - Input.WorldPos);
    // float NdotL = max(dot(normal, lightDir), 0.0);
    // float3 diffuse = LightColor * Intensity * NdotL;
    return float4(0, 0, 0, 1);

#elif LIGHTING_MODEL_PHONG
    // Phong Shading: PS에서 Diffuse + Specular 라이팅 계산
    // TODO: Ambient + Diffuse + Specular 라이팅 계산
    // float3 normal = normalize(Input.Normal);
    // float3 lightDir = normalize(LightPosition - Input.WorldPos);
    // float3 viewDir = normalize(CameraPosition - Input.WorldPos);
    // float3 reflectDir = reflect(-lightDir, normal);
    // float spec = pow(max(dot(viewDir, reflectDir), 0.0), Shininess);
    return float4(0, 0, 0, 1);

#else
    // Deferred Light Volume Rendering (기존 방식)
    // Viewport UV 계산
    float2 ViewportUV = Input.ScreenPos.xy / Input.ScreenPos.w;
    ViewportUV = ViewportUV * 0.5 + 0.5;
    ViewportUV.y = 1.0 - ViewportUV.y;

    // Scene RT UV 변환
    float2 SceneUV = (ViewportTopLeft + ViewportUV * ViewportSize) / SceneRTSize;

    // Depth 샘플링
    float Depth = SceneDepthTexture.Sample(DepthSampler, SceneUV).r;

    // World Position 재구성
    float3 SceneWorldPosition = ReconstructWorldPosition(ViewportUV, Depth);

    // 거리 계산
    float3 LightToScene = SceneWorldPosition - LightPosition;
    float DistFromLight = length(LightToScene);

    // Radius 밖이면 discard
    if (DistFromLight > Radius)
    {
        discard;
    }

    // 거리 감쇠
    float3 WorldLightVector = LightToScene / Radius;
    float NormalizedDistanceSquared = dot(WorldLightVector, WorldLightVector);
    float AttenuationMask = 1.0 - saturate(NormalizedDistanceSquared);
    float DistanceAttenuation = pow(AttenuationMask, RadiusFalloff);

    float FinalAttenuation = DistanceAttenuation;

#if LIGHT_TYPE_SPOT
    // SpotLight Cone Attenuation
    {
        float3 LightToSurfaceDir = normalize(LightToScene);
        float ConeAttenuation = CalculateSpotLightConeAttenuation(LightToSurfaceDir);

        if (ConeAttenuation <= 0.0)
        {
            discard;
        }

        FinalAttenuation *= ConeAttenuation;
    }
#endif

    // Final Light Contribution
    float3 LightContribution = LightColor * Intensity * FinalAttenuation;

    return float4(LightContribution, FinalAttenuation);
#endif
}
