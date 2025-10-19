/*
cbuffer constants : register(b0)
{
	row_major float4x4 world;
}

cbuffer PerFrame : register(b1)
{
	row_major float4x4 View;		// View Matrix Calculation of MVP Matrix
	row_major float4x4 Projection;	// Projection Matrix Calculation of MVP Matrix
};

struct VS_INPUT
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float4 color : COLOR;
	float2 tex : TEXCOORD0;
};

struct PS_INPUT
{
	float4 position : SV_POSITION;
	float depth : TEXCOORD0;		// Linear depth for visualization
};

PS_INPUT mainVS(VS_INPUT input)
{
	PS_INPUT output;

	float4 tmp = float4(input.position, 1.0f);
	tmp = mul(tmp, world);
	tmp = mul(tmp, View);

	// Store view-space depth before projection
	output.depth = tmp.z;

	tmp = mul(tmp, Projection);
	output.position = tmp;

	return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
	// Normalize depth to 0-1 range
	// Assuming typical far plane is around 1000-10000 units
	float normalizedDepth = (input.depth % 100.0f) / 100.0f;

	// Create color gradient from near (dark blue) to far (white)
	float3 nearColor = float3(0.0f, 0.0f, 0.0f);	// Dark blue
	float3 farColor = float3(1.0f, 1.0f, 1.0f);		// White

	float3 depthColor = lerp(nearColor, farColor, normalizedDepth);

	return float4(depthColor, 1.0f);
}
*/

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
	//float2 depthUV = (ViewportTopLeft + In.texCoord * ViewportSize) / SceneRTSize;
	float2 depthUV = In.texCoord;

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
