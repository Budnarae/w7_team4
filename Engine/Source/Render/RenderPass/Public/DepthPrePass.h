#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

class UPipeline;

/**
 * @brief Depth Buffer만 렌더링하는 Depth Pre-pass
 * Light Culling을 위한 정확한 Depth 정보 생성
 */
class FDepthPrePass :
	public FRenderPass
{
public:
	FDepthPrePass(
		UPipeline* InPipeline,
		ID3D11DepthStencilView* InDepthDSV,
		ID3D11Buffer* InConstantBufferViewProj,
		ID3D11Buffer* InConstantBufferModel,
		ID3D11VertexShader* InDepthVS,
		ID3D11PixelShader* InDepthPS,
		ID3D11InputLayout* InDepthLayout,
		ID3D11DepthStencilState* InDepthState
	);

	void Execute(FRenderingContext& Context) override;
	void Release() override;

	// Update DSV after resize
	void UpdateDepthStencilView(ID3D11DepthStencilView* InDepthDSV);

private:
	ID3D11DepthStencilView* DepthDSV;
	ID3D11VertexShader* DepthVS;
	ID3D11PixelShader* DepthPS;
	ID3D11InputLayout* DepthLayout;
	ID3D11DepthStencilState* DepthState;
};
