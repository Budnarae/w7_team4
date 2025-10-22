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
    float2 ScreenDimensions;  // 현재 Viewport 크기
    uint2 NumTiles;           // 현재 Viewport 타일 개수
    uint2 ViewportOffset;     // SceneDepth 텍스처 내 Viewport 시작 위치 (픽셀)
    uint2 SceneRTSize;        // 전체 SceneDepth 텍스처 크기 (픽셀)
    uint NumPointLights;
    uint NumSpotLights;
    float NearPlane;
    float FarPlane;
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

// Output: 타일별 Light 인덱스 리스트
RWStructuredBuffer<uint> TileLightOffsets : register(u0);  // 각 타일의 라이트 리스트 시작 오프셋
RWStructuredBuffer<uint2> TileLightCounts : register(u1);  // 각 타일의 (PointLight 개수, SpotLight 개수)
RWStructuredBuffer<uint> TileLightIndices : register(u2);  // 전역 라이트 인덱스 배열

// Shared Memory for min/max depth reduction
#define TILE_THREAD_COUNT (TILE_SIZE * TILE_SIZE)
#define MAX_LIGHTS_PER_TILE 256
groupshared float SharedMinDepths[TILE_THREAD_COUNT];
groupshared float SharedMaxDepths[TILE_THREAD_COUNT];
groupshared float SharedMinDepth;
groupshared float SharedMaxDepth;

// Shared memory for light culling
groupshared uint SharedPointLightIndices[MAX_LIGHTS_PER_TILE];
groupshared uint SharedSpotLightIndices[MAX_LIGHTS_PER_TILE];
groupshared uint SharedPointLightCount;
groupshared uint SharedSpotLightCount;

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

// 평면 정의 (Plane: normal.xyz, distance)
struct Plane
{
    float3 Normal;
    float Distance;  // 원점으로부터의 거리
};

// 평면 생성: 3개의 점으로부터 (반시계방향)
Plane CreatePlane(float3 p0, float3 p1, float3 p2)
{
    Plane plane;
    float3 v0 = p1 - p0;
    float3 v1 = p2 - p0;

    // 외적으로 법선 벡터 계산 (Frustum 내부를 향하도록)
    plane.Normal = normalize(cross(v1, v0));

    // 평면 방정식: dot(Normal, P) + Distance = 0
    // Distance = -dot(Normal, p0)
    plane.Distance = -dot(plane.Normal, p0);

    return plane;
}

// 구와 평면의 거리 계산
float DistanceToPlane(Plane plane, float3 p)
{
    // 양수: 평면의 Normal 방향 (Frustum 외부)
    // 음수: 평면의 반대 방향 (Frustum 내부)
    return dot(plane.Normal, p) + plane.Distance;
}

// Sphere와 Tile Frustum의 충돌 검사 (6개 평면 사용)
bool SphereIntersectsFrustum(float3 ViewSpaceCenter, float Radius, float MinDepthValue, float MaxDepthValue, float2 TileMin, float2 TileMax)
{
    // MinDepth > MaxDepth이면 빈 타일
    if (MinDepthValue > MaxDepthValue)
    {
        return false;
    }

    // Perspective depth에서 View Space Z로 변환
    float TileMinZ = (NearPlane * FarPlane) / (FarPlane - MinDepthValue * (FarPlane - NearPlane));
    float TileMaxZ = (NearPlane * FarPlane) / (FarPlane - MaxDepthValue * (FarPlane - NearPlane));

    // Light 전체가 카메라 뒤라면 컬링
    float LightViewZ = ViewSpaceCenter.z;
    float LightFarZ = LightViewZ + Radius;
    if (LightFarZ < 0.0)
    {
        return false;
    }

    // Frustum의 8개 코너 계산
    float3 NearCorners[4];
    NearCorners[0] = ScreenToView(float2(TileMin.x, TileMin.y), 0.0f);  // 좌하
    NearCorners[1] = ScreenToView(float2(TileMax.x, TileMin.y), 0.0f);  // 우하
    NearCorners[2] = ScreenToView(float2(TileMax.x, TileMax.y), 0.0f);  // 우상
    NearCorners[3] = ScreenToView(float2(TileMin.x, TileMax.y), 0.0f);  // 좌상

    float3 FarCorners[4];
    FarCorners[0] = ScreenToView(float2(TileMin.x, TileMin.y), 1.0f);
    FarCorners[1] = ScreenToView(float2(TileMax.x, TileMin.y), 1.0f);
    FarCorners[2] = ScreenToView(float2(TileMax.x, TileMax.y), 1.0f);
    FarCorners[3] = ScreenToView(float2(TileMin.x, TileMax.y), 1.0f);

    // Frustum의 6개 평면 생성
    Plane planes[6];

    // Near Plane (법선이 카메라 반대 방향)
    planes[0] = CreatePlane(NearCorners[0], NearCorners[3], NearCorners[1]);

    // Far Plane (법선이 카메라 방향)
    planes[1] = CreatePlane(FarCorners[0], FarCorners[1], FarCorners[3]);

    // Left Plane (좌측 면)
    planes[2] = CreatePlane(NearCorners[0], FarCorners[0], NearCorners[3]);

    // Right Plane (우측 면)
    planes[3] = CreatePlane(NearCorners[1], NearCorners[2], FarCorners[1]);

    // Bottom Plane (하단 면)
    planes[4] = CreatePlane(NearCorners[0], NearCorners[1], FarCorners[0]);

    // Top Plane (상단 면)
    planes[5] = CreatePlane(NearCorners[3], FarCorners[3], NearCorners[2]);

    // 6개 평면 모두에 대해 충돌 검사
    [unroll]
    for (int i = 0; i < 6; ++i)
    {
        float distance = DistanceToPlane(planes[i], ViewSpaceCenter);

        // 구의 중심이 평면 밖에 있고, 반지름보다 멀리 있으면
        // 이 평면 기준으로 Frustum 완전히 벗어남
        if (distance > 0.0f && distance - Radius >= 0.0f)
        {
            return false;  // 이 평면 밖 → Frustum과 충돌 없음
        }
    }

    // 모든 평면에 대해 충돌 또는 내부 → Frustum과 겹침
    return true;
}

