#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

struct FFXAAParameters
{
	float SubpixelBlend;
	float EdgeThreshold;
	float EdgeThresholdMin;
	float EnableFXAA;

	FVector2 ViewportTopLeft;
	FVector2 ViewportSize;
	FVector2 SceneRTSize;
	FVector2 Padding2;
};

class FFXAAPass : public FRenderPass
{
public:
	FFXAAPass
	(
		UPipeline* InPipeline,
		ID3D11RenderTargetView* InBackBufferRTV,
		ID3D11DepthStencilView* InBackBufferDSV,
		ID3D11ShaderResourceView* InSceneColorSRV,
		ID3D11ShaderResourceView* InSceneDepthSRV,
		ID3D11SamplerState* InLinearSamplerState,
		ID3D11Buffer* InConstantBufferViewProj,
		ID3D11Buffer* InConstantBufferModel,
		ID3D11Buffer* InConstantBufferFXAAParameters,
		ID3D11VertexShader* InVS,
		ID3D11PixelShader* InPS,
		ID3D11DepthStencilState* InDS
	);
	void Execute(FRenderingContext& Context) override;
	void Release() override;

	// Update render targets after resize
	void UpdateRenderTargets(ID3D11RenderTargetView* InBackBufferRTV, ID3D11DepthStencilView* InBackBufferDSV, ID3D11ShaderResourceView* InSceneColorSRV, ID3D11ShaderResourceView* InSceneDepthSRV);

private:
	ID3D11RenderTargetView* BackBufferRTV = nullptr;
	ID3D11DepthStencilView* BackBufferDSV = nullptr;

	ID3D11ShaderResourceView* SceneColorSRV = nullptr;
	ID3D11ShaderResourceView* SceneDepthSRV = nullptr;

	ID3D11SamplerState* LinearSamplerState = nullptr;

	ID3D11VertexShader* VS = nullptr;
	ID3D11PixelShader* PS = nullptr;
	ID3D11DepthStencilState* DS = nullptr;

	ID3D11Buffer* ConstantBufferFXAAParameters = nullptr;
};
