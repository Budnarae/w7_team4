cbuffer SceneDepthBuffer : register(b0)
{
	float near;
	float far;
	float2 ViewportTopLeft;
	float2 ViewportSize;
	float2 SceneRTSize;
};

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
	float2 depthUV = (ViewportTopLeft + In.texCoord * ViewportSize) / SceneRTSize;

	float NdcZ = SceneDepthTexture.Sample(DefaultSampler, depthUV).r;

	// NdcZ는 비선형 깊이이므로 view 공간에서의 Depth를 복원한 후
	float LinearDepth = (near * far) / (far - NdcZ * (far - near));

	// [0, 1] 범위로 정규
	LinearDepth = (LinearDepth - near) / (far - near);

	// 부동소수점 오차 방지를 위한 clamp
	LinearDepth = clamp(LinearDepth, 0.0f, 1.0f);

	float4 finalDepthColor = float4(LinearDepth, LinearDepth, LinearDepth, 1.0f);
	return finalDepthColor;
}
