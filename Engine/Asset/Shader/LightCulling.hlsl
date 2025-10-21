// Light Culling Compute Shader
// 화면을 타일로 나누고 각 타일에 영향을 주는 라이트만 선별

#define TILE_SIZE 32
#define MAX_LIGHTS_PER_TILE 256

// Constant Buffers
cbuffer PerFrame : register(b0)
{
    row_major float4x4 ViewMatrix;
    row_major float4x4 ProjectionMatrix;
    row_major float4x4 InverseProjectionMatrix;
    float2 ScreenDimensions;  // Screen width, height
    uint2 NumTiles;           // Number of tiles (x, y)
    uint NumPointLights;
    uint NumSpotLights;
    float NearPlane;          // Camera near plane
    float FarPlane;           // Camera far plane
};

// Light Structures
struct FPointLightInfo
{
    float3 Position;
    float Intensity;
    float3 Color;
    float Radius;
    float Falloff;
    float3 _Padding;
};

struct FSpotLightInfo
{
    float3 Position;
    float Intensity;
    float3 Direction;
    float Radius;
    float3 Color;
    float InnerConeAngle;
    float OuterConeAngle;
    float Falloff;
    float2 _Padding;
};

// Input Resources
Texture2D<float> SceneDepthTexture : register(t0);
StructuredBuffer<FPointLightInfo> PointLights : register(t1);
StructuredBuffer<FSpotLightInfo> SpotLights : register(t2);

// Output: 타일별 Light 비트마스크
// TileLightMasks[TileIndex * 2 + 0] = PointLight 비트마스크
// TileLightMasks[TileIndex * 2 + 1] = SpotLight 비트마스크
RWStructuredBuffer<uint> TileLightMasks : register(u0);

// Shared Memory for min/max depth reduction
// 각 타일(Thread Group) 내 1024개 스레드가 공유 (Reduction 필요)
#define TILE_THREAD_COUNT (TILE_SIZE * TILE_SIZE)
groupshared float SharedDepths[TILE_THREAD_COUNT];
groupshared float SharedMinDepth;
groupshared float SharedMaxDepth;

// NDC 좌표를 View Space로 변환
float3 ScreenToView(float2 ScreenPos, float Depth)
{
    // Screen space to NDC
    float2 NDC = float2(
        (ScreenPos.x / ScreenDimensions.x) * 2.0 - 1.0,
        1.0 - (ScreenPos.y / ScreenDimensions.y) * 2.0
    );

    // NDC to View space
    float4 ViewPos = mul(float4(NDC, Depth, 1.0), InverseProjectionMatrix);
    ViewPos.xyz /= ViewPos.w;

    return ViewPos.xyz;
}

// Sphere와 Frustum의 충돌 검사
// MinDepthValue/MaxDepthValue는 타일의 Depth 범위 (0~1)
// ViewSpaceCenter.z는 View Space Z (Left-handed: 카메라 앞이 양수)
bool SphereIntersectsFrustum(float3 ViewSpaceCenter, float Radius, float MinDepthValue, float MaxDepthValue, float2 TileMin, float2 TileMax)
{
    // 카메라 뒤 체크 제거
    // Light의 영향 범위(Radius)가 카메라 앞까지 닿을 수 있으므로
    // 단순히 Light Center가 뒤에 있다고 Culling하면 안됨

    float LightViewZ = ViewSpaceCenter.z;

    // Perspective projection depth 값을 View Space Z로 정확히 역변환
    // Standard perspective projection: ViewZ = (Near * Far) / (Far - Depth * (Far - Near))
    float TileMinZ = (NearPlane * FarPlane) / (FarPlane - MinDepthValue * (FarPlane - NearPlane));
    float TileMaxZ = (NearPlane * FarPlane) / (FarPlane - MaxDepthValue * (FarPlane - NearPlane));

    // Light Sphere의 depth 범위 (Radius 고려)
    float LightNearZ = LightViewZ - Radius;
    float LightFarZ = LightViewZ + Radius;

    // Light Sphere가 카메라 앞 영역과 겹치는지 체크
    // LightFarZ > 0 이면 Light가 카메라 앞까지 영향을 줌
    if (LightFarZ < 0.0)
    {
        return false; // Light 전체가 카메라 뒤라면 컬링
    }

    // 타일의 Depth 범위와 겹치는지 체크
    if (LightFarZ < TileMinZ || LightNearZ > TileMaxZ)
    {
        return false;
    }

    return true;
}

