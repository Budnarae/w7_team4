#include "pch.h"
#include "Render/RenderPass/Public/LightComplexityPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/RenderPass/Public/LightCullingPass.h"
#include "Render/Renderer/Public/Renderer.h"

FLightComplexityPass::FLightComplexityPass(
	UPipeline* InPipeline,
	ID3D11RenderTargetView* InSceneColorRTV,
	ID3D11SamplerState* InSamplerState,
	ID3D11VertexShader* InVS,
	ID3D11PixelShader* InPS,
	ID3D11DepthStencilState* InDepthState,
	ID3D11BlendState* InBlendState,
	ID3D11Buffer* InConstantBuffer
) :
	FRenderPass(InPipeline, nullptr, nullptr, nullptr),
	SceneColorRTV(InSceneColorRTV),
	SamplerState(InSamplerState),
	VS(InVS),
	PS(InPS),
	DepthState(InDepthState),
	BlendState(InBlendState),
	ConstantBuffer(InConstantBuffer)
{
}

void FLightComplexityPass::Execute(FRenderingContext& Context)
{
	if (!VS)
	{
		return;
	}
	if (!PS)
	{
		return;
	}
	if (!ConstantBuffer)
	{
		return;
	}
	if (!SceneColorRTV)
	{
		return;
	}
	if (!BlendState)
	{
		return;
	}

	if (!Context.LightingData)
	{
		return;
	}

	if (!Context.Viewport.Width || !Context.Viewport.Height)
	{
		return;
	}

	auto* DeviceContext = Pipeline->GetContext();
	if (!DeviceContext)
	{
		return;
	}

	// RenderTarget 바인딩
	DeviceContext->OMSetRenderTargets(1, &SceneColorRTV, nullptr);
	DeviceContext->RSSetViewports(1, &Context.Viewport);

	// Constant Buffer 업데이트
	FLightComplexityConstants Constants = {};
	Constants.ScreenDimensions = Context.SceneRTSize;
	Constants.NumTilesX = (static_cast<uint32>(Context.SceneRTSize.X) + 31) / 32;
	Constants.NumTilesY = (static_cast<uint32>(Context.SceneRTSize.Y) + 31) / 32;
	Constants.NumPointLights = Context.LightingData->NumActivePointLights;
	Constants.NumSpotLights = Context.LightingData->NumActiveSpotLights;

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBuffer, Constants);

	// Pipeline 설정 (Alpha Blend로 메시 위에 오버레이)
	FPipelineInfo PipelineInfo = {
		nullptr, // Input Layout 불필요
		VS,
		nullptr, // Rasterizer 기본값
		DepthState, // Depth Test는 Always, Write는 Off
		PS,
		BlendState, // Alpha Blend State
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	// Constant Buffer 바인딩
	Pipeline->SetConstantBuffer(0, false, ConstantBuffer);

	// Sampler 바인딩
	if (SamplerState)
	{
		Pipeline->SetSamplerState(0, false, SamplerState);
	}

	// Tile Light Mask SRV 바인딩
	if (auto* LightCullingPass = URenderer::GetInstance().GetLightCullingPass())
	{
		if (auto* TileLightMaskSRV = LightCullingPass->GetTileLightMaskSRV())
		{
			Pipeline->SetTexture(0, false, TileLightMaskSRV);
		}
	}

	// Input Assembler 언바인딩
	DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);

	// Fullscreen triangle 렌더링
	Pipeline->Draw(3, 0);

	// SRV 언바인딩
	ID3D11ShaderResourceView* NullSRV = nullptr;
	DeviceContext->PSSetShaderResources(0, 1, &NullSRV);
}

void FLightComplexityPass::Release()
{
	// 리소스는 Renderer가 소유
}
