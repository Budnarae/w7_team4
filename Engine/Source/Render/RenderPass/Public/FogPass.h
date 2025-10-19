#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

struct FHeightFogParameters
{
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
		ID3D11RenderTargetView* InBackBufferRTV,
		ID3D11DepthStencilView* InBackBufferDSV,
		ID3D11ShaderResourceView* InSceneColorSRV,
		ID3D11ShaderResourceView* InSceneDepthSRV,
		D3D11_VIEWPORT InViewport,
		ID3D11Buffer* InConstantBufferViewProj,
		ID3D11Buffer* InConstantBufferModel,
		ID3D11Buffer* InConstantBufferFogProperties,
		ID3D11VertexShader* InVS,
		ID3D11PixelShader* InPS,
		ID3D11InputLayout* InLayout,
		ID3D11DepthStencilState* InDS
	);
	void Execute(FRenderingContext& Context) override;
	void Release() override;

private:
	ID3D11RenderTargetView* BackBufferRTV = nullptr;
	ID3D11DepthStencilView* BackBufferDSV = nullptr;

	ID3D11ShaderResourceView* SceneColorSRV = nullptr;
	ID3D11ShaderResourceView* SceneDepthSRV = nullptr;

	D3D11_VIEWPORT Viewport;

	ID3D11VertexShader* VS = nullptr;
	ID3D11PixelShader* PS = nullptr;
	ID3D11InputLayout* InputLayout = nullptr;
	ID3D11DepthStencilState* DS = nullptr;

	ID3D11Buffer* ConstantBufferFogProperties = nullptr;
};
