#include "pch.h"
#include "Render/RenderPass/Public/LightPass.h"

#include "Component/Light/Public/PointLightComponent.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Level/Public/Level.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/Renderer/Public/Pipeline.h"

FLightPass::FLightPass(UPipeline* InPipeline,
                       ID3D11Buffer* InConstantBufferViewProj,
                       ID3D11Buffer* InConstantBufferModel,
                       ID3D11ShaderResourceView* InSceneDepthSRV,
                       ID3D11RenderTargetView* InSceneColorRTV,
                       ID3D11VertexShader* InLightVolumeVS,
                       ID3D11InputLayout* InLightVolumeLayout,
                       ID3D11PixelShader* InPointLightPS,
                       ID3D11PixelShader* InSpotLightPS,
                       ID3D11SamplerState* InSamplerState,
                       ID3D11DepthStencilState* InDepthLessEqualNoWrite,
                       ID3D11BlendState* InAdditiveBlend)
	: FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel)
	, SceneDepthSRV(InSceneDepthSRV)
	, SceneColorRTV(InSceneColorRTV)
	, LightVolumeVertexShader(InLightVolumeVS)
	, LightVolumeInputLayout(InLightVolumeLayout)
	, PointLightPixelShader(InPointLightPS)
	, SpotLightPixelShader(InSpotLightPS)
	, LightSamplerState(InSamplerState)
	, DepthLessEqualNoWrite(InDepthLessEqualNoWrite)
	, AdditiveBlendState(InAdditiveBlend)
{
	// Constant Buffer 생성
	ConstantBufferLightProperties = FRenderResourceFactory::CreateConstantBuffer<FLightProperties>();
}

void FLightPass::Execute(FRenderingContext& Context)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel)
	{
		return;
	}

	const auto& PointLights = CurrentLevel->GetAllPointLights();
	const auto& SpotLights = CurrentLevel->GetAllSpotLights();

	// 렌더링할 라이트가 없으면 리턴
	if (PointLights.empty() && SpotLights.empty())
	{
		return;
	}

	auto* D3DContext = URenderer::GetInstance().GetDeviceContext();
	const D3D11_VIEWPORT& InViewport = Context.Viewport;
	const FVector2& SceneRTSize = Context.SceneRTSize;

	// Scene RT 바인딩 (DSV는 nullptr - Depth를 SRV로 읽어야 하므로)
	ID3D11RenderTargetView* SceneRenderTargetView[] = { SceneColorRTV };
	D3DContext->OMSetRenderTargets(1, SceneRenderTargetView, nullptr);
	D3DContext->RSSetViewports(1, &InViewport);

	// Inverse ViewProj Matrix 계산
	const FViewProjConstants& ViewProj = *Context.ViewProjConstants;
	FMatrix ViewProjMatrix = ViewProj.View * ViewProj.Projection;
	FMatrix InvViewProj = ViewProjMatrix.Inverse();

	// Pipeline 공통 설정
	// PixelShader는 각 세부 Light에서 직접 세팅
	FPipelineInfo PipelineInfo = {
		LightVolumeInputLayout,
		LightVolumeVertexShader,
		FRenderResourceFactory::GetRasterizerState({ ECullMode::Front, EFillMode::Solid }),
		DepthLessEqualNoWrite,
		nullptr, // PS
		AdditiveBlendState,
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};

	Pipeline->UpdatePipeline(PipelineInfo);

	// Scene Depth Texture 바인딩
	D3DContext->PSSetShaderResources(0, 1, &SceneDepthSRV);
	Pipeline->SetSamplerState(0, false, LightSamplerState);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

	// PointLight 렌더링
	if (!PointLights.empty())
	{
		RenderPointLights(PointLights, InvViewProj, InViewport, SceneRTSize);
	}

	// SpotLight 렌더링
	if (!SpotLights.empty())
	{
		RenderSpotLights(SpotLights, InvViewProj, InViewport, SceneRTSize);
	}

	// SRV 언바인딩
	ID3D11ShaderResourceView* NullSrv = nullptr;
	D3DContext->PSSetShaderResources(0, 1, &NullSrv);
}

