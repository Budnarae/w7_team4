#include "pch.h"
#include "Render/RenderPass/Public/LightComplexityPass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

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
	Constants.ScreenDimensions = FVector2(Context.Viewport.Width, Context.Viewport.Height);
	Constants.NumTilesX = (static_cast<uint32>(Context.Viewport.Width) + 31) / 32;
	Constants.NumTilesY = (static_cast<uint32>(Context.Viewport.Height) + 31) / 32;
	Constants.NumPointLights = Context.LightingData->NumActivePointLights;
	Constants.NumSpotLights = Context.LightingData->NumActiveSpotLights;
	Constants.PointLightUsageMask = Context.LightingData->PointLightUsageMask;
	Constants.SpotLightUsageMask = Context.LightingData->SpotLightUsageMask;

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBuffer, Constants);

	// Pipeline 설정 (Alpha Blend로 메시 위에 오버레이)
	FPipelineInfo PipelineInfo = {
		nullptr, // Input Layout 불필요
		VS,
		nullptr, // Rasterizer 기본값
		nullptr, // Depth State nullptr
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

	// Input Assembler 언바인딩
	DeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
	DeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);

	// Fullscreen triangle 렌더링
	Pipeline->Draw(3, 0);
}

void FLightComplexityPass::Release()
{
	// 리소스는 Renderer가 소유
}
