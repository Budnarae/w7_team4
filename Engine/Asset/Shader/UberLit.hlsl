#ifndef LIGHTING_MODEL_GOURAUD
#define LIGHTING_MODEL_GOURAUD 0
#endif

#ifndef LIGHTING_MODEL_LAMBERT
#define LIGHTING_MODEL_LAMBERT 0
#endif

#ifndef LIGHTING_MODEL_PHONG
#define LIGHTING_MODEL_PHONG 0
#endif

#ifndef NORMAL_MAPPING
#define NORMAL_MAPPING 0
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 16
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 16
#endif

#ifndef TILE_SIZE
#define TILE_SIZE 32
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
    float3 CameraPosition;
    float _PerFramePadding;
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
    uint _LightingPadding[2];
};

// 추가 정보
cbuffer SceneInfo : register(b4)
{
    float2 ViewportTopLeft;
    float2 ViewportSize;
    float2 SceneRTSize;
    uint NumTilesX;
    uint NumTilesY;
};

// Textures
Texture2D DiffuseTexture : register(t0);

#if NORMAL_MAPPING
Texture2D NormalMap : register(t3);
#endif

// Tile Light 인덱스 리스트 (Tiled-Based Light Culling 결과)
StructuredBuffer<uint> TileLightOffsets : register(t4);   // 각 타일의 라이트 리스트 시작 오프셋
StructuredBuffer<uint2> TileLightCounts : register(t5);   // 각 타일의 (PointLight 개수, SpotLight 개수)
StructuredBuffer<uint> TileLightIndices : register(t6);   // 전역 라이트 인덱스 배열

SamplerState TextureSampler : register(s0);

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float4 Color : COLOR;
    float2 TexCoord : TEXCOORD;

#if NORMAL_MAPPING
	// Data for normal mapping
	float3 Tangent : TANGENT0;
	float3 BiTangent : TANGENT1;
	float3 TangentNormal : TANGENT2;
#endif
};

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;

#if NORMAL_MAPPING
	// TBN 행렬 (TEXCOORD3~5로 자동 분할됨)
	row_major float3x3 TBN : TEXCOORD3;
#endif

#if LIGHTING_MODEL_GOURAUD
    float4 VertexColor : COLOR;
#endif
};

struct PS_OUTPUT
{
	float4 Color : SV_TARGET0;
	float4 Normal : SV_TARGET1;
};

// Helper Functions

// 픽셀의 타일 라이트 정보 반환
void GetTileLightInfo(float4 ScreenPos, out uint OutOffset, out uint OutPointCount, out uint OutSpotCount)
{
    // SV_POSITION.xy는 전체 화면 좌표 (SceneRT 내 절대 좌표)
    uint2 GlobalPixelCoord = uint2(ScreenPos.xy);
    uint2 GlobalTileCoord = GlobalPixelCoord / TILE_SIZE;

    // 전체 SceneRT 기준 타일 개수
    uint SceneNumTilesX = (uint(SceneRTSize.x) + TILE_SIZE - 1) / TILE_SIZE;
    uint SceneNumTilesY = (uint(SceneRTSize.y) + TILE_SIZE - 1) / TILE_SIZE;

    // 범위 체크 (전체 화면 기준)
    if (GlobalTileCoord.x >= SceneNumTilesX || GlobalTileCoord.y >= SceneNumTilesY)
    {
        OutOffset = 0;
        OutPointCount = 0;
        OutSpotCount = 0;
        return;
    }

    // 전체 화면 기준 타일 인덱스 계산
    uint TileIndex = GlobalTileCoord.y * SceneNumTilesX + GlobalTileCoord.x;

    // 타일의 라이트 정보 읽기
    OutOffset = TileLightOffsets[TileIndex];
    uint2 Counts = TileLightCounts[TileIndex];
    OutPointCount = Counts.x;
    OutSpotCount = Counts.y;
}

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

float3 CalculateBlinnPhongSpecular(float3 Normal, float3 LightDirection, float3 ViewDirection, float3 LightColor, float Intensity, float Shininess)
{
    // Blinn-Phong: Halfway vector 사용
    float3 HalfVector = normalize(LightDirection + ViewDirection);
    float NdotH = max(dot(Normal, HalfVector), 0.0);
    float SpecularFactor = pow(NdotH, Shininess);
    return LightColor * Intensity * SpecularFactor;
}

