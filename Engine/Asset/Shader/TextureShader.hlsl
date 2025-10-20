cbuffer constants : register(b0)
{
	row_major float4x4 world;
}

cbuffer PerFrame : register(b1)
{
	row_major float4x4 View;		// View Matrix Calculation of MVP Matrix
	row_major float4x4 Projection;	// Projection Matrix Calculation of MVP Matrix
};

cbuffer MaterialConstants : register(b2)
{
	float4 Ka;		// Ambient color
	float4 Kd;		// Diffuse color
	float4 Ks;		// Specular color
	float Ns;		// Specular exponent
	float Ni;		// Index of refraction
	float D;		// Dissolve factor
	uint MaterialFlags;	// Which textures are available (bitfield)
	float Time;
};

Texture2D DiffuseTexture : register(t0);	// map_Kd
Texture2D AmbientTexture : register(t1);	// map_Ka
Texture2D SpecularTexture : register(t2);	// map_Ks
Texture2D NormalTexture : register(t3);		// map_Ns
Texture2D AlphaTexture : register(t4);		// map_d
Texture2D BumpTexture : register(t5);		// map_bump

SamplerState SamplerWrap : register(s0);

// Material flags
#define HAS_DIFFUSE_MAP	 (1 << 0)
#define HAS_AMBIENT_MAP	 (1 << 1)
#define HAS_SPECULAR_MAP (1 << 2)
#define HAS_NORMAL_MAP	 (1 << 3)
#define HAS_ALPHA_MAP	 (1 << 4)
#define HAS_BUMP_MAP	 (1 << 5)

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
	float3 normal : TEXCOORD0;
	float2 tex : TEXCOORD1;
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

	output.tex = input.tex;

	return output;
}

PS_OUTPUT mainPS(PS_INPUT input)
{
	//float4 finalColor = float4(0.f, 0.f, 0.f, 1.f);

	//// Base diffuse color
	//float4 diffuseColor = Kd;
	//if (MaterialFlags & HAS_DIFFUSE_MAP)
	//{
	//	diffuseColor *= DiffuseTexture.Sample(SamplerWrap, input.tex);
	//}

	//// Ambient contribution
	//float4 ambientColor = Ka;
	//if (MaterialFlags & HAS_AMBIENT_MAP)
	//{
	//	ambientColor *= AmbientTexture.Sample(SamplerWrap, input.tex);
	//}

	//finalColor.rgb = diffuseColor.rgb + ambientColor.rgb;

	//// Alpha handling
	//finalColor.a = D;
	//if (MaterialFlags & HAS_ALPHA_MAP)
	//{
	//	float alpha = AlphaTexture.Sample(SamplerWrap, input.tex).r;
	//	finalColor.a *= alpha;
	//}

	//return finalColor;
	PS_OUTPUT output;

	float2 ScrollSpeed = float2(0.0f, 0.1f);
	float2 UV = frac(input.tex + ScrollSpeed * Time);

	output.color = DiffuseTexture.Sample(SamplerWrap, UV);
	// 빌보드 투명하게 보이게
    clip(output.color.a - 0.1f);

	// 텍스처 크기 가져오기
	uint width, height;
	NormalTexture.GetDimensions(width, height);

	// 크기가 0이면 Normal Texture 바인딩 안 됨
	if (width > 0 && height > 0)
	{
		// [0, 1] 범위로 변환하여 저장
		output.normal = float4(\
			NormalTexture.Sample(SamplerWrap, UV) * 0.5f + 0.5f, \
			1.0f\
			);
	}
	else
	{
		output.normal = float4(input.normal * 0.5f + 0.5f, 1.0f);
	}

	return output;
}
