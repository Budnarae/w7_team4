#include "pch.h"
#include "Render/RenderPass/Public/DebugPass.h"
#include "Render/RenderPass/Public/RenderPass.h"
#include "Editor/Public/Editor.h"

FDebugPass::FDebugPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel)
	: FRenderPass(InPipeline, InConstantBufferViewProj, InConstantBufferModel)
{
}

void FDebugPass::Execute(FRenderingContext& Context)
{
	// PIE 모드가 아닐 때만 디버그 프리미티브 렌더링
	if (GEditor->IsPIESessionActive())
	{
		return;
	}

	// Editor의 RenderDebugPrimitives 호출
	GEditor->GetEditorModule()->RenderDebugPrimitives();
}

void FDebugPass::Release()
{
	// DebugPass는 소유한 리소스가 없으므로 비워둠
}