float3 CalculateDirectionalLight(FDirectionalLightInfo Light, float3 Normal)
{
    // Directional Light는 방향만 가지고 위치는 없음 (무한히 먼 광원)
    // Light.Direction은 빛이 향하는 방향이므로 음수를 취해 픽셀에서 빛으로 향하는 방향을 구함
    float3 LightDir = -normalize(Light.Direction);
    return CalculateLambertDiffuse(Normal, LightDir, Light.Color, Light.Intensity);
}

// Diffuse와 Specular를 분리하여 반환하는 버전
void CalculateDirectionalLightBlinnPhong(FDirectionalLightInfo Light, float3 Normal, float3 ViewDirection, float Shininess, out float3 OutDiffuse, out float3 OutSpecular)
{
    float3 LightDirection = -normalize(Light.Direction);
    float NdotL = max(dot(Normal, LightDirection), 0.0);

    // 뒷면이면 Diffuse와 Specular 모두 0
    if (NdotL <= 0.0)
    {
        OutDiffuse = float3(0, 0, 0);
        OutSpecular = float3(0, 0, 0);
        return;
    }

    OutDiffuse = Light.Color * Light.Intensity * NdotL;
    OutSpecular = CalculateBlinnPhongSpecular(Normal, LightDirection, ViewDirection, Light.Color, Light.Intensity, Shininess);
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

void CalculatePointLightBlinnPhong(FPointLightInfo Light, float3 WorldPosition, float3 Normal, float3 ViewDirection, float Shininess, out float3 OutDiffuse, out float3 OutSpecular)
{
    float3 LightVector = Light.Position - WorldPosition;
    float Distance = length(LightVector);

    if (Distance > Light.Radius)
    {
        OutDiffuse = float3(0, 0, 0);
        OutSpecular = float3(0, 0, 0);
        return;
    }

    float3 LightDirection = LightVector / Distance;
    float NdotL = max(dot(Normal, LightDirection), 0.0);

    // 뒷면이면 Diffuse와 Specular 모두 0
    if (NdotL <= 0.0)
    {
        OutDiffuse = float3(0, 0, 0);
        OutSpecular = float3(0, 0, 0);
        return;
    }

    float Attenuation = CalculateAttenuation(Distance, Light.Radius, Light.Falloff);

    OutDiffuse = Light.Color * Light.Intensity * NdotL * Attenuation;
    OutSpecular = CalculateBlinnPhongSpecular(Normal, LightDirection, ViewDirection, Light.Color, Light.Intensity, Shininess) * Attenuation;
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

void CalculateSpotLightBlinnPhong(FSpotLightInfo Light, float3 WorldPosition, float3 Normal, float3 ViewDirection, float Shininess, out float3 OutDiffuse, out float3 OutSpecular)
{
    float3 LightVector = Light.Position - WorldPosition;
    float Distance = length(LightVector);

    if (Distance > Light.Radius)
    {
        OutDiffuse = float3(0, 0, 0);
        OutSpecular = float3(0, 0, 0);
        return;
    }

    float3 LightDirection = normalize(LightVector);
    float NdotL = max(dot(Normal, LightDirection), 0.0);

    // 뒷면이면 Diffuse와 Specular 모두 0
    if (NdotL <= 0.0)
    {
        OutDiffuse = float3(0, 0, 0);
        OutSpecular = float3(0, 0, 0);
        return;
    }

    // Spot Cone Attenuation
    float CosAngle = dot(normalize(Light.Direction), -LightDirection);
    float CosInner = cos(Light.InnerConeAngle);
    float CosOuter = cos(Light.OuterConeAngle);

    if (CosAngle < CosOuter)
    {
        OutDiffuse = float3(0, 0, 0);
        OutSpecular = float3(0, 0, 0);
        return;
    }

    float SpotAttenuation = saturate((CosAngle - CosOuter) / max(CosInner - CosOuter, 0.0001));
    SpotAttenuation *=    SpotAttenuation;

    float DistAttenuation = CalculateAttenuation(Distance, Light.Radius, Light.Falloff);
    float TotalAttenuation = DistAttenuation * SpotAttenuation;

    OutDiffuse = Light.Color * Light.Intensity * NdotL * TotalAttenuation;
    OutSpecular = CalculateBlinnPhongSpecular(Normal, LightDirection, ViewDirection, Light.Color, Light.Intensity, Shininess) * TotalAttenuation;
}

// Helper function for coordinate system conversion

float3 NormalToUEBasis(float3 Position)
{
	return float3(Position.x, -Position.y, Position.z);
}

// Vertex Shader
PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT Output;

    // World-View-Projection 변환
    float4 WorldPosition = mul(float4(input.Position, 1.0), World);
    float4 ViewPosition = mul(WorldPosition, View);
    Output.Position = mul(ViewPosition, Projection);

    Output.WorldPos =   WorldPosition.xyz;
    Output.Normal = normalize(mul(float4(input.Normal, 0.0), WorldTransInv).xyz);
    Output.TexCoord = input.TexCoord;

#if NORMAL_MAPPING
	// TBN 행렬을 좌표계 변환 후 World Space로 변환
	// 1. 먼저 Local Space (오른손) → Local Space (왼손 Z-up) 좌표계 변환
	float3 TransformedTangent = NormalToUEBasis(input.Tangent);
	float3 TransformedBitangent = NormalToUEBasis(input.BiTangent);
	float3 TransformedNormal = NormalToUEBasis(input.TangentNormal);

	// 2. 그 다음 World Space로 변환
	// Tangent와 Bitangent는 World 행렬로 변환 (방향 벡터)
	float3 WorldTangent = normalize(mul(float4(TransformedTangent, 0.0), World).xyz);
	float3 WorldBitangent = normalize(mul(float4(TransformedBitangent, 0.0), World).xyz);
	// Normal은 WorldTransInv로 변환 (비균등 스케일 대응)
	float3 WorldNormal = normalize(mul(float4(TransformedNormal, 0.0), WorldTransInv).xyz);

	// TBN 행렬 구성 (row-major: 각 행이 World Space의 Tangent, Bitangent, Normal)
	Output.TBN = float3x3(
			WorldTangent,
			WorldBitangent,
			WorldNormal
		);

#endif

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: Vertex Shader에서 라이팅 계산
    float3 TotalLight = CalculateAmbientLight(Ambient);

    // Directional Light
    TotalLight += CalculateDirectionalLight(Directional, Output.Normal);

    // Point Lights
    for (uint i = 0; i < NumActivePointLights; ++i)
    {
        TotalLight += CalculatePointLight(PointLights[i], Output.WorldPos, Output.Normal);
    }

    // Spot Lights
    for (uint j = 0; j < NumActiveSpotLights; ++j)
    {
        TotalLight += CalculateSpotLight(SpotLights[j], Output.WorldPos, Output.Normal);
    }

    Output.VertexColor = float4(TotalLight, 1.0);
	//Output.VertexColor = float4(0, 0, 0, 1);
#endif

    return Output;
}

// Pixel Shader
PS_OUTPUT mainPS(PS_INPUT Input)
{
	PS_OUTPUT Output;
    // Texture Color 샘플링
    float4 BaseColor   = DiffuseTexture.Sample(TextureSampler, Input.TexCoord);

#if NORMAL_MAPPING
	// Normal Map 샘플링 (tangent space, [0,1] 범위)
	float3 TangentNormal = NormalMap.Sample(TextureSampler, Input.TexCoord).xyz;

	// [0,1] → [-1,1] 범위 변환
	TangentNormal = TangentNormal * 2.0 - 1.0;

	// TBN 행렬로 tangent space → world space 변환
	// TBN이 이미 좌표계 변환 및 World Space로 변환되어 있으므로, 추가 변환 불필요
	// TBN이 row-major이므로 벡터를 왼쪽에 배치
	float3 Normal = normalize(mul(TangentNormal, Input.TBN));

#else
	float3 Normal = normalize(Input.Normal);
#endif

#if LIGHTING_MODEL_GOURAUD
    // Gouraud Shading: VS에서 계산한 라이팅 사용
    // VertexColor.rgb = Ambient + Diffuse Light
    float3 VertexLighting = Input.VertexColor.rgb;
    float3 FinalColor = BaseColor.rgb * VertexLighting;
    Output.Color = float4(FinalColor, BaseColor.a);

#elif LIGHTING_MODEL_LAMBERT
    // Lambert Shading: PS에서 Diffuse 라이팅 계산
    float3 TotalLight = CalculateAmbientLight(Ambient);

    // Directional Light
    TotalLight += CalculateDirectionalLight(Directional, Normal);

    // 타일별 라이트 정보 가져오기
    uint TileOffset, PointCount, SpotCount;
    GetTileLightInfo(Input.Position, TileOffset, PointCount, SpotCount);

    // Point Lights (타일에 영향을 주는 라이트만)
    for (uint i = 0; i < PointCount; ++i)
    {
        uint LightIndex = TileLightIndices[TileOffset + i];
        TotalLight += CalculatePointLight(PointLights[LightIndex], Input.WorldPos, Normal);
    }

    // Spot Lights (타일에 영향을 주는 라이트만)
    for (uint j = 0; j < SpotCount; ++j)
    {
        uint LightIndex = TileLightIndices[TileOffset + PointCount + j];
        TotalLight += CalculateSpotLight(SpotLights[LightIndex], Input.WorldPos, Normal);
    }

    float3 FinalColor = BaseColor.rgb * TotalLight;
    Output.Color = float4(FinalColor, BaseColor.a);

#elif LIGHTING_MODEL_PHONG
    // Blinn-Phong Shading: PS에서 Diffuse + Specular 라이팅 계산
    float3 ViewDirection = normalize(CameraPosition - Input.WorldPos);

    // Ambient + Diffuse 라이팅 누적
    float3 DiffuseLight = CalculateAmbientLight(Ambient);

    // Specular 라이팅 누적
    float3 SpecularLight = float3(0, 0, 0);

    // Directional Light (Diffuse + Specular)
    float3 DirDiffuse, DirSpecular;
    CalculateDirectionalLightBlinnPhong(Directional, Normal, ViewDirection, Ns, DirDiffuse, DirSpecular);
    DiffuseLight += DirDiffuse;
    SpecularLight += DirSpecular;

    // 타일별 라이트 정보 가져오기
    uint TileOffset, PointCount, SpotCount;
    GetTileLightInfo(Input.Position, TileOffset, PointCount, SpotCount);

    // Point Lights (타일에 영향을 주는 라이트만)
    for (uint i = 0; i < PointCount; ++i)
    {
        uint LightIndex = TileLightIndices[TileOffset + i];
        float3 PointDiffuse, PointSpecular;
        CalculatePointLightBlinnPhong(PointLights[LightIndex], Input.WorldPos, Normal, ViewDirection, Ns, PointDiffuse, PointSpecular);
        DiffuseLight += PointDiffuse;
        SpecularLight += PointSpecular;
    }

    // Spot Lights (타일에 영향을 주는 라이트만)
    for (uint j = 0; j < SpotCount; ++j)
    {
        uint LightIndex = TileLightIndices[TileOffset + PointCount + j];
        float3 SpotDiffuse, SpotSpecular;
        CalculateSpotLightBlinnPhong(SpotLights[LightIndex], Input.WorldPos, Normal, ViewDirection, Ns, SpotDiffuse, SpotSpecular);
        DiffuseLight += SpotDiffuse;
        SpecularLight += SpotSpecular;
    }

    // Diffuse: BaseColor * DiffuseLight
    float3 DiffuseContribution = BaseColor.rgb * DiffuseLight;

    // Specular: Ks가 거의 0이면 Specular 없음
    float3 SpecularContribution = float3(0, 0, 0);
    if (Ks.r + Ks.g + Ks.b > 0.01)
    {
        SpecularContribution = Ks.rgb * SpecularLight;
    }

    // Final = Diffuse + Specular
    float3 FinalColor = DiffuseContribution + SpecularContribution;
    Output.Color = float4(FinalColor, BaseColor.a);

#else
    // No Lighting Model: Unlit (텍스처만 출력)
    Output.Color = BaseColor;
#endif

	float3 DebugNormal = normalize(Normal);
	Output.Normal = float4(DebugNormal * 0.5 + 0.5, 1.0);

	return Output;
}
