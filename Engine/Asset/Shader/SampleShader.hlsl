cbuffer constants : register(b0)
{
	row_major float4x4 world;
}

cbuffer PerFrame : register(b1)
{
	row_major float4x4 View;        // View Matrix Calculation of MVP Matrix
	row_major float4x4 Projection;  // Projection Matrix Calculation of MVP Matrix
};

cbuffer PerFrame : register(b2)
{
	float4 totalColor;
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
    float4 position : SV_POSITION;	// Transformed position to pass to the pixel shader
    float4 color : COLOR;			// Color to pass to the pixel shader
	float3 normal : NORMAL;
};

struct PS_OUTPUT
{
	float4 color : SV_TARGET0;
	float4 normal : SV_TARGET1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
	float4 tmp = float4(input.position, 1.0f);
    tmp = mul(tmp, world);
    tmp = mul(tmp, View);
    tmp = mul(tmp, Projection);

	output.position = tmp;
    output.color = input.color;

	// Normal이 zero vector인지 체크 (데이터 없음)
	float normalLength = length(input.normal);
	if (normalLength > 0.001f)
	{
		output.normal = normalize(mul(float4(input.normal, 0.0f), world).xyz);
	}
	else
	{
		// Normal이 없으면 0 벡터 그대로 전달 (ViewMode Normal에서 확인 가능)
		output.normal = float3(0.0f, 0.0f, 0.0f);
	}

    return output;
}

PS_OUTPUT mainPS(PS_INPUT input)
{
	PS_OUTPUT output;

	output.color = lerp(input.color, totalColor, totalColor.a);
	// [0, 1] 범위로 변환하여 저장
	output.normal = float4(input.normal * 0.5f + 0.5f, 1.0f);
	return output;
}
