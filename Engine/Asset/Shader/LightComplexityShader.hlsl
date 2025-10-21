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
    uint2 _Padding;
};

// Tile Light Masks (Tiled-Based Light Culling 결과)
StructuredBuffer<uint> TileLightMasks : register(t0);

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
    // 0 = 검은색 (라이트 없음)
    // 1~3 = 파랑
    // 4~6 = 시안~초록
    // 7~10 = 노랑
    // 11~16 = 주황
    // 17+ = 빨강

    if (LightCount == 0)
    {
        return float3(0, 0, 0);  // 검은색 (라이트 없음)
    }

    float t;
    if (LightCount <= 10)
    {
        t = (float)LightCount / 10.0 * 0.6;  // 0~10 라이트 = 색상 스펙트럼의 60%
    }
    else
    {
        t = 0.6 + ((float)(LightCount - 10) / 22.0) * 0.4;  // 11~32 라이트 = 나머지 40%
    }

    t = saturate(t);

    // Heat map: Blue -> Cyan -> Green -> Yellow -> Orange -> Red
    if (t < 0.2)
    {
        // Blue to Cyan
        float localT = t / 0.2;
        return float3(0, localT * 0.7, 1);
    }
    else if (t < 0.4)
    {
        // Cyan to Green
        float localT = (t - 0.2) / 0.2;
        return float3(0, 0.7 + localT * 0.3, 1 - localT);
    }
    else if (t < 0.6)
    {
        // Green to Yellow
        float localT = (t - 0.4) / 0.2;
        return float3(localT, 1, 0);
    }
    else if (t < 0.8)
    {
        // Yellow to Orange
        float localT = (t - 0.6) / 0.2;
        return float3(1, 1 - localT * 0.5, 0);
    }
    else
    {
        // Orange to Red
        float localT = (t - 0.8) / 0.2;
        return float3(1, 0.5 - localT * 0.5, 0);
    }
}

// Pixel Shader
float4 mainPS(VSOutput Input) : SV_TARGET
{
    // 현재 픽셀의 화면 좌표
    uint2 PixelPos = uint2(Input.TexCoord * ScreenDimensions);

    // 타일 인덱스 계산
    uint2 TileID = PixelPos / TILE_SIZE;
    uint TileIndex = TileID.y * NumTilesX + TileID.x;

    // 타일별 라이트 마스크 읽기
    uint PointLightMask = TileLightMasks[TileIndex * 2 + 0];
    uint SpotLightMask = TileLightMasks[TileIndex * 2 + 1];

    // 비트 카운트 (해당 타일에 영향을 주는 라이트 개수)
    uint PointLightCount = countbits(PointLightMask);
    uint SpotLightCount = countbits(SpotLightMask);
    uint TotalLightCount = PointLightCount + SpotLightCount;

    // 라이트 개수를 색상으로 변환
    float3 Color = LightCountToColor(TotalLightCount);

    // Alpha = 0.5 (적절한 투명도로 메시와 heat map 모두 보이게)
    return float4(Color, 0.5);
}
