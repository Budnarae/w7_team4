#include "pch.h"
#include "Render/RenderPass/Public/IconPass.h"
#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Public/IconComponent.h"

FIconPass::FIconPass
(
    UPipeline* InPipeline,
    ID3D11Buffer* InConstantBufferViewProj,
    ID3D11Buffer* InConstantBufferModel,
    ID3D11Buffer* InConstantBufferIconProperties,
    ID3D11VertexShader* InVS,
    ID3D11PixelShader* InPS,
    ID3D11InputLayout* InLayout,
    ID3D11DepthStencilState* InDS
)
    :
    FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel),
    ConstantBufferIconProperties(InConstantBufferIconProperties),
    VS(InVS),
    PS(InPS),
    InputLayout(InLayout), DS(InDS)
{
}

void FIconPass::Execute(FRenderingContext& Context)
{
    FRenderState RenderState = UIconComponent::GetClassDefaultRenderState();

    static FPipelineInfo PipelineInfo = {
	    InputLayout,
    	VS,
    	FRenderResourceFactory::GetRasterizerState(RenderState),
    	DS,
    	PS,
    	nullptr
    };

    Pipeline->UpdatePipeline(PipelineInfo);

    if (!(Context.ShowFlags & EEngineShowFlags::SF_Billboard)) return;
    for (UIconComponent* IconComp : Context.Icons)
    {
        // 1) 카메라를 향하는 빌보드 전용 행렬을 갱신
        IconComp->UpdateBillboardMatrix(Context.CurrentCamera->GetLocation());

        Pipeline->SetVertexBuffer(IconComp->GetVertexBuffer(), sizeof(FNormalVertex));
        Pipeline->SetIndexBuffer(IconComp->GetIndexBuffer(), 0);

        // 3) 모델 상수버퍼에는 '월드행렬' 대신 '빌보드 RT 행렬'을 사용
        FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModel, IconComp->GetRTMatrix());
        Pipeline->SetConstantBuffer(0, true, ConstantBufferModel);

        FRenderResourceFactory::UpdateConstantBufferData(
            ConstantBufferIconProperties,
            FIconProperties
            {
                IconComp->GetIconColor(),
                IconComp->GetIconIntensity()
			}
        );
        Pipeline->SetConstantBuffer(2, false, ConstantBufferIconProperties);
        Pipeline->SetTexture(0, false, IconComp->GetSprite().second);
        Pipeline->SetSamplerState(0, false, IconComp->GetSampler());
        Pipeline->DrawIndexed(IconComp->GetNumIndices(), 0, 0);
    }
}

void FIconPass::Release()
{
}
