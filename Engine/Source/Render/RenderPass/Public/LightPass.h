#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

class UCamera;

// Light Properties
struct FLightProperties
{
	// Common fields
	FVector LightPosition;       // 월드 공간 라이트 위치 (offset 0)
	float Intensity;             // 조명 강도 (offset 12)

	FVector LightColor;          // RGB 색상 (offset 16)
	float Radius;                // 영향 반경 (offset 28)

	FVector LightDirection;      // SpotLight 방향 (normalized) (offset 32)
	float RadiusFalloff;         // 감쇠 지수 (offset 44)

	FVector2 ViewportTopLeft;    // Viewport 시작 위치 (offset 48)
	FVector2 ViewportSize;       // Viewport 크기 (offset 56)

	FVector2 SceneRTSize;        // Scene RT 전체 크기 (offset 64)
	float InnerConeAngle;        // SpotLight 내부 각도 (라디안) (offset 72)
	float OuterConeAngle;        // SpotLight 외부 각도 (라디안) (offset 76)

	uint32 LightType;            // 0 = PointLight, 1 = SpotLight (offset 80)
	FVector Padding;             // PADDING (offset 84)

	FMatrix InvViewProj;         // World Position 재구성용 (offset 96)
};

/**
 * @brief UberShader 기반 라이트 렌더링 Pass
 * Scene RT를 직접 받아서 Volume Lighting 수행
 */
class FLightPass :
public FRenderPass
{
public:
	FLightPass(UPipeline* InPipeline,
	           ID3D11Buffer* InConstantBufferViewProj,
	           ID3D11Buffer* InConstantBufferModel,
	           ID3D11ShaderResourceView* InSceneDepthSRV,
	           ID3D11RenderTargetView* InSceneColorRTV,
	           ID3D11VertexShader* InLightVolumeVS,
	           ID3D11InputLayout* InLightVolumeLayout,
	           ID3D11PixelShader* InUberLightPS,
	           ID3D11SamplerState* InSamplerState,
	           ID3D11DepthStencilState* InDepthLessEqualNoWrite,
	           ID3D11BlendState* InAdditiveBlend);

	void Execute(FRenderingContext& Context) override;
	void Release() override;

private:
	// Scene RT/DSV
	ID3D11ShaderResourceView* SceneDepthSRV = nullptr;
	ID3D11RenderTargetView* SceneColorRTV = nullptr;

	// Shader
	ID3D11VertexShader* LightVolumeVertexShader = nullptr;
	ID3D11InputLayout* LightVolumeInputLayout = nullptr;
	ID3D11PixelShader* LightPixelShader = nullptr;

	// Render States
	ID3D11SamplerState* LightSamplerState = nullptr;
	ID3D11DepthStencilState* DepthLessEqualNoWrite = nullptr;
	ID3D11BlendState* AdditiveBlendState = nullptr;

	// Constant Buffer
	ID3D11Buffer* ConstantBufferLightProperties = nullptr;
};
