cbuffer NormalBuffer : register(b0)
{
	float2 ViewportTopLeft;
	float2 ViewportSize;
	float2 SceneRTSize;
	float2 Padding;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float2 texCoord : TEXCOORD0;
};

Texture2D SceneNormalTexture : register(t0);
SamplerState DefaultSampler : register(s0);

PS_INPUT mainVS(uint VertexID : SV_VertexID)
{
	PS_INPUT Out;
	float2 pos[3] =
	{
		float2(-1.0f, 3.0f),
		float2(3.0f, -1.0f),
		float2(-1.0f, -1.0f)
	};

	Out.position = float4(pos[VertexID], 0.1f, 1.0f);
	Out.texCoord = (pos[VertexID] + 1.0) * 0.5; // [-1,1] → [0,1]
	Out.texCoord.y = 1.0 - Out.texCoord.y;      // Y축 뒤집기
	return Out;
}

float4 mainPS(PS_INPUT In) : SV_TARGET
{
	// Viewport 화면비를 반영한 UV 계산
	float2 NormalUV = (ViewportTopLeft + In.texCoord * ViewportSize) / SceneRTSize;

	return SceneNormalTexture.Sample(DefaultSampler, NormalUV);
}
