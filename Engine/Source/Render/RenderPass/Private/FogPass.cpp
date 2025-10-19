#include "pch.h"
#include "Render/RenderPass/Public/FogPass.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Level/Public/Level.h"
#include "Component/Public/HeightFogComponent.h"

FFogPass::FFogPass
(
    UPipeline* InPipeline,
    ID3D11RenderTargetView* InBackBufferRTV,
    ID3D11DepthStencilView* InBackBufferDSV,
    ID3D11ShaderResourceView* InSceneColorSRV,
    ID3D11ShaderResourceView* InSceneDepthSRV,
    ID3D11SamplerState* InLinearSamplerState,
    ID3D11Buffer* InConstantBufferViewProj,
    ID3D11Buffer* InConstantBufferModel,
    ID3D11Buffer* InConstantBufferFogProperties,
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
    ConstantBufferFogProperties(InConstantBufferFogProperties),
    VS(InVS),
    PS(InPS),
	DS(InDS)
{
}

void FFogPass::Execute(FRenderingContext& Context)
{
	FHeightFogParameters FogParams = {};

	// 여기서부터 +
	Pipeline->GetContext()->OMSetRenderTargets(1, &BackBufferRTV, BackBufferDSV);
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
		nullptr, // PostProcess fullscreen quad layout
		VS, // PostProcess VS (fullscreen quad)
		FRenderResourceFactory::GetRasterizerState({ECullMode::None, EFillMode::Solid}),
		DS, // Depth test X, Depth write O
		PS, // PostProcess PS (Fog + FXAA 통합)
		nullptr, // Blend
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	Pipeline->SetConstantBuffer(0, false, ConstantBufferFogProperties);

	Pipeline->SetSamplerState(0, false, LinearSamplerState);

	// 소스 텍스처 샘플러 (Scene Color + Scene Depth)
	ID3D11ShaderResourceView* srvs[2] = {SceneColorSRV, SceneDepthSRV};
	Pipeline->GetContext()->PSSetShaderResources(0, 2, srvs);

	// Fullscreen Quad 그리기 (RenderFog과 동일한 방식)
	uint32 stride = sizeof(float) * 5; // Position(3) + TexCoord(2)
	uint32 offset = 0;
	Pipeline->GetContext()->Draw(3, 0);

	// SRV 언바인드 (경고 방지)
	ID3D11ShaderResourceView* NullSrvs[2] = {nullptr, nullptr};
	Pipeline->GetContext()->PSSetShaderResources(0, 2, NullSrvs);
}

void FFogPass::Release()
{
}
