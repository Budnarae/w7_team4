// Light Complexity Visualization Shader
// 타일별 라이트 개수를 색상으로 시각화

#define TILE_SIZE 32
#define MAX_LIGHTS 32  // Point + Spot 합산 최대

cbuffer PerFrame : register(b0)
{
    float2 ScreenDimensions;
    uint NumTilesX;
    uint NumTilesY;
    uint NumPointLights;
    uint NumSpotLights;
    uint PointLightUsageMask;
    uint SpotLightUsageMask;
};

// Light Info (CPU에서 전달받지 않고 간단한 시각화만)
// 실제 Forward+ 구현을 위해서는 StructuredBuffer<LightInfo> 필요

// Fullscreen Quad Vertex Shader
struct VSOutput
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

VSOutput mainVS(uint VertexID : SV_VertexID)
{
    VSOutput Output;

    // Fullscreen triangle
    Output.TexCoord = float2((VertexID << 1) & 2, VertexID & 2);
    Output.Position = float4(Output.TexCoord * float2(2, -2) + float2(-1, 1), 0, 1);

    return Output;
}

// 라이트 개수를 Heat Map 색상으로 변환
float3 LightCountToColor(uint LightCount)
{
    // 0 라이트 = 흰색 (라이트 없음을 명확히 표시)
    // 1~8 = 파랑 -> 초록
    // 9~16 = 초록 -> 노랑
    // 17~24 = 노랑 -> 주황
    // 25~32 = 주황 -> 빨강

    if (LightCount == 0)
        return float3(1, 1, 1);

    float t = saturate((float)LightCount / MAX_LIGHTS);

    // Heat map: Blue -> Cyan -> Green -> Yellow -> Red
    if (t < 0.25)
    {
        // Blue to Cyan
        float localT = t / 0.25;
        return float3(0, localT, 1);
    }
    else if (t < 0.5)
    {
        // Cyan to Green
        float localT = (t - 0.25) / 0.25;
        return float3(0, 1, 1 - localT);
    }
    else if (t < 0.75)
    {
        // Green to Yellow
        float localT = (t - 0.5) / 0.25;
        return float3(localT, 1, 0);
    }
    else
    {
        // Yellow to Red
        float localT = (t - 0.75) / 0.25;
        return float3(1, 1 - localT, 0);
    }
}

// Pixel Shader
float4 mainPS(VSOutput Input) : SV_TARGET
{
    // 현재 픽셀의 화면 좌표
    uint2 PixelPos = uint2(Input.TexCoord * ScreenDimensions);

    // 타일 인덱스 계산
    uint2 TileID = PixelPos / TILE_SIZE;

    // 타일별로 다른 Light 개수를 시각화하기 위한 간단한 알고리즘
    // 실제 Forward+에서는 각 타일의 Light List를 사용하지만,
    // 현재는 글로벌 Mask만 있으므로 타일 ID 기반으로 변화를 주어 시각화

    uint TotalLights = NumPointLights + NumSpotLights;

    // 타일 ID 해시 (간단한 pseudo-random)
    uint TileHash = (TileID.x * 73 + TileID.y * 151) % 16;

    // 타일마다 다른 비율의 라이트 표시 (중앙에 가까울수록 많이)
    float2 TileCenter = (float2(TileID) + 0.5) * TILE_SIZE;
    float2 ScreenCenter = ScreenDimensions * 0.5;
    float DistToCenter = length(TileCenter - ScreenCenter);
    float MaxDist = length(ScreenDimensions * 0.5);

    // 중앙에서 멀수록 라이트 개수 감소 (간단한 시각화)
    float DistFactor = 1.0 - saturate(DistToCenter / MaxDist);
    uint ActiveLightCount = uint(float(TotalLights) * (0.3 + DistFactor * 0.7));

    // 최소 1개는 보이도록 (0이면 흰색)
    if (TotalLights > 0 && ActiveLightCount == 0)
        ActiveLightCount = 1;

    // 라이트 개수를 색상으로 변환
    float3 Color = LightCountToColor(ActiveLightCount);

    // 타일 경계선 그리기 (선택사항)
    uint2 LocalPixel = PixelPos % TILE_SIZE;
    if (LocalPixel.x == 0 || LocalPixel.y == 0)
    {
        Color *= 0.7; // 경계선을 약간 어둡게
    }

    // Alpha = 0.3 (더 투명하게, 메시를 더 잘 보이게)
    return float4(Color, 0.3);
}
