#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

class UCamera;

// Unified Light Properties (Total: 160 bytes)
struct FLightProperties
{
	FMatrix InvViewProj;         // 64 bytes (4x4 float matrix)

	FVector LightPosition;       // 12 bytes
	float Intensity;             // 4 bytes

	FVector LightColor;          // 12 bytes
	float Radius;                // 4 bytes

	float RadiusFalloff;         // 4 bytes
	float _Padding0;             // 4 bytes
	FVector2 ViewportTopLeft;    // 8 bytes

	FVector2 ViewportSize;       // 8 bytes
	FVector2 SceneRTSize;        // 8 bytes

	// SpotLight 전용 필드 (PointLight에서는 미사용)
	FVector LightDirection;      // 12 bytes
	float InnerConeAngle;        // 4 bytes

	float OuterConeAngle;        // 4 bytes
	FVector _Padding1;           // 12 bytes (padding for 16-byte alignment)
};

/**
 * @brief 라이트 렌더링 Pass
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
	           ID3D11PixelShader* InPointLightPS,
	           ID3D11PixelShader* InSpotLightPS,
	           ID3D11SamplerState* InSamplerState,
	           ID3D11DepthStencilState* InDepthLessEqualNoWrite,
	           ID3D11BlendState* InAdditiveBlend);

	void Execute(FRenderingContext& Context) override;
	void Release() override;

private:
	// Helper functions
	void RenderPointLights(const TArray<class UPointLightComponent*>& PointLights,
	                       const FMatrix& InvViewProj,
	                       const D3D11_VIEWPORT& Viewport,
	                       const FVector2& SceneRTSize) const;

	void RenderSpotLights(const TArray<class USpotLightComponent*>& SpotLights,
	                      const FMatrix& InvViewProj,
	                      const D3D11_VIEWPORT& Viewport,
	                      const FVector2& SceneRTSize) const;

	// Scene RT/DSV
	ID3D11ShaderResourceView* SceneDepthSRV = nullptr;
	ID3D11RenderTargetView* SceneColorRTV = nullptr;

	// Shader Permutations
	ID3D11VertexShader* LightVolumeVertexShader = nullptr;
	ID3D11InputLayout* LightVolumeInputLayout = nullptr;
	ID3D11PixelShader* PointLightPixelShader = nullptr;
	ID3D11PixelShader* SpotLightPixelShader = nullptr;

	// Render States
	ID3D11SamplerState* LightSamplerState = nullptr;
	ID3D11DepthStencilState* DepthLessEqualNoWrite = nullptr;
	ID3D11BlendState* AdditiveBlendState = nullptr;

	// Constant Buffer
	ID3D11Buffer* ConstantBufferLightProperties = nullptr;
};
