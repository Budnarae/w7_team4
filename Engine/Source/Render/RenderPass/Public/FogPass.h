#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

struct FHeightFogParameters
{
	FVector2 ViewportTopLeft;
	FVector2 ViewportSize;
	FVector2 SceneRTSize;
	FVector2 Padding2;

	float FogDensity;
	float FogHeightFalloff;
	float StartDistance;
	float FogCutoffDistance;

	float FogMaxOpacity;
	FVector FogInscatteringColor;

	FVector CameraPosition;
	float FogHeight;

	FMatrix InvViewProj;
};

class FFogPass : public FRenderPass
{
public:
	FFogPass
	(
		UPipeline* InPipeline,
		ID3D11RenderTargetView* InSceneColorRTV,
		ID3D11ShaderResourceView* InSceneDepthSRV,
		ID3D11SamplerState* InLinearSamplerState,
		ID3D11Buffer* InConstantBufferViewProj,
		ID3D11Buffer* InConstantBufferModel,
		ID3D11Buffer* InConstantBufferFogProperties,
		ID3D11VertexShader* InVS,
		ID3D11PixelShader* InPS,
		ID3D11DepthStencilState* InDepthTestNoWriteState,
		ID3D11BlendState* InAlphaBlendState
	);
	void Execute(FRenderingContext& Context) override;
	void Release() override;

	// Update render targets after resize
	void UpdateRenderTargets(ID3D11RenderTargetView* InSceneColorRTV, ID3D11ShaderResourceView* InSceneDepthSRV);

private:
	ID3D11RenderTargetView* SceneColorRTV = nullptr;
	ID3D11ShaderResourceView* SceneDepthSRV = nullptr;

	ID3D11SamplerState* LinearSamplerState = nullptr;

	ID3D11VertexShader* VS = nullptr;
	ID3D11PixelShader* PS = nullptr;
	ID3D11DepthStencilState* DepthTestNoWriteState = nullptr;
	ID3D11BlendState* AlphaBlendState = nullptr;

	ID3D11Buffer* ConstantBufferFogProperties = nullptr;
};
