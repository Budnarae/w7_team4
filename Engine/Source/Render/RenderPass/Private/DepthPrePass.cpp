#include "pch.h"
#include "Render/RenderPass/Public/DepthPrePass.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Mesh/Public/StaticMesh.h"
#include "Manager/Asset/Public/AssetManager.h"

FDepthPrePass::FDepthPrePass(
	UPipeline* InPipeline,
	ID3D11DepthStencilView* InDepthDSV,
	ID3D11Buffer* InConstantBufferViewProj,
	ID3D11Buffer* InConstantBufferModel,
	ID3D11VertexShader* InDepthVS,
	ID3D11PixelShader* InDepthPS,
	ID3D11InputLayout* InDepthLayout,
	ID3D11DepthStencilState* InDepthState
) :
	FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel, nullptr),
	DepthDSV(InDepthDSV),
	DepthVS(InDepthVS),
	DepthPS(InDepthPS),
	DepthLayout(InDepthLayout),
	DepthState(InDepthState)
{
}

void FDepthPrePass::Execute(FRenderingContext& Context)
{
	if (!DepthDSV)
	{
		return;
	}

	if (!Context.Viewport.Width || !Context.Viewport.Height)
	{
		return;
	}

	// Depth Pre-pass: Depth만 쓰고 Color는 쓰지 않음
	ID3D11RenderTargetView* NullRTV[] = { nullptr };
	Pipeline->GetContext()->OMSetRenderTargets(1, NullRTV, DepthDSV);

	// IMPORTANT: Viewport는 현재 카메라의 Viewport 사용 (멀티뷰포트 지원)
	// Context.Viewport는 RenderLevel에서 설정됨
	Pipeline->GetContext()->RSSetViewports(1, &Context.Viewport);

	FPipelineInfo PipelineInfo = {
		DepthLayout,
		DepthVS,
		nullptr, // Rasterizer는 기본값 사용
		DepthState,
		DepthPS,
		nullptr, // Blend State 불필요
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	// ViewProj 바인딩
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, *Context.ViewProjConstants);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

	FStaticMesh* CurrentMeshAsset = nullptr;

	for (UStaticMeshComponent* MeshComp : Context.StaticMeshes)
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

		// Empty mesh check
		if (MeshAsset->Indices.empty())
		{
			continue;
		}

		if (CurrentMeshAsset != MeshAsset)
		{
			ID3D11Buffer* VB = MeshComp->GetVertexBuffer();
			ID3D11Buffer* IB = MeshComp->GetIndexBuffer();

			if (!VB || !IB)
			{
				continue;
			}

			Pipeline->SetVertexBuffer(VB, sizeof(FNormalVertex));
			Pipeline->SetIndexBuffer(IB, 0);
			CurrentMeshAsset = MeshAsset;
		}

		// Model 변환 행렬 업데이트
		FMatrix WorldMatrix = MeshComp->GetWorldTransformMatrix();
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, WorldMatrix);
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

		// Depth만 렌더링
		Pipeline->DrawIndexed(static_cast<uint32>(MeshAsset->Indices.size()), 0, 0);
	}

	// DSV 언바인딩 (다음 Pass에서 SRV로 읽을 수 있도록)
	Pipeline->GetContext()->OMSetRenderTargets(0, nullptr, nullptr);
}

void FDepthPrePass::Release()
{
	// 리소스는 Renderer가 소유하므로 여기서는 해제하지 않음
}
