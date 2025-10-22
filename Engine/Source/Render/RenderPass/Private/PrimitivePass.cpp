#include "pch.h"
#include "Render/RenderPass/Public/PrimitivePass.h"

#include "Render/Renderer/Public/RenderResourceFactory.h"

FPrimitivePass::FPrimitivePass(
	UPipeline* InPipeline,
    ID3D11RenderTargetView* InSceneColorRTV,
    ID3D11RenderTargetView* InSceneNormalRTV,
    ID3D11DepthStencilView* InSceneDepthDSV,
	ID3D11Buffer* InConstantBufferViewProj,
	ID3D11Buffer* InConstantBufferModel,
	ID3D11VertexShader* InVS,
	ID3D11PixelShader* InPS,
	ID3D11InputLayout* InLayout,
	ID3D11DepthStencilState* InDS
	) :
	FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel),
	SceneColorRTV(InSceneColorRTV),
	SceneNormalRTV(InSceneNormalRTV),
	SceneDepthDSV(InSceneDepthDSV),
	VS(InVS),
	PS(InPS),
	InputLayout(InLayout),
	DS(InDS)
{
    ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
    // 초기값으로 초기화 (쓰레기 값 방지)
    FVector4 InitialColor = FVector4{0.0f, 0.0f, 0.0f, 0.0f};
    FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, InitialColor);
}

void FPrimitivePass::Execute(FRenderingContext& Context)
{
	// 첫번째 Target에는 Color, 두번째 Target에는 Normal을 내보냄.
	ID3D11RenderTargetView* RTVs[] = {SceneColorRTV, SceneNormalRTV};
	Pipeline->GetContext()->OMSetRenderTargets(2, RTVs, SceneDepthDSV);

    FRenderState DefaultState;
    if (Context.ViewMode == EViewModeIndex::VMI_Wireframe)
    {
       DefaultState.CullMode = ECullMode::None;
       DefaultState.FillMode = EFillMode::WireFrame;
    }

    // Select shaders based on ViewMode
    ID3D11VertexShader* SelectedVS = VS;
    ID3D11PixelShader* SelectedPS = PS;
    ID3D11InputLayout* SelectedLayout = InputLayout;

    FPipelineInfo PipelineInfo = { SelectedLayout, SelectedVS, nullptr, DS, SelectedPS, nullptr };
    Pipeline->UpdatePipeline(PipelineInfo);
    if (!(Context.ShowFlags & EEngineShowFlags::SF_Primitives)) return;

	Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);
	//FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, Context.CurrentCamera->GetFViewProjConstants());
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
	Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);

    for (UPrimitiveComponent* PrimitiveComponent : Context.DefaultPrimitives)
    {
        if (Context.ViewMode != EViewModeIndex::VMI_Wireframe)
        {
            DefaultState = PrimitiveComponent->GetRenderState();
        }

        PipelineInfo.RasterizerState = FRenderResourceFactory::GetRasterizerState(DefaultState);
        Pipeline->UpdatePipeline(PipelineInfo);

        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, PrimitiveComponent->GetWorldTransformMatrix());
        Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, PrimitiveComponent->GetColor());
        Pipeline->SetConstantBuffer(2, true, ConstantBufferColor);
        Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);
        Pipeline->SetVertexBuffer(PrimitiveComponent->GetVertexBuffer(), sizeof(FNormalVertex));

        if (PrimitiveComponent->GetIndexBuffer() && PrimitiveComponent->GetIndicesData())
        {
           Pipeline->SetIndexBuffer(PrimitiveComponent->GetIndexBuffer(), 0);
           Pipeline->DrawIndexed(PrimitiveComponent->GetNumIndices(), 0, 0);
        }
        else
        {
           Pipeline->Draw(PrimitiveComponent->GetNumVertices(), 0);
        }
    }

	// OMSet RollBack - slot 1의 SceneNormalRTV를 명시적으로 언바인딩
	ID3D11RenderTargetView* SingleRTV[] = {SceneColorRTV, nullptr};
	Pipeline->GetContext()->OMSetRenderTargets(2, SingleRTV, SceneDepthDSV);
}

void FPrimitivePass::Release()
{
    SafeRelease(ConstantBufferColor);
}

void FPrimitivePass::UpdateRenderTargets(ID3D11RenderTargetView* InSceneColorRTV, ID3D11RenderTargetView* InSceneNormalRTV, ID3D11DepthStencilView* InSceneDepthDSV)
{
	SceneColorRTV = InSceneColorRTV;
	SceneNormalRTV = InSceneNormalRTV;
	SceneDepthDSV = InSceneDepthDSV;
}
