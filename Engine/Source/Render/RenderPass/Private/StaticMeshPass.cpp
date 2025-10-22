#include "pch.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Texture/Public/Texture.h"
#include "Texture/Public/TextureRenderProxy.h"
#include "Render/RenderPass/Public/LightCullingPass.h"

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
	ConstantBufferSceneInfo = FRenderResourceFactory::CreateConstantBuffer<FSceneInfoConstants>();
}

void FStaticMeshPass::Execute(FRenderingContext& Context)
{
	auto& Renderer = URenderer::GetInstance();

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

	// ViewProj cbuffer 업데이트 및 바인딩
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, *Context.ViewProjConstants);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
	Pipeline->SetConstantBuffer(1, false, ConstantBufferViewProj);

	// Lighting cbuffer 바인딩
	if (ConstantBufferLighting)
	{
		Pipeline->SetConstantBuffer(3, true, ConstantBufferLighting);
		Pipeline->SetConstantBuffer(3, false, ConstantBufferLighting);
	}

	// SceneInfo cbuffer 업데이트 및 바인딩
	if (ConstantBufferSceneInfo)
	{
		FSceneInfoConstants SceneInfo = {};
		SceneInfo.ViewportTopLeft = FVector2(Context.Viewport.TopLeftX, Context.Viewport.TopLeftY);
		SceneInfo.ViewportSize = FVector2(Context.Viewport.Width, Context.Viewport.Height);
		SceneInfo.SceneRTSize = Context.SceneRTSize;

		// NumTiles 계산
		uint32 ScreenWidth = static_cast<uint32>(Context.SceneRTSize.X);
		uint32 ScreenHeight = static_cast<uint32>(Context.SceneRTSize.Y);
		SceneInfo.NumTilesX = (ScreenWidth + 31) / 32;
		SceneInfo.NumTilesY = (ScreenHeight + 31) / 32;

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferSceneInfo, SceneInfo);
		Pipeline->SetConstantBuffer(4, true, ConstantBufferSceneInfo);
		Pipeline->SetConstantBuffer(4, false, ConstantBufferSceneInfo);
	}

	// Tile Light 인덱스 리스트 SRV 바인딩
	if (auto* LightCullingPass = Renderer.GetLightCullingPass())
	{
		if (auto* OffsetsSRV = LightCullingPass->GetTileLightOffsetsSRV())
		{
			Pipeline->SetTexture(4, false, OffsetsSRV);
		}
		if (auto* CountsSRV = LightCullingPass->GetTileLightCountsSRV())
		{
			Pipeline->SetTexture(5, false, CountsSRV);
		}
		if (auto* IndicesSRV = LightCullingPass->GetTileLightIndicesSRV())
		{
			Pipeline->SetTexture(6, false, IndicesSRV);
		}
	}

	// Select shaders based on ViewMode
	ID3D11VertexShader* SelectedVS = VS;
	ID3D11PixelShader* SelectedPS = PS;
	ID3D11InputLayout* SelectedLayout = InputLayout;

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

		FRenderResourceFactory::UpdateConstantBufferData(
			ConstantBufferModel,
			FModelForLight
			{
				MeshComp->GetWorldTransformMatrix(),
				MeshComp->GetWorldTransformMatrix().Transpose().Inverse()
			}
			);
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
				if (UTexture* NormalTexture = Material->GetBumpTexture())
				{
					if(auto* Proxy = NormalTexture->GetRenderProxy())
					{
						MaterialConstants.MaterialFlags |= (1 << 3); // HAS_NORMAL_MAP
						Pipeline->SetTexture(3, false, Proxy->GetSRV());
					}

					// Normal Mapping을 위한 셰이더 선택
					bool bUseNormalMapping = false;
					if (Context.ViewMode == EViewModeIndex::VMI_Lit_Lambert)
					{
						SelectedVS = Renderer.GetUberLitNormalMappingVertexShader();
						SelectedPS = Renderer.GetUberLambertNormalMappingPixelShader();
						SelectedLayout = Renderer.GetUberLitNormalMappingInputLayout();
						bUseNormalMapping = true;
					}
					else if (Context.ViewMode == EViewModeIndex::VMI_Lit_Blinn_Phong)
					{
						SelectedVS = Renderer.GetUberLitNormalMappingVertexShader();
						SelectedPS = Renderer.GetUberPhongNormalMappingPixelShader();
						SelectedLayout = Renderer.GetUberLitNormalMappingInputLayout();
						bUseNormalMapping = true;
					}
					else if (Context.ViewMode == EViewModeIndex::VMI_Normal)
					{
						SelectedVS = Renderer.GetUberLitNormalMappingVertexShader();
						SelectedPS = Renderer.GetUberPhongNormalMappingPixelShader();
						SelectedLayout = Renderer.GetUberLitNormalMappingInputLayout();
						bUseNormalMapping = true;
					}
					else if (Context.ViewMode == EViewModeIndex::VMI_LightComplexity)
					{
						// LightComplexity 모드: Normal Mapping 적용된 Phong 쉐이더 사용
						SelectedVS = Renderer.GetUberLitNormalMappingVertexShader();
						SelectedPS = Renderer.GetUberPhongNormalMappingPixelShader();
						SelectedLayout = Renderer.GetUberLitNormalMappingInputLayout();
						bUseNormalMapping = true;
					}
					else if (Context.ViewMode == EViewModeIndex::VMI_Lit_Gouraud)
					{
						SelectedVS = Renderer.GetUberLitGouraudVertexShader();
						SelectedPS = Renderer.GetTextureGouraudPixelShader();
						SelectedLayout = Renderer.GetUberLitGouraudInputLayout();
					}
					else if (Context.ViewMode == EViewModeIndex::VMI_Unlit)
					{
						SelectedVS = Renderer.GetUberLitVertexShader();
						SelectedPS = Renderer.GetTextureUnlitPixelShader();
						SelectedLayout = Renderer.GetUberLitInputLayout();
					}

					// Normal Mapping 사용 시 TBN을 포함한 확장 버텍스 버퍼 생성
					if (bUseNormalMapping)
					{
						// FNormalVertex 데이터 읽기
						const TArray<FNormalVertex>& OriginalVertices = MeshAsset->Vertices;
						const TArray<uint32>& Indices = MeshAsset->Indices;
						TArray<FNormalMapping> NormalMappingVertices;
						NormalMappingVertices.resize(OriginalVertices.size());

						// 각 버텍스의 TBN 초기화
						TArray<FVector> Tangents(OriginalVertices.size(), FVector::ZeroVector());
						TArray<FVector> Bitangents(OriginalVertices.size(), FVector::ZeroVector());

						// 삼각형 단위로 Tangent와 Bitangent 계산 (UV gradient 기반)
						for (size_t i = 0; i < Indices.size(); i += 3)
						{
							uint32 i0 = Indices[i];
							uint32 i1 = Indices[i + 1];
							uint32 i2 = Indices[i + 2];

							const FNormalVertex& v0 = OriginalVertices[i0];
							const FNormalVertex& v1 = OriginalVertices[i1];
							const FNormalVertex& v2 = OriginalVertices[i2];

							// Edge vectors
							FVector Edge1 = v1.Position - v0.Position;
							FVector Edge2 = v2.Position - v0.Position;

							// UV deltas
							FVector2 DeltaUV1 = v1.TexCoord - v0.TexCoord;
							FVector2 DeltaUV2 = v2.TexCoord - v0.TexCoord;

							// Tangent와 Bitangent 계산
							float f = DeltaUV1.X * DeltaUV2.Y - DeltaUV2.X * DeltaUV1.Y;

							FVector Tangent, Bitangent;
							if (fabs(f) > 1e-6f)
							{
								f = 1.0f / f;
								Tangent.X = f * (DeltaUV2.Y * Edge1.X - DeltaUV1.Y * Edge2.X);
								Tangent.Y = f * (DeltaUV2.Y * Edge1.Y - DeltaUV1.Y * Edge2.Y);
								Tangent.Z = f * (DeltaUV2.Y * Edge1.Z - DeltaUV1.Y * Edge2.Z);

								Bitangent.X = f * (-DeltaUV2.X * Edge1.X + DeltaUV1.X * Edge2.X);
								Bitangent.Y = f * (-DeltaUV2.X * Edge1.Y + DeltaUV1.X * Edge2.Y);
								Bitangent.Z = f * (-DeltaUV2.X * Edge1.Z + DeltaUV1.X * Edge2.Z);
							}
							else
							{
								// Degenerate UV case: fallback to arbitrary tangent
								FVector N = v0.Normal;
								N.Normalize();
								// 오른손 좌표계 유지
								Tangent = (fabs(N.X) < 0.9f) ? N.Cross(FVector(0, 0, 1)) : N.Cross(FVector(1, 0, 0));
								Tangent.Normalize();
								Bitangent = N.Cross(Tangent);
							}

							// 세 버텍스에 누적 (평균을 위해)
							Tangents[i0] = Tangents[i0] + Tangent;
							Tangents[i1] = Tangents[i1] + Tangent;
							Tangents[i2] = Tangents[i2] + Tangent;

							Bitangents[i0] = Bitangents[i0] + Bitangent;
							Bitangents[i1] = Bitangents[i1] + Bitangent;
							Bitangents[i2] = Bitangents[i2] + Bitangent;
						}

						// 각 버텍스에 대해 정규화 및 Gram-Schmidt 직교화
						for (size_t i = 0; i < OriginalVertices.size(); ++i)
						{
							const FNormalVertex& Vertex = OriginalVertices[i];
							FNormalMapping& NMVertex = NormalMappingVertices[i];
							NMVertex.NormalVertex = Vertex;

							FVector Normal = Vertex.Normal;
							Normal.Normalize();

							FVector Tangent = Tangents[i];
							Tangent.Normalize();

							// Gram-Schmidt 직교화: T' = T - (N·T)N
							Tangent = Tangent - Normal * Normal.Dot(Tangent);
							Tangent.Normalize();

							// Bitangent 재계산 (일관성 보장) - 오른손 좌표계
							FVector Bitangent = Normal.Cross(Tangent);
							Bitangent.Normalize();

							NMVertex.Tangent = Tangent;
							NMVertex.BiTangent = Bitangent;
							NMVertex.TangentNormal = Normal;
						}

						// 동적 버텍스 버퍼 생성
						ID3D11Buffer* TempNormalMappingVB = nullptr;
						D3D11_BUFFER_DESC vbDesc = {};
						vbDesc.Usage = D3D11_USAGE_DYNAMIC;
						vbDesc.ByteWidth = static_cast<UINT>(sizeof(FNormalMapping) * NormalMappingVertices.size());
						vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
						vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

						D3D11_SUBRESOURCE_DATA initData = {};
						initData.pSysMem = NormalMappingVertices.data();

						ID3D11Device* Device = nullptr;
						Pipeline->GetContext()->GetDevice(&Device);
						if (Device && SUCCEEDED(Device->CreateBuffer(&vbDesc, &initData, &TempNormalMappingVB)))
						{
							Pipeline->SetVertexBuffer(TempNormalMappingVB, sizeof(FNormalMapping));
							TempNormalMappingVB->Release(); // SetVertexBuffer가 참조 증가시키므로 즉시 Release
						}
						if (Device) Device->Release();
					}
				}
				else
				{
					switch (Context.ViewMode)
					{
					case EViewModeIndex::VMI_Lit_Lambert:
						SelectedVS = Renderer.GetUberLitVertexShader();
						SelectedPS = Renderer.GetTextureLambertPixelShader();
						SelectedLayout = Renderer.GetUberLitInputLayout();
						break;
					case EViewModeIndex::VMI_Lit_Gouraud:
						SelectedVS = Renderer.GetUberLitGouraudVertexShader();
						SelectedPS = Renderer.GetTextureGouraudPixelShader();
						SelectedLayout = Renderer.GetUberLitGouraudInputLayout();
						break;
					case EViewModeIndex::VMI_Lit_Blinn_Phong:
						SelectedVS = Renderer.GetUberLitVertexShader();
						SelectedPS = Renderer.GetTexturePhongPixelShader();
						SelectedLayout = Renderer.GetUberLitInputLayout();
						break;
					case EViewModeIndex::VMI_Normal:
						// Normal Map이 없으면 Vertex Normal을 사용하는 일반 쉐이더
						SelectedVS = Renderer.GetUberLitVertexShader();
						SelectedPS = Renderer.GetTexturePhongPixelShader();
						SelectedLayout = Renderer.GetUberLitInputLayout();
						break;
					case EViewModeIndex::VMI_Unlit:
						SelectedVS = Renderer.GetUberLitVertexShader();
						SelectedPS = Renderer.GetTextureUnlitPixelShader();
						SelectedLayout = Renderer.GetUberLitInputLayout();
						break;
					case EViewModeIndex::VMI_SceneDepth:
						// SceneDepthPass가 최종 시각화를 담당
						// StaticMeshPass는 일반 렌더링으로 Depth 버퍼에만 기록
						break;
					case EViewModeIndex::VMI_LightComplexity:
						// LightComplexity 모드: Lit 쉐이더로 라이팅 적용 후 heat map 오버레이
						SelectedVS = Renderer.GetUberLitVertexShader();
						SelectedPS = Renderer.GetTexturePhongPixelShader();
						SelectedLayout = Renderer.GetUberLitInputLayout();
						break;
					case EViewModeIndex::VMI_Wireframe:
						// Wireframe은 PS 필요 없음
						break;
					default:
						break;
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
			FPipelineInfo PipelineInfo = { SelectedLayout, SelectedVS, RS, DS, SelectedPS, nullptr };
			Pipeline->UpdatePipeline(PipelineInfo);

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
	SafeRelease(ConstantBufferSceneInfo);
}

void FStaticMeshPass::UpdateRenderTargets(ID3D11RenderTargetView* InSceneColorRTV, ID3D11RenderTargetView* InSceneNormalRTV, ID3D11DepthStencilView* InSceneDepthDSV)
{
	SceneColorRTV = InSceneColorRTV;
	SceneNormalRTV = InSceneNormalRTV;
	SceneDepthDSV = InSceneDepthDSV;
}
