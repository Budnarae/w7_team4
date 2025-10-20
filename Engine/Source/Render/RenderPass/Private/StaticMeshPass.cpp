#include "pch.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Texture/Public/Texture.h"
#include "Texture/Public/TextureRenderProxy.h"

FStaticMeshPass::FStaticMeshPass(
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
	) :
	FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel, InConstantBufferLighting),
	SceneColorRTV(InSceneColorRTV),
	SceneNormalRTV(InSceneNormalRTV),
	SceneDepthDSV(InSceneDepthDSV),
	VS(InVS),
	PS(InPS),
	InputLayout(InLayout),
	DS(InDS)
{
	ConstantBufferMaterial = FRenderResourceFactory::CreateConstantBuffer<FMaterialConstants>();
}

void FStaticMeshPass::Execute(FRenderingContext& Context)
{
	// 첫번째 Target에는 Color, 두번째 Target에는 Normal을 내보냄.
	ID3D11RenderTargetView* RTVs[] = {SceneColorRTV, SceneNormalRTV};
	Pipeline->GetContext()->OMSetRenderTargets(2, RTVs, SceneDepthDSV);

	if (!(Context.ShowFlags & EEngineShowFlags::SF_StaticMesh))
	{
		return;
	}
	TArray<UStaticMeshComponent*>& MeshComponents = Context.StaticMeshes;
	sort(MeshComponents.begin(), MeshComponents.end(),
		[](UStaticMeshComponent* A, UStaticMeshComponent* B) {
			int32 MeshA = A->GetStaticMesh() ? A->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			int32 MeshB = B->GetStaticMesh() ? B->GetStaticMesh()->GetAssetPathFileName().GetComparisonIndex() : 0;
			return MeshA < MeshB;
		});

	FStaticMesh* CurrentMeshAsset = nullptr;
	UMaterial* CurrentMaterial = nullptr;
	FRenderState RenderState = UStaticMeshComponent::GetClassDefaultRenderState();
	if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
	{
		RenderState.CullMode = ECullMode::None; RenderState.FillMode = EFillMode::WireFrame;
	}
	ID3D11RasterizerState* RS = FRenderResourceFactory::GetRasterizerState(RenderState);

	// Select shaders based on ViewMode
	ID3D11VertexShader* SelectedVS = VS;
	ID3D11PixelShader* SelectedPS = PS;
	ID3D11InputLayout* SelectedLayout = InputLayout;

	auto& Renderer = URenderer::GetInstance();
	switch (Context.ViewMode)
	{
	case EViewModeIndex::VMI_Lit_Lambert:
		SelectedVS = Renderer.GetUberLitVertexShader();
		SelectedPS = Renderer.GetTextureLitPixelShader();
		SelectedLayout = Renderer.GetUberLitInputLayout();
		break;
	case EViewModeIndex::VMI_Unlit:
		SelectedVS = Renderer.GetUberLitVertexShader();
		SelectedPS = Renderer.GetTextureUnlitPixelShader();
		SelectedLayout = Renderer.GetUberLitInputLayout();
		break;
	case EViewModeIndex::VMI_SceneDepth:
		SelectedVS = Renderer.GetDepthVertexShader();
		SelectedPS = Renderer.GetDepthPixelShader();
		SelectedLayout = Renderer.GetDepthInputLayout();
		break;
	case EViewModeIndex::VMI_Wireframe:
		// Wireframe은 PS 필요 없음
		break;
	default:
		break;
	}

	FPipelineInfo PipelineInfo = { SelectedLayout, SelectedVS, RS, DS, SelectedPS, nullptr };
	Pipeline->UpdatePipeline(PipelineInfo);

	// ViewProj cbuffer 바인딩
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

	// Lighting cbuffer 바인딩
	if (ConstantBufferLighting)
	{
		Pipeline->SetConstantBuffer(3, true, ConstantBufferLighting);
		Pipeline->SetConstantBuffer(3, false, ConstantBufferLighting);
	}

	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (!MeshComp->GetStaticMesh())
		{
			continue;
		}
		FStaticMesh* MeshAsset = MeshComp->GetStaticMesh()->GetStaticMeshAsset();
		if (!MeshAsset)
		{
			continue;
		}

		if (CurrentMeshAsset != MeshAsset)
		{
			Pipeline->SetVertexBuffer(MeshComp->GetVertexBuffer(), sizeof(FNormalVertex));
			Pipeline->SetIndexBuffer(MeshComp->GetIndexBuffer(), 0);
			CurrentMeshAsset = MeshAsset;
		}

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, MeshComp->GetWorldTransformMatrix());
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

		if (MeshAsset->MaterialInfo.empty() || MeshComp->GetStaticMesh()->GetNumMaterials() == 0)
		{
			Pipeline->DrawIndexed(static_cast<uint32>(MeshAsset->Indices.size()), 0, 0);
			continue;
		}

		if (MeshComp->IsScrollEnabled())
		{
			MeshComp->SetElapsedTime(MeshComp->GetElapsedTime() + UTimeManager::GetInstance().GetDeltaTime());
		}

		for (const FMeshSection& Section : MeshAsset->Sections)
		{
			UMaterial* Material = MeshComp->GetMaterial(Section.MaterialSlot);
			if (CurrentMaterial != Material) {
				FMaterialConstants MaterialConstants = {};
				FVector AmbientColor = Material->GetAmbientColor(); MaterialConstants.Ka = FVector4(AmbientColor.X, AmbientColor.Y, AmbientColor.Z, 1.0f);
				FVector DiffuseColor = Material->GetDiffuseColor(); MaterialConstants.Kd = FVector4(DiffuseColor.X, DiffuseColor.Y, DiffuseColor.Z, 1.0f);
				FVector SpecularColor = Material->GetSpecularColor(); MaterialConstants.Ks = FVector4(SpecularColor.X, SpecularColor.Y, SpecularColor.Z, 1.0f);
				MaterialConstants.Ns = Material->GetSpecularExponent();
				MaterialConstants.Ni = Material->GetRefractionIndex();
				MaterialConstants.D = Material->GetDissolveFactor();
				MaterialConstants.MaterialFlags = 0;
				MaterialConstants.Time = MeshComp->GetElapsedTime();

				// MaterialFlags 설정: 텍스처 바인딩 여부 표시
				if (UTexture* DiffuseTexture = Material->GetDiffuseTexture())
				{
					if(auto* Proxy = DiffuseTexture->GetRenderProxy())
					{
						MaterialConstants.MaterialFlags |= (1 << 0); // HAS_DIFFUSE_MAP
						Pipeline->SetTexture(0, false, Proxy->GetSRV());
						Pipeline->SetSamplerState(0, false, Proxy->GetSampler());
					}
				}
				if (UTexture* AmbientTexture = Material->GetAmbientTexture())
				{
					if(auto* Proxy = AmbientTexture->GetRenderProxy())
					{
						MaterialConstants.MaterialFlags |= (1 << 1); // HAS_AMBIENT_MAP
						Pipeline->SetTexture(1, false, Proxy->GetSRV());
					}
				}
				if (UTexture* SpecularTexture = Material->GetSpecularTexture())
				{
					if(auto* Proxy = SpecularTexture->GetRenderProxy())
					{
						MaterialConstants.MaterialFlags |= (1 << 2); // HAS_SPECULAR_MAP
						Pipeline->SetTexture(2, false, Proxy->GetSRV());
					}
				}
				if (UTexture* NormalTexture = Material->GetNormalTexture())
				{
					if(auto* Proxy = NormalTexture->GetRenderProxy())
					{
						MaterialConstants.MaterialFlags |= (1 << 3); // HAS_NORMAL_MAP
						Pipeline->SetTexture(3, false, Proxy->GetSRV());
					}
				}
				if (UTexture* AlphaTexture = Material->GetAlphaTexture())
				{
					if(auto* Proxy = AlphaTexture->GetRenderProxy())
					{
						MaterialConstants.MaterialFlags |= (1 << 4); // HAS_ALPHA_MAP
						Pipeline->SetTexture(4, false, Proxy->GetSRV());
					}
				}

				FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferMaterial, MaterialConstants);
				Pipeline->SetConstantBuffer(2, false, ConstantBufferMaterial);

				CurrentMaterial = Material;
			}
			Pipeline->DrawIndexed(Section.IndexCount, Section.StartIndex, 0);
		}
	}
	Pipeline->SetConstantBuffer(2, false, nullptr);

	// OMSet RollBack - slot 1의 SceneNormalRTV를 명시적으로 언바인딩
	ID3D11RenderTargetView* SingleRTV[] = {SceneColorRTV, nullptr};
	Pipeline->GetContext()->OMSetRenderTargets(2, SingleRTV, SceneDepthDSV);
}

void FStaticMeshPass::Release()
{
	SafeRelease(ConstantBufferMaterial);
}
