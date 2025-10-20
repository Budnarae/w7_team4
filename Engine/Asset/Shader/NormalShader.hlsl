struct PS_INPUT
{
	float4 position : SV_POSITION; // Transformed position to pass to the pixel shader
	float2 texCoord : TEXCOORD0; // 추가!
};

Texture2D SceneDepthTexture : register(t0);
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
	//float2 depthUV = (ViewportTopLeft + In.texCoord * ViewportSize) / SceneRTSize;
	float2 NormalUV = In.texCoord;

	return SceneDepthTexture.Sample(DefaultSampler, NormalUV);
}
