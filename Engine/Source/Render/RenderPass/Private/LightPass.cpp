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
                       ID3D11PixelShader* InUberLightPS,
                       ID3D11SamplerState* InSamplerState,
                       ID3D11DepthStencilState* InDepthLessEqualNoWrite,
                       ID3D11BlendState* InAdditiveBlend)
	: FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel)
	, SceneDepthSRV(InSceneDepthSRV)
	, SceneColorRTV(InSceneColorRTV)
	, LightVolumeVertexShader(InLightVolumeVS)
	, LightVolumeInputLayout(InLightVolumeLayout)
	, LightPixelShader(InUberLightPS)
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
	ID3D11RenderTargetView* SceneRtvs[] = { SceneColorRTV };
	D3DContext->OMSetRenderTargets(1, SceneRtvs, nullptr);
	D3DContext->RSSetViewports(1, &InViewport);

	// Inverse ViewProj Matrix 계산
	const FViewProjConstants& ViewProj = *Context.ViewProjConstants;
	FMatrix ViewProjMatrix = ViewProj.View * ViewProj.Projection;
	FMatrix InvViewProj = ViewProjMatrix.Inverse();

	// AssetManager에서 Sphere Mesh 가져오기 (PointLight와 SpotLight 공통 사용)
	UAssetManager& AssetMgr = UAssetManager::GetInstance();
	ID3D11Buffer* SphereVB = AssetMgr.GetVertexbuffer(EPrimitiveType::Sphere);
	ID3D11Buffer* SphereIB = AssetMgr.GetIndexbuffer(EPrimitiveType::Sphere);
	uint32 SphereNumIndices = AssetMgr.GetNumIndices(EPrimitiveType::Sphere);
	uint32 SphereNumVertices = AssetMgr.GetNumVertices(EPrimitiveType::Sphere);

	// Pipeline 설정 (공통)
	FPipelineInfo PipelineInfo = {
		LightVolumeInputLayout,
		LightVolumeVertexShader,
		FRenderResourceFactory::GetRasterizerState({ ECullMode::Front, EFillMode::Solid }),
		DepthLessEqualNoWrite,
		LightPixelShader,  // 단일 UberShader 사용
		AdditiveBlendState,
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	// Scene Depth Texture 바인딩 (Pixel Shader에서 읽음)
	D3DContext->PSSetShaderResources(0, 1, &SceneDepthSRV);
	Pipeline->SetSamplerState(0, false, LightSamplerState);

	// Constant Buffer 설정 (Slot 순서: 0=Model, 1=ViewProj, 2=LightProperties)
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

	// ===== PointLight 렌더링 =====
	for (auto* PointLight : PointLights)
	{
		if (!PointLight || !PointLight->GetOwner())
		{
			continue;
		}

		// Unified Light Properties 설정
		FLightProperties LightProps = {};
		LightProps.LightPosition = PointLight->GetWorldLocation();
		LightProps.Intensity = PointLight->GetIntensity();
		LightProps.LightColor = PointLight->GetLightColor();
		LightProps.Radius = PointLight->GetAttenuationRadius();
		LightProps.LightDirection = FVector::ZeroVector();  // PointLight는 사용 안함
		LightProps.RadiusFalloff = PointLight->GetLightFalloffExponent();

		// Viewport 정보
		LightProps.ViewportTopLeft = FVector2(InViewport.TopLeftX, InViewport.TopLeftY);
		LightProps.ViewportSize = FVector2(InViewport.Width, InViewport.Height);
		LightProps.SceneRTSize = SceneRTSize;

		// SpotLight 전용 필드 (PointLight는 0으로 초기화)
		LightProps.InnerConeAngle = 0.0f;
		LightProps.OuterConeAngle = 0.0f;

		// LightType 설정 (0 = PointLight)
		LightProps.LightType = 0;
		LightProps.Padding = FVector::ZeroVector();

		// InvViewProj Matrix
		LightProps.InvViewProj = InvViewProj;

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

	// ===== SpotLight 렌더링 =====
	for (auto* SpotLight : SpotLights)
	{
		if (!SpotLight || !SpotLight->GetOwner())
		{
			continue;
		}

		// Unified Light Properties 설정
		FLightProperties LightProps = {};
		LightProps.LightPosition = SpotLight->GetWorldLocation();
		LightProps.Intensity = SpotLight->GetIntensity();
		LightProps.LightColor = SpotLight->GetLightColor();
		LightProps.Radius = SpotLight->GetRadius();
		LightProps.RadiusFalloff = SpotLight->GetRadiusFalloff();

		// Viewport 정보
		LightProps.ViewportTopLeft = FVector2(InViewport.TopLeftX, InViewport.TopLeftY);
		LightProps.ViewportSize = FVector2(InViewport.Width, InViewport.Height);
		LightProps.SceneRTSize = SceneRTSize;

		// SpotLight 전용 필드: Direction
		const FMatrix& TransformMatrix = SpotLight->GetWorldTransformMatrix();
		FVector LightDirection = FVector(TransformMatrix.Data[0][0],
		                                  TransformMatrix.Data[0][1],
		                                  TransformMatrix.Data[0][2]);
		LightDirection.Normalize();
		LightProps.LightDirection = LightDirection;

		// SpotLight 전용 필드: Cone Angles (도 -> 라디안 변환)
		LightProps.InnerConeAngle = FVector::GetDegreeToRadian(SpotLight->GetInnerConeAngle());
		LightProps.OuterConeAngle = FVector::GetDegreeToRadian(SpotLight->GetOuterConeAngle());

		// LightType 설정 (1 = SpotLight)
		LightProps.LightType = 1;
		LightProps.Padding = FVector::ZeroVector();

		// InvViewProj Matrix
		LightProps.InvViewProj = InvViewProj;

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

	// SRV 언바인딩
	ID3D11ShaderResourceView* NullSrv = nullptr;
	D3DContext->PSSetShaderResources(0, 1, &NullSrv);
}

void FLightPass::Release()
{
	// Constant Buffer만 Pass에서 해제
	SafeRelease(ConstantBufferLightProperties);
}
