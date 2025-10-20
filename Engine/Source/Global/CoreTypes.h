#pragma once
#include "Global/Vector.h"
#include "Global/Matrix.h"
#include "Global/Types.h"
#include "Core/Public/Name.h"
#include <Texture/Public/Material.h>

//struct BatchLineContants
//{
//	float CellSize;
//	//FMatrix BoundingBoxModel;
//	uint32 ZGridStartIndex; // 인덱스 버퍼에서, z방향쪽 그리드가 시작되는 인덱스
//	uint32 BoundingBoxStartIndex; // 인덱스 버퍼에서, 바운딩박스가 시작되는 인덱스
//};

struct FViewProjConstants
{
	FViewProjConstants()
	{
		View = FMatrix::Identity();
		Projection = FMatrix::Identity();
	}

	FMatrix View;
	FMatrix Projection;
};

struct FMaterialConstants
{
	FVector4 Ka;
	FVector4 Kd;
	FVector4 Ks;
	float Ns;
	float Ni;
	float D;
	uint32 MaterialFlags;
	float Time; // Time in seconds
};

// UberLit Shader Light Structures
struct FAmbientLightInfo
{
	FVector Color;          // 12 bytes
	float Intensity;        // 4 bytes
	// Total: 16 bytes (HLSL: float3 Color + float Intensity = 16 bytes aligned)
};

struct FDirectionalLightInfo
{
	FVector Direction;      // 12 bytes
	float Intensity;        // 4 bytes
	FVector Color;          // 12 bytes
	float _Padding;         // 4 bytes
};

struct FPointLightInfo
{
	FVector Position;       // 12 bytes
	float Intensity;        // 4 bytes
	FVector Color;          // 12 bytes
	float Radius;           // 4 bytes
	float Falloff;          // 4 bytes
	FVector _Padding;       // 12 bytes (padding for alignment)
};

struct FSpotLightInfo
{
	FVector Position;       // 12 bytes
	float Intensity;        // 4 bytes
	FVector Direction;      // 12 bytes
	float Radius;           // 4 bytes
	FVector Color;          // 12 bytes
	float InnerConeAngle;   // 4 bytes
	float OuterConeAngle;   // 4 bytes
	float Falloff;          // 4 bytes
	FVector2 _Padding;      // 8 bytes
};

// UberLit Shader Constant Buffers
struct FPerObjectConstants
{
	FMatrix World;
	FMatrix View;
	FMatrix Projection;
};

#define MAX_POINT_LIGHTS 4
#define MAX_SPOT_LIGHTS 4

struct FLightingConstants
{
	FAmbientLightInfo Ambient;
	FDirectionalLightInfo Directional;
	FPointLightInfo PointLights[MAX_POINT_LIGHTS];
	FSpotLightInfo SpotLights[MAX_SPOT_LIGHTS];

	uint32 NumActivePointLights;
	uint32 NumActiveSpotLights;
	FVector2 _LightingPadding;
};

struct FModelForLight
{
	FMatrix World;
	FMatrix WorldTransInv;
};

struct FVertex
{
	FVector Position;
	FVector4 Color;
};

struct FNormalVertex
{
	FVector Position;
	FVector Normal;
	FVector4 Color;
	FVector2 TexCoord;
};

struct FRay
{
	FVector4 Origin;
	FVector4 Direction;
};

/**
 * @brief Render State Settings for Actor's Component
 */
struct FRenderState
{
	ECullMode CullMode = ECullMode::None;
	EFillMode FillMode = EFillMode::Solid;
};

/**
 * @brief 변환 정보를 담는 구조체
 */
struct FTransform
{
	FVector Location = FVector(0.0f, 0.0f, 0.0f);
	FVector Rotation = FVector(0.0f, 0.0f, 0.0f);
	FVector Scale = FVector(1.0f, 1.0f, 1.0f);

	FTransform() = default;

	FTransform(const FVector& InLocation, const FVector& InRotation = FVector::ZeroVector(),
		const FVector& InScale = FVector::OneVector())
		: Location(InLocation), Rotation(InRotation), Scale(InScale)
	{
	}
};

/**
 * @brief 2차원 좌표의 정보를 담는 구조체
 */
struct FPoint
{
	float X = 0.0f;
	float Y = 0.0f;
};

/**
 * @brief 윈도우를 비롯한 2D 화면의 정보를 담는 구조체
 */
struct FRect
{
	float Left = 0.0f;
	float Top = 0.0f;
	float Width = 0.0f;
	float Height = 0.0f;

	float GetRight() const { return Left + Width; }
	float GetBottom() const { return Top + Height; }
};
