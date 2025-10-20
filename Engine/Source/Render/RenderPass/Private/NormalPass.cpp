#include "pch.h"
#include "Render/RenderPass/Public/NormalPass.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

FNormalPass::FNormalPass
(
    UPipeline* InPipeline,
    ID3D11RenderTargetView* InSceneColorRTV,
    ID3D11ShaderResourceView* InNormalSRV,
    ID3D11SamplerState* InLinearSamplerState,
    ID3D11VertexShader* InVS,
    ID3D11PixelShader* InPS,
    ID3D11DepthStencilState* InDepthTestNoWriteState//,
    //ID3D11Buffer* InConstantBufferNormalProperties
)
    :
    FRenderPass(InPipeline, nullptr, nullptr),
	SceneColorRTV(InSceneColorRTV),
	NormalSRV(InNormalSRV),
	LinearSamplerState(InLinearSamplerState),
    VS(InVS),
    PS(InPS),
	DepthTestNoWriteState(InDepthTestNoWriteState)//,
	//ConstantBufferNormalProperties(InConstantBufferNormalProperties)
{
}

void FNormalPass::Execute(FRenderingContext& Context)
{
	// Forward 방식: SceneColorRTV에 렌더링, DSV는 nullptr (Depth Read 가능)
	Pipeline->GetContext()->OMSetRenderTargets(1, &SceneColorRTV, nullptr);
	// Viewport 설정 (각 ViewportClient 영역에만 적용)
	Pipeline->GetContext()->RSSetViewports(1, &Context.Viewport);

    static FPipelineInfo PipelineInfo = {
	    nullptr,
    	VS,
    	FRenderResourceFactory::GetRasterizerState({ECullMode::None, EFillMode::Solid}),
    	DepthTestNoWriteState,
    	PS,
    	nullptr
    };

    Pipeline->UpdatePipeline(PipelineInfo);

 //    FNormalParameters NormalParameters;
 //    NormalParameters.Near = Context.CurrentCamera->GetNearZ();
 //    NormalParameters.Far = Context.CurrentCamera->GetFarZ();
	// // Viewport 정보 설정
	// NormalParameters.ViewportTopLeft = FVector2(Context.Viewport.TopLeftX, Context.Viewport.TopLeftY);
	// NormalParameters.ViewportSize = FVector2(Context.Viewport.Width, Context.Viewport.Height);
	// NormalParameters.SceneRTSize = Context.SceneRTSize;
 //
 //    FRenderResourceFactory::UpdateConstantBufferData
 //    (
	//     ConstantBufferNormalProperties,
	//     NormalParameters
 //    );
 //
 //    Pipeline->SetConstantBuffer(0, false, ConstantBufferNormalProperties);
    Pipeline->SetSamplerState(0, false, LinearSamplerState);

    // Depth 텍스처만 바인딩 (SceneColorRTV에 쓰고 있으므로 SRV로 읽을 수 없음)
	Pipeline->GetContext()->PSSetShaderResources(0, 1, &NormalSRV);

	// Fullscreen Quad 그리기 (SV_VertexID 사용)
	Pipeline->GetContext()->Draw(3, 0);

	// SRV 언바인드 (경고 방지)
	ID3D11ShaderResourceView* NullSrvs[2] = {nullptr, nullptr};
	Pipeline->GetContext()->PSSetShaderResources(0, 2, NullSrvs);
}

void FNormalPass::Release()
{
}