// World Space에서 View Space로 변환
float3 WorldToView(float3 WorldPos)
{
    float4 ViewPos = mul(float4(WorldPos, 1.0), ViewMatrix);
    return ViewPos.xyz;
}

// Parallel Reduction Helper
void ReduceMinMaxDepth(uint ThreadIndex, uint Stride)
{
    if (ThreadIndex < Stride)
    {
        SharedMinDepths[ThreadIndex] = min(SharedMinDepths[ThreadIndex], SharedMinDepths[ThreadIndex + Stride]);
        SharedMaxDepths[ThreadIndex] = max(SharedMaxDepths[ThreadIndex], SharedMaxDepths[ThreadIndex + Stride]);
    }
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void mainCS(
    uint3 GroupID : SV_GroupID,
    uint3 GroupThreadID : SV_GroupThreadID,
    uint3 DispatchThreadID : SV_DispatchThreadID,
    uint GroupIndex : SV_GroupIndex
)
{
    // 각 스레드가 Depth 읽기
    // DispatchThreadID는 뷰포트 로컬 좌표 (0,0 = 뷰포트 왼쪽 위)
    // SceneDepth 텍스처 좌표를 얻기 위해 ViewportOffset 추가
    uint2 ViewportLocalPos = DispatchThreadID.xy;
    uint2 GlobalPixelPos = ViewportLocalPos + ViewportOffset;

    // Shared memory 초기화
    SharedMinDepths[GroupIndex] = 1.0;  // Min은 최대값으로 시작
    SharedMaxDepths[GroupIndex] = 0.0;  // Max는 최소값으로 시작

    // 유효한 픽셀인 경우에만 실제 depth로 업데이트
    if (ViewportLocalPos.x < uint(ScreenDimensions.x) && ViewportLocalPos.y < uint(ScreenDimensions.y) &&
        GlobalPixelPos.x < SceneRTSize.x && GlobalPixelPos.y < SceneRTSize.y)
    {
        float Depth = SceneDepthTexture.Load(int3(GlobalPixelPos, 0));
        SharedMinDepths[GroupIndex] = Depth;
        SharedMaxDepths[GroupIndex] = Depth;
    }

    GroupMemoryBarrierWithGroupSync();

    // Parallel Reduction
    ReduceMinMaxDepth(GroupIndex, 512);
    GroupMemoryBarrierWithGroupSync();
    ReduceMinMaxDepth(GroupIndex, 256);
    GroupMemoryBarrierWithGroupSync();
    ReduceMinMaxDepth(GroupIndex, 128);
    GroupMemoryBarrierWithGroupSync();
    ReduceMinMaxDepth(GroupIndex, 64);
    GroupMemoryBarrierWithGroupSync();
    ReduceMinMaxDepth(GroupIndex, 32);
    GroupMemoryBarrierWithGroupSync();

    // Warp 내부 처리
    ReduceMinMaxDepth(GroupIndex, 16);
    ReduceMinMaxDepth(GroupIndex, 8);
    ReduceMinMaxDepth(GroupIndex, 4);
    ReduceMinMaxDepth(GroupIndex, 2);
    ReduceMinMaxDepth(GroupIndex, 1);

    // Thread 0이 최종 결과 저장
    if (GroupIndex == 0)
    {
        SharedMinDepth = SharedMinDepths[0];
        SharedMaxDepth = SharedMaxDepths[0];
    }

    GroupMemoryBarrierWithGroupSync();

    // 타일의 Frustum 계산 (코너 4개의 View Space 좌표)
    float MinDepth = SharedMinDepth;
    float MaxDepth = SharedMaxDepth;

    // 타일의 화면 공간 경계 (뷰포트 로컬 좌표)
    float2 TileMin = float2(GroupID.xy) * TILE_SIZE;
    float2 TileMax = TileMin + TILE_SIZE;

    // 전체 화면 기준 타일 좌표 계산 (각 뷰포트가 버퍼의 다른 영역 사용)
    uint2 GlobalTileCoord = uint2(ViewportOffset) / TILE_SIZE + GroupID.xy;

    // 전체 SceneRT 기준 타일 개수
    uint SceneNumTilesX = (SceneRTSize.x + TILE_SIZE - 1) / TILE_SIZE;
    uint SceneNumTilesY = (SceneRTSize.y + TILE_SIZE - 1) / TILE_SIZE;

    // 범위 체크 및 TileIndex 계산
    uint TileIndex = 0;
    bool bValidTile = (GlobalTileCoord.x < SceneNumTilesX && GlobalTileCoord.y < SceneNumTilesY);

    if (bValidTile)
    {
        TileIndex = GlobalTileCoord.y * SceneNumTilesX + GlobalTileCoord.x;
    }

    // Geometry 존재 여부 체크
    // MinDepth > MaxDepth: 모든 픽셀이 범위 밖
    // MinDepth >= 1.0: 모든 픽셀이 배경 (FarPlane)
    // !bValidTile: 타일이 Scene RT 범위 밖
    if (MinDepth > MaxDepth || MinDepth >= 1.0 || !bValidTile)
    {
        // 빈 타일 - 라이트 없음으로 처리하고 early return
        if (GroupIndex == 0 && bValidTile)
        {
            TileLightOffsets[TileIndex] = 0;
            TileLightCounts[TileIndex] = uint2(0, 0);
        }
        return;
    }

    // Thread 0이 shared memory 초기화
    if (GroupIndex == 0)
    {
        SharedPointLightCount = 0;
        SharedSpotLightCount = 0;
    }
    GroupMemoryBarrierWithGroupSync();

    // Point Lights Culling
    for (uint i = GroupIndex; i < NumPointLights; i += TILE_THREAD_COUNT)
    {
        FPointLightInfo Light = PointLights[i];

        // Light를 View Space로 변환
        float3 ViewSpaceLightPos = WorldToView(Light.Position);

        // Frustum 충돌 검사
        if (SphereIntersectsFrustum(ViewSpaceLightPos, Light.Radius, MinDepth, MaxDepth, TileMin, TileMax))
        {
            // Shared memory에 라이트 인덱스 추가 (Atomic)
            uint LocalIndex;
            InterlockedAdd(SharedPointLightCount, 1, LocalIndex);
            if (LocalIndex < MAX_LIGHTS_PER_TILE)
            {
                SharedPointLightIndices[LocalIndex] = i;
            }
        }
    }

    // Spot Lights Culling
    for (uint j = GroupIndex; j < NumSpotLights; j += TILE_THREAD_COUNT)
    {
        FSpotLightInfo Light = SpotLights[j];

        float3 ViewSpaceLightPos = WorldToView(Light.Position);

        if (SphereIntersectsFrustum(ViewSpaceLightPos, Light.Radius, MinDepth, MaxDepth, TileMin, TileMax))
        {
            // Shared memory에 라이트 인덱스 추가 (Atomic)
            uint LocalIndex;
            InterlockedAdd(SharedSpotLightCount, 1, LocalIndex);
            if (LocalIndex < MAX_LIGHTS_PER_TILE)
            {
                SharedSpotLightIndices[LocalIndex] = j;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // Thread 0이 글로벌 버퍼에 결과 쓰기
    if (GroupIndex == 0)
    {
        uint PointCount = min(SharedPointLightCount, MAX_LIGHTS_PER_TILE);
        uint SpotCount = min(SharedSpotLightCount, MAX_LIGHTS_PER_TILE);

        // 글로벌 인덱스 배열에서 오프셋 할당
        uint GlobalOffset;
        InterlockedAdd(TileLightOffsets[0], PointCount + SpotCount, GlobalOffset);

        // 타일의 오프셋과 개수 저장
        TileLightOffsets[TileIndex] = GlobalOffset;
        TileLightCounts[TileIndex] = uint2(PointCount, SpotCount);

        // Point Light 인덱스 복사
        for (uint p = 0; p < PointCount; ++p)
        {
            TileLightIndices[GlobalOffset + p] = SharedPointLightIndices[p];
        }

        // Spot Light 인덱스 복사
        for (uint s = 0; s < SpotCount; ++s)
        {
            TileLightIndices[GlobalOffset + PointCount + s] = SharedSpotLightIndices[s];
        }
    }
}
