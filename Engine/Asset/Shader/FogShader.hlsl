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
};

// Forward Alpha Blend 방식: Depth만 읽음 (SceneColor는 RTV로 쓰고 있으므로 읽을 수 없음)
Texture2D SceneDepthTexture : register(t0);
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

// Calculate Fog Color (Alpha Blend 방식)
float4 CalculateFogColor(float Depth, float2 viewportUV)
{
	// If FogDensity is 0, fog is disabled
	if (FogDensity <= 0.0)
	{
		// Alpha = 0 means no fog blending (100% scene color visible)
		return float4(FogInscatteringColor, 0.0);
	}

	// Reconstruct World Position from Depth
	// IMPORTANT: Use viewportUV (0~1 within viewport) instead of sceneUV
	float2 screenPos = viewportUV * 2.0 - 1.0;
	screenPos.y = -screenPos.y;
	float4 clipPos = float4(screenPos, Depth, 1.0);
	float4 worldPos = mul(clipPos, InvViewProj);
	worldPos /= worldPos.w;

	float3 worldPosition = worldPos.xyz;
	float3 rayDir = worldPosition - CameraPosition;
	float d = length(rayDir);

	// Apply Start Distance
	float adjustedDistance = max(0.0, d - StartDistance);

	// Early exit for cutoff distance
	if (FogCutoffDistance > 0.0 && adjustedDistance > FogCutoffDistance)
	{
		// Alpha = 0 means no fog blending (100% scene color visible)
		return float4(FogInscatteringColor, 0.0);
	}

	// Exponential Height Fog - Line Integral (Iquilezles formula)
	float vh = rayDir.z / max(d, 1e-5); // z-axis component (normalized ray direction)
	float h0 = CameraPosition.z - FogHeight; // Camera height relative to fog origin
	float a = vh * FogHeightFalloff;
	float tau;

	if (abs(a) < 1e-5)
	{
		// Horizontal ray (no height change) - simple exponential fog
		tau = FogDensity * exp(-h0 * FogHeightFalloff) * adjustedDistance;
	}
	else
	{
		// Ray with height change - analytical integration
		// exponent represents the exponential change over the adjusted distance
		float exponent = -adjustedDistance * a;

		// Prevent exp() overflow (exp(87) ≈ 6e37, near float max)
		if (exponent > 87.0)
		{
			// Very dense fog at this point
			return float4(FogInscatteringColor, FogMaxOpacity);
		}

		exponent = max(exponent, -87.0); // Clamp negative side too

		// Integrated optical depth along the ray
		tau = (FogDensity / FogHeightFalloff) * exp(-h0 * FogHeightFalloff) *
		      (1.0 - exp(exponent)) / vh;
	}

	// Convert optical depth to fog factor and clamp to max opacity
	float FogFactor = 1.0 - exp(-tau);
	FogFactor = min(FogFactor, FogMaxOpacity);

	// Alpha Blend: Fog 색상 그대로 반환, Alpha는 FogFactor
	// BlendState = Source.rgb * Source.a + Dest.rgb * (1 - Source.a)
	// Result = FogColor * FogFactor + SceneColor * (1 - FogFactor)
	return float4(FogInscatteringColor, FogFactor);
}

PS_OUTPUT mainPS(PS_INPUT In)
{
    PS_OUTPUT Output;

    // Convert viewport TexCoord to Scene RT UV
    // In.UV는 현재 viewport 내에서 (0,0)~(1,1)
    // Scene RT UV로 변환: (ViewportTopLeft + UV * ViewportSize) / SceneRTSize
    float2 sceneUV = (ViewportTopLeft + In.UV * ViewportSize) / SceneRTSize;

    // Sample Scene Depth
    float Depth = SceneDepthTexture.Sample(LinearClamp, sceneUV).r;

    // Calculate Fog Color (Additive 방식)
    // BlendState가 Additive이므로 기존 SceneColor에 FogColor가 더해짐
    Output.Color = CalculateFogColor(Depth, In.UV);
    return Output;
}