// Point Light를 World Space에서 View Space로 변환
float3 WorldToView(float3 WorldPos)
{
    float4 ViewPos = mul(float4(WorldPos, 1.0), ViewMatrix);
    return ViewPos.xyz;
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void mainCS(
    uint3 GroupID : SV_GroupID,
    uint3 GroupThreadID : SV_GroupThreadID,
    uint3 DispatchThreadID : SV_DispatchThreadID,
    uint GroupIndex : SV_GroupIndex
)
{
    // 각 스레드가 Depth 읽기 (Out-of-Bounds 안전)
    uint2 PixelPos = DispatchThreadID.xy;
    float Depth = 1.0; // 기본값: Far plane

    if (PixelPos.x < uint(ScreenDimensions.x) && PixelPos.y < uint(ScreenDimensions.y))
    {
        Depth = SceneDepthTexture.Load(int3(PixelPos, 0));
    }

    // Shared memory에 저장 (각 스레드가 자신의 인덱스에만 씀 - race condition 없음)
    SharedDepths[GroupIndex] = Depth;

    GroupMemoryBarrierWithGroupSync();

    // Parallel Reduction으로 Min/Max 계산 (첫 번째 스레드만)
    if (GroupIndex == 0)
    {
        float MinDepth = 1.0;
        float MaxDepth = 0.0;

        // 모든 스레드의 Depth를 순회하며 Min/Max 계산
        for (uint i = 0; i < TILE_THREAD_COUNT; ++i)
        {
            MinDepth = min(MinDepth, SharedDepths[i]);
            MaxDepth = max(MaxDepth, SharedDepths[i]);
        }

        SharedMinDepth = MinDepth;
        SharedMaxDepth = MaxDepth;
    }

    GroupMemoryBarrierWithGroupSync();

    // 타일의 Frustum 계산 (코너 4개의 View Space 좌표)
    float MinDepth = SharedMinDepth;
    float MaxDepth = SharedMaxDepth;

    // 타일의 화면 공간 경계
    float2 TileMin = float2(GroupID.xy) * TILE_SIZE;
    float2 TileMax = TileMin + TILE_SIZE;

    // 타일 인덱스 계산
    uint TileIndex = GroupID.y * NumTiles.x + GroupID.x;

    // Point Lights Culling
    for (uint i = GroupIndex; i < NumPointLights; i += TILE_THREAD_COUNT)
    {
        FPointLightInfo Light = PointLights[i];

        // Light를 View Space로 변환
        float3 ViewSpaceLightPos = WorldToView(Light.Position);

        // Frustum 충돌 검사
        if (SphereIntersectsFrustum(ViewSpaceLightPos, Light.Radius, MinDepth, MaxDepth, TileMin, TileMax))
        {
            // 타일별 비트마스크에 해당 Light 비트 설정 (Atomic OR)
            InterlockedOr(TileLightMasks[TileIndex * 2 + 0], 1u << i);
        }
    }

    // Spot Lights Culling (간단히 Sphere로 근사)
    for (uint j = GroupIndex; j < NumSpotLights; j += TILE_THREAD_COUNT)
    {
        FSpotLightInfo Light = SpotLights[j];

        float3 ViewSpaceLightPos = WorldToView(Light.Position);

        if (SphereIntersectsFrustum(ViewSpaceLightPos, Light.Radius, MinDepth, MaxDepth, TileMin, TileMax))
        {
            // 타일별 비트마스크에 해당 Light 비트 설정 (Atomic OR)
            InterlockedOr(TileLightMasks[TileIndex * 2 + 1], 1u << j);
        }
    }
}
