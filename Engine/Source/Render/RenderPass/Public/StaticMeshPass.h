#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

class FStaticMeshPass : public FRenderPass
{
public:
    FStaticMeshPass(
    	UPipeline* InPipeline,
    	ID3D11RenderTargetView* InSceneColorRTV,
    	ID3D11RenderTargetView* InSceneNormalRTV,
    	ID3D11DepthStencilView* InSceneDepthDSV,
    	ID3D11Buffer* InConstantBufferViewProj,
    	ID3D11Buffer* InConstantBufferModel,
    	ID3D11Buffer* InConstantBufferLighting,
        ID3D11VertexShader* InVS,
        ID3D11PixelShader* InPS,
        ID3D11InputLayout* InLayout,
        ID3D11DepthStencilState* InDS
        );
    void Execute(FRenderingContext& Context) override;
    void Release() override;

    // Update render targets after resize
    void UpdateRenderTargets(ID3D11RenderTargetView* InSceneColorRTV, ID3D11RenderTargetView* InSceneNormalRTV, ID3D11DepthStencilView* InSceneDepthDSV);

private:
	ID3D11RenderTargetView* SceneColorRTV = nullptr;
	ID3D11RenderTargetView* SceneNormalRTV = nullptr;
	ID3D11DepthStencilView* SceneDepthDSV = nullptr;

    ID3D11VertexShader* VS = nullptr;
    ID3D11PixelShader* PS = nullptr;
    ID3D11InputLayout* InputLayout = nullptr;
    ID3D11DepthStencilState* DS = nullptr;

    ID3D11Buffer* ConstantBufferMaterial = nullptr;
    ID3D11Buffer* ConstantBufferSceneInfo = nullptr;
};
