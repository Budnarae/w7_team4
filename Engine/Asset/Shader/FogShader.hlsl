cbuffer FogParameters : register(b0)
{
	float2 ViewportTopLeft;
	float2 ViewportSize;
	float2 SceneRTSize;
	float2 Padding2;

	// Fog Parameters
	float FogDensity;
	float FogHeightFalloff;
	float StartDistance;
	float FogCutoffDistance;

	float FogMaxOpacity;
	float3 FogInscatteringColor;

	float3 CameraPosition;
	float FogHeight;

	row_major float4x4 InvViewProj;
};

struct PS_INPUT
{
	float4 Position : SV_Position;
	float2 UV : TEXCOORD0;
};

struct PS_OUTPUT
{
	float4 Color : SV_TARGET;
	float Depth : SV_DEPTH;
};

Texture2D SceneTexture : register(t0);
Texture2D SceneDepthTexture : register(t1);
SamplerState LinearClamp : register(s0);

// Fullscreen Quad Vertex Shader
PS_INPUT mainVS(uint VertexID : SV_VertexID)
{
	PS_INPUT Out;
	float2 pos[3] =
	{
		float2(-1.0f, 3.0f),
		float2(3.0f, -1.0f),
		float2(-1.0f, -1.0f)
	};

	Out.Position = float4(pos[VertexID], 0.1f, 1.0f);
	Out.UV = (pos[VertexID] + float2(1.0f, -1.0f)) * float2(0.5f, -0.5f);

	return Out;
}

// Apply Fog (same logic as HeightFogShader.hlsl)
float3 ApplyFog(float3 SceneColor, float Depth, float2 viewportUV)
{
	// If FogDensity is 0, fog is disabled
	if (FogDensity <= 0.0)
	{
		return SceneColor;
	}

	// Reconstruct World Position from Depth
	// IMPORTANT: Use viewportUV (0~1 within viewport) instead of sceneUV
	float2 screenPos = viewportUV * 2.0 - 1.0;
	screenPos.y = -screenPos.y;
	float4 clipPos = float4(screenPos, Depth, 1.0);
	float4 worldPos = mul(clipPos, InvViewProj);
	worldPos /= worldPos.w;

	float3 worldPosition = worldPos.xyz;
	float DistanceFromCamera = length(worldPosition - CameraPosition);

	// Apply Start Distance
	float fogDistance = max(0.0, DistanceFromCamera - StartDistance);

	// Exponential Height Fog - Line Integral
	float CameraHeight = CameraPosition.z;
	float PixelHeight = worldPosition.z;
	float Falloff = FogHeightFalloff * (PixelHeight - CameraHeight);
	float LineIntegral;

	if (abs(Falloff) > 0.01)
	{
		float ExpStart = exp(-FogHeightFalloff * (CameraHeight - FogHeight));
		float ExpEnd = exp(-FogHeightFalloff * (PixelHeight - FogHeight));
		LineIntegral = (ExpStart - ExpEnd) / Falloff;
	}
	else
	{
		float AvgHeight = (CameraHeight + PixelHeight) * 0.5;
		LineIntegral = exp(-FogHeightFalloff * (AvgHeight - FogHeight));
	}

	float FogAmount = FogDensity * LineIntegral * fogDistance;
	float FogFactor = 1.0 - exp(-FogAmount);

	if (DistanceFromCamera > FogCutoffDistance)
	{
		FogFactor = 1.0;
	}

	FogFactor = saturate(FogFactor);
	FogFactor = min(FogFactor, FogMaxOpacity);

	return lerp(SceneColor, FogInscatteringColor, FogFactor);
}

PS_OUTPUT mainPS(PS_INPUT In)
{
    PS_OUTPUT Output;

    // Convert viewport TexCoord to Scene RT UV
    // In.UV는 현재 viewport 내에서 (0,0)~(1,1)
    // Scene RT UV로 변환: (ViewportTopLeft + UV * ViewportSize) / SceneRTSize
    float2 sceneUV = (ViewportTopLeft + In.UV * ViewportSize) / SceneRTSize;

    // Sample Scene Depth and output to BackBuffer DSV
    float Depth = SceneDepthTexture.Sample(LinearClamp, sceneUV).r;
    Output.Depth = Depth;

    float3 SceneColor = SceneTexture.Sample(LinearClamp, sceneUV).rgb;
    SceneColor = ApplyFog(SceneColor, Depth, In.UV);
    Output.Color = float4(SceneColor, 1.0);
    return Output;
}
