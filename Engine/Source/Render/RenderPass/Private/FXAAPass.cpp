#include "pch.h"
#include "Render/RenderPass/Public/FXAAPass.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Manager/PostProcess/Public/FXAAManager.h"

FFXAAPass::FFXAAPass
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
)
	:
	FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel),
	BackBufferRTV(InBackBufferRTV),
	BackBufferDSV(InBackBufferDSV),
	SceneColorSRV(InSceneColorSRV),
	SceneDepthSRV(InSceneDepthSRV),
	LinearSamplerState(InLinearSamplerState),
	ConstantBufferFXAAParameters(InConstantBufferFXAAParameters),
	VS(InVS),
	PS(InPS),
	DS(InDS)
{
}

void FFXAAPass::Execute(FRenderingContext& Context)
{
	FFXAAParameters FXAAParams = {};

	Pipeline->GetContext()->OMSetRenderTargets(1, &BackBufferRTV, BackBufferDSV);
	Pipeline->GetContext()->RSSetViewports(1, &Context.Viewport);

	// Viewport 정보 설정
	FXAAParams.ViewportTopLeft = FVector2(Context.Viewport.TopLeftX, Context.Viewport.TopLeftY);
	FXAAParams.ViewportSize = FVector2(Context.Viewport.Width, Context.Viewport.Height);
	FXAAParams.SceneRTSize = Context.SceneRTSize;

	// FXAAManager에서 FXAA 파라미터 가져오기
	UFXAAManager& FXAAMgr = UFXAAManager::GetInstance();
	FXAAParams.SubpixelBlend = FXAAMgr.GetSubpixelBlend();
	FXAAParams.EdgeThreshold = FXAAMgr.GetEdgeThreshold();
	FXAAParams.EdgeThresholdMin = FXAAMgr.GetEdgeThresholdMin();
	FXAAParams.EnableFXAA = FXAAMgr.IsEnabled() ? 1.0f : 0.0f;

	FRenderResourceFactory::UpdateConstantBufferData
	(
		ConstantBufferFXAAParameters,
		FXAAParams
	);

	// 파이프라인 셋업
	FPipelineInfo PipelineInfo = {
		nullptr, // PostProcess fullscreen quad layout
		VS, // PostProcess VS (fullscreen quad)
		FRenderResourceFactory::GetRasterizerState({ECullMode::None, EFillMode::Solid}),
		DS, // Depth test X, Depth write O
		PS, // PostProcess PS (FXAA)
		nullptr, // Blend
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	Pipeline->SetConstantBuffer(0, false, ConstantBufferFXAAParameters);

	Pipeline->SetSamplerState(0, false, LinearSamplerState);

	// 소스 텍스처 샘플러 (Scene Color + Scene Depth)
	ID3D11ShaderResourceView* srvs[2] = {SceneColorSRV, SceneDepthSRV};
	Pipeline->GetContext()->PSSetShaderResources(0, 2, srvs);

	// Fullscreen Quad 그리기 (SV_VertexID 사용)
	Pipeline->GetContext()->Draw(3, 0);

	// SRV 언바인드 (경고 방지)
	ID3D11ShaderResourceView* NullSrvs[2] = {nullptr, nullptr};
	Pipeline->GetContext()->PSSetShaderResources(0, 2, NullSrvs);
}

void FFXAAPass::Release()
{
}

void FFXAAPass::UpdateRenderTargets(ID3D11RenderTargetView* InBackBufferRTV, ID3D11DepthStencilView* InBackBufferDSV, ID3D11ShaderResourceView* InSceneColorSRV, ID3D11ShaderResourceView* InSceneDepthSRV)
{
	BackBufferRTV = InBackBufferRTV;
	BackBufferDSV = InBackBufferDSV;
	SceneColorSRV = InSceneColorSRV;
	SceneDepthSRV = InSceneDepthSRV;
}
