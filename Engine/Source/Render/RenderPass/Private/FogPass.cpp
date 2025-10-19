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
    D3D11_VIEWPORT InViewport,
    ID3D11Buffer* InConstantBufferViewProj,
    ID3D11Buffer* InConstantBufferModel,
    ID3D11Buffer* InConstantBufferFogProperties,
    ID3D11VertexShader* InVS,
    ID3D11PixelShader* InPS,
    ID3D11InputLayout* InLayout,
    ID3D11DepthStencilState* InDS
)
    :
    FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel),
	BackBufferRTV(InBackBufferRTV),
	BackBufferDSV(InBackBufferDSV),
	SceneColorSRV(InSceneColorSRV),
	SceneDepthSRV(InSceneDepthSRV),
	Viewport(InViewport),
    ConstantBufferFogProperties(InConstantBufferFogProperties),
    VS(InVS),
    PS(InPS),
    InputLayout(InLayout),
	DS(InDS)
{
}

void FFogPass::Execute(FRenderingContext& Context)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();

	// Fog 파라미터 설정
	const bool bShowFog = CurrentLevel && (CurrentLevel->GetShowFlags() & EEngineShowFlags::SF_Fog)
		!= 0;

	UHeightFogComponent* FogComponent = Context.Fogs.empty() ? nullptr : Context.Fogs.front();

	FHeightFogParameters postProcessParams;

	// Fog 파라미터 채우기
	if (FogComponent && bShowFog)
	{
		postProcessParams.FogDensity = FogComponent->GetFogDensity();
		postProcessParams.FogHeightFalloff = FogComponent->GetFogHeightFalloff();
		postProcessParams.StartDistance = FogComponent->GetStartDistance();
		postProcessParams.FogCutoffDistance = FogComponent->GetFogCutoffDistance();
		postProcessParams.FogMaxOpacity = FogComponent->GetFogMaxOpacity();

		FVector4 ColorRGBA = FogComponent->GetFogInscatteringColor();
		postProcessParams.FogInscatteringColor = FVector(ColorRGBA.X, ColorRGBA.Y, ColorRGBA.Z);

		postProcessParams.CameraPosition = Context.CurrentCamera->GetLocation();
		postProcessParams.FogHeight = FogComponent->GetWorldLocation().Z;
	}
	else
	{
		// Fog 비활성화
		postProcessParams.FogDensity = 0.0f;
		postProcessParams.FogMaxOpacity = 0.0f;
	}

	// Inverse View-Projection Matrix
	const FViewProjConstants& ViewProj = Context.CurrentCamera->GetFViewProjConstants();
	FMatrix ViewProjMatrix = ViewProj.View * ViewProj.Projection;
	postProcessParams.InvViewProj = ViewProjMatrix.Inverse();

	FRenderResourceFactory::UpdateConstantBufferData
	(
		ConstantBufferFogProperties,
		postProcessParams
	);

	// 파이프라인 셋업
	FPipelineInfo PipelineInfo = {
		InputLayout, // PostProcess fullscreen quad layout
		VS, // PostProcess VS (fullscreen quad)
		FRenderResourceFactory::GetRasterizerState({ECullMode::None, EFillMode::Solid}),
		DS, // Depth test X, Depth write O
		PS, // PostProcess PS (Fog + FXAA 통합)
		nullptr, // Blend
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	Pipeline->SetConstantBuffer(0, false, ConstantBufferFogProperties);

	// 소스 텍스처 샘플러 (Scene Color + Scene Depth)
	ID3D11ShaderResourceView* srvs[2] = {SceneColorSRV, SceneDepthSRV};
	Pipeline->GetContext()->PSSetShaderResources(0, 2, srvs);

	// Fullscreen Quad 그리기 (RenderFog과 동일한 방식)
	uint32 stride = sizeof(float) * 5; // Position(3) + TexCoord(2)
	uint32 offset = 0;
	Pipeline->DrawIndexed(3, 0, 0);

	// SRV 언바인드 (경고 방지)
	ID3D11ShaderResourceView* NullSrvs[2] = {nullptr, nullptr};
	Pipeline->GetContext()->PSSetShaderResources(0, 2, NullSrvs);
}

void FFogPass::Release()
{
}
