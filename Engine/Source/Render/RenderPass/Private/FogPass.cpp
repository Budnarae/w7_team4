#include "pch.h"
#include "Render/RenderPass/Public/FogPass.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Level/Public/Level.h"
#include "Component/Public/HeightFogComponent.h"

FFogPass::FFogPass
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
)
    :
    FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel),
	SceneColorRTV(InSceneColorRTV),
	SceneDepthSRV(InSceneDepthSRV),
	LinearSamplerState(InLinearSamplerState),
    ConstantBufferFogProperties(InConstantBufferFogProperties),
    VS(InVS),
    PS(InPS),
	DepthTestNoWriteState(InDepthTestNoWriteState),
	AlphaBlendState(InAlphaBlendState)
{
}

void FFogPass::Execute(FRenderingContext& Context)
{
	FHeightFogParameters FogParams = {};

	// Forward 방식: SceneColorRTV에 렌더링, DSV는 nullptr (Depth Read 가능)
	Pipeline->GetContext()->OMSetRenderTargets(1, &SceneColorRTV, nullptr);
	// Viewport 설정 (각 ViewportClient 영역에만 적용)
	Pipeline->GetContext()->RSSetViewports(1, &Context.Viewport);

	// Viewport 정보 설정
	FogParams.ViewportTopLeft = FVector2(Context.Viewport.TopLeftX, Context.Viewport.TopLeftY);
	FogParams.ViewportSize = FVector2(Context.Viewport.Width, Context.Viewport.Height);
	FogParams.SceneRTSize = Context.SceneRTSize;

	const ULevel* CurrentLevel = GWorld->GetLevel();
	// Fog 파라미터 설정
	const bool bShowFog = CurrentLevel && (CurrentLevel->GetShowFlags() & EEngineShowFlags::SF_Fog)
		!= 0;

	UHeightFogComponent* FogComponent = Context.Fogs.empty() ? nullptr : Context.Fogs.front();

	// Fog 파라미터 채우기
	if (FogComponent && bShowFog)
	{
		FogParams.FogDensity = FogComponent->GetFogDensity();
		FogParams.FogHeightFalloff = FogComponent->GetFogHeightFalloff();
		FogParams.StartDistance = FogComponent->GetStartDistance();
		FogParams.FogCutoffDistance = FogComponent->GetFogCutoffDistance();
		FogParams.FogMaxOpacity = FogComponent->GetFogMaxOpacity();

		FVector4 ColorRGBA = FogComponent->GetFogInscatteringColor();
		FogParams.FogInscatteringColor = FVector(ColorRGBA.X, ColorRGBA.Y, ColorRGBA.Z);

		FogParams.CameraPosition = Context.CurrentCamera->GetLocation();
		FogParams.FogHeight = FogComponent->GetWorldLocation().Z;
	}
	else
	{
		// Fog 비활성화
		FogParams.FogDensity = 0.0f;
		FogParams.FogMaxOpacity = 0.0f;
	}

	// Inverse View-Projection Matrix
	const FViewProjConstants& ViewProj = Context.CurrentCamera->GetFViewProjConstants();
	FMatrix ViewProjMatrix = ViewProj.View * ViewProj.Projection;
	FogParams.InvViewProj = ViewProjMatrix.Inverse();

	FRenderResourceFactory::UpdateConstantBufferData
	(
		ConstantBufferFogProperties,
		FogParams
	);

	// 파이프라인 셋업
	FPipelineInfo PipelineInfo = {
		nullptr, // PostProcess fullscreen quad layout (SV_VertexID 사용)
		VS, // PostProcess VS (fullscreen quad)
		FRenderResourceFactory::GetRasterizerState({ECullMode::None, EFillMode::Solid}),
		DepthTestNoWriteState, // Depth Test ALWAYS, Depth Write OFF
		PS, // PostProcess PS (Fog)
		AlphaBlendState, // Alpha Blend: Source.rgb * Source.a + Dest.rgb * (1 - Source.a)
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	Pipeline->SetConstantBuffer(0, false, ConstantBufferFogProperties);

	Pipeline->SetSamplerState(0, false, LinearSamplerState);

	// Depth 텍스처만 바인딩 (SceneColorRTV에 쓰고 있으므로 SRV로 읽을 수 없음)
	Pipeline->GetContext()->PSSetShaderResources(0, 1, &SceneDepthSRV);

	// Fullscreen Quad 그리기 (SV_VertexID 사용)
	Pipeline->GetContext()->Draw(3, 0);

	// SRV 언바인드 (경고 방지)
	ID3D11ShaderResourceView* NullSrvs[2] = {nullptr, nullptr};
	Pipeline->GetContext()->PSSetShaderResources(0, 2, NullSrvs);
}

void FFogPass::Release()
{
}
