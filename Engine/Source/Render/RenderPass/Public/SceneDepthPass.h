#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

struct FSceneDepthParameters
{
	float Near = 0.1f;
	float Far = 100.0f;

	FVector2 ViewportTopLeft;
	FVector2 ViewportSize;
	FVector2 SceneRTSize;
};

class FSceneDepthPass : public FRenderPass
{
public:
	FSceneDepthPass
	(
		UPipeline* InPipeline,
		ID3D11RenderTargetView* InSceneColorRTV,
		ID3D11ShaderResourceView* InSceneDepthSRV,
		ID3D11SamplerState* InLinearSamplerState,
		ID3D11VertexShader* InVS,
		ID3D11PixelShader* InPS,
		ID3D11DepthStencilState* InDepthTestNoWriteState,
		ID3D11Buffer* InConstantBufferSceneDepthProperties
	);
	void Execute(FRenderingContext& Context) override;
	void Release() override;

private:
	ID3D11RenderTargetView* SceneColorRTV;
	ID3D11ShaderResourceView* SceneDepthSRV = nullptr;
	ID3D11SamplerState* LinearSamplerState = nullptr;

	ID3D11VertexShader* VS = nullptr;
	ID3D11PixelShader* PS = nullptr;
	ID3D11DepthStencilState* DepthTestNoWriteState = nullptr;
	ID3D11Buffer* ConstantBufferSceneDepthProperties = nullptr;
};