void FLightPass::RenderPointLights(const TArray<UPointLightComponent*>& PointLights,
                                   const FMatrix& InvViewProj,
                                   const D3D11_VIEWPORT& Viewport,
                                   const FVector2& SceneRTSize) const
{
	// PointLight 전용 Pixel Shader 설정
	if (!PointLightPixelShader)
	{
		UE_LOG("LightPass: PointLightPixelShader is nullptr");
		return;
	}

	Pipeline->UpdatePixelShaderOnly(PointLightPixelShader);

	// Sphere Mesh 가져오기
	UAssetManager& AssetManager = UAssetManager::GetInstance();
	ID3D11Buffer* SphereVB = AssetManager.GetVertexbuffer(EPrimitiveType::Sphere);
	ID3D11Buffer* SphereIB = AssetManager.GetIndexbuffer(EPrimitiveType::Sphere);
	uint32 SphereNumIndices = AssetManager.GetNumIndices(EPrimitiveType::Sphere);
	uint32 SphereNumVertices = AssetManager.GetNumVertices(EPrimitiveType::Sphere);

	for (auto* PointLight : PointLights)
	{
		if (!PointLight || !PointLight->GetOwner())
		{
			continue;
		}

		// Light Properties 설정
		FLightProperties LightProps = {};
		LightProps.InvViewProj = InvViewProj;
		LightProps.LightPosition = PointLight->GetWorldLocation();
		LightProps.Intensity = PointLight->GetIntensity();
		LightProps.LightColor = PointLight->GetLightColor();
		LightProps.Radius = PointLight->GetAttenuationRadius();
		LightProps.RadiusFalloff = PointLight->GetLightFalloffExponent();
		LightProps._Padding0 = 0.0f;
		LightProps.ViewportTopLeft = FVector2(Viewport.TopLeftX, Viewport.TopLeftY);
		LightProps.ViewportSize = FVector2(Viewport.Width, Viewport.Height);
		LightProps.SceneRTSize = SceneRTSize;

		// PointLight는 SpotLight 필드 미사용 (0으로 초기화)
		LightProps.LightDirection = FVector::ZeroVector();
		LightProps.InnerConeAngle = 0.0f;
		LightProps.OuterConeAngle = 0.0f;
		LightProps._Padding1 = FVector::ZeroVector();

		// Constant Buffer 업데이트
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLightProperties, LightProps);
		Pipeline->SetConstantBuffer(2, false, ConstantBufferLightProperties);

		// World Transform (Sphere를 라이트 위치/반경으로 스케일)
		FVector LightPos = PointLight->GetWorldLocation();
		FVector LightScale = FVector(PointLight->GetAttenuationRadius(),
		                              PointLight->GetAttenuationRadius(),
		                              PointLight->GetAttenuationRadius());
		FMatrix WorldMatrix = FMatrix::GetModelMatrix(LightPos, FVector::ZeroVector(), LightScale);

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

		// Sphere Mesh 렌더링
		Pipeline->SetVertexBuffer(SphereVB, sizeof(FNormalVertex));
		if (SphereIB)
		{
			Pipeline->SetIndexBuffer(SphereIB, 0);
			Pipeline->DrawIndexed(SphereNumIndices, 0, 0);
		}
		else
		{
			Pipeline->Draw(SphereNumVertices, 0);
		}
	}
}

void FLightPass::RenderSpotLights(const TArray<USpotLightComponent*>& SpotLights,
                                  const FMatrix& InvViewProj,
                                  const D3D11_VIEWPORT& Viewport,
                                  const FVector2& SceneRTSize) const
{
	// SpotLight 전용 Pixel Shader 설정
	if (!SpotLightPixelShader)
	{
		UE_LOG("LightPass: SpotLightPixelShader is nullptr");
		return;
	}

	Pipeline->UpdatePixelShaderOnly(SpotLightPixelShader);

	// Sphere Mesh 가져오기
	UAssetManager& AssetMgr = UAssetManager::GetInstance();
	ID3D11Buffer* SphereVB = AssetMgr.GetVertexbuffer(EPrimitiveType::Sphere);
	ID3D11Buffer* SphereIB = AssetMgr.GetIndexbuffer(EPrimitiveType::Sphere);
	uint32 SphereNumIndices = AssetMgr.GetNumIndices(EPrimitiveType::Sphere);
	uint32 SphereNumVertices = AssetMgr.GetNumVertices(EPrimitiveType::Sphere);

	for (auto* SpotLight : SpotLights)
	{
		if (!SpotLight || !SpotLight->GetOwner())
		{
			continue;
		}

		// Light Properties 설정
		FLightProperties LightProps = {};
		LightProps.InvViewProj = InvViewProj;
		LightProps.LightPosition = SpotLight->GetWorldLocation();
		LightProps.Intensity = SpotLight->GetIntensity();
		LightProps.LightColor = SpotLight->GetLightColor();
		LightProps.Radius = SpotLight->GetRadius();
		LightProps.RadiusFalloff = SpotLight->GetLightFalloffExponent();
		LightProps._Padding0 = 0.0f;
		LightProps.ViewportTopLeft = FVector2(Viewport.TopLeftX, Viewport.TopLeftY);
		LightProps.ViewportSize = FVector2(Viewport.Width, Viewport.Height);
		LightProps.SceneRTSize = SceneRTSize;

		// SpotLight 전용 필드
		const FMatrix& TransformMatrix = SpotLight->GetWorldTransformMatrix();
		FVector LightDirection = FVector(TransformMatrix.Data[0][0],
		                                  TransformMatrix.Data[0][1],
		                                  TransformMatrix.Data[0][2]);
		LightDirection.Normalize();
		LightProps.LightDirection = LightDirection;
		LightProps.InnerConeAngle = FVector::GetDegreeToRadian(SpotLight->GetInnerConeAngle());
		LightProps.OuterConeAngle = FVector::GetDegreeToRadian(SpotLight->GetOuterConeAngle());
		LightProps._Padding1 = FVector::ZeroVector();

		// Constant Buffer 업데이트
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLightProperties, LightProps);
		Pipeline->SetConstantBuffer(2, false, ConstantBufferLightProperties);

		// World Transform (Sphere를 라이트 위치/반경으로 스케일)
		FVector LightPos = SpotLight->GetWorldLocation();
		FVector LightScale = FVector(SpotLight->GetRadius(), SpotLight->GetRadius(), SpotLight->GetRadius());
		FMatrix WorldMatrix = FMatrix::GetModelMatrix(LightPos, FVector::ZeroVector(), LightScale);

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

		// Sphere Mesh 렌더링
		Pipeline->SetVertexBuffer(SphereVB, sizeof(FNormalVertex));
		if (SphereIB)
		{
			Pipeline->SetIndexBuffer(SphereIB, 0);
			Pipeline->DrawIndexed(SphereNumIndices, 0, 0);
		}
		else
		{
			Pipeline->Draw(SphereNumVertices, 0);
		}
	}
}

void FLightPass::Release()
{
	SafeRelease(ConstantBufferLightProperties);
}
