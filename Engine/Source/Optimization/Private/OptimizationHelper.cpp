#include "pch.h"
#include "Optimization/Public/OptimizationHelper.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Editor/Public/Viewport.h"

/* Primitive�� �߰��� �˾ƾ� �ϴ� �ٸ� ��ü�鿡�� �˸���. */
void Optimization::NotifyPrimitiveAdditionToOthers()
{
	URenderer::GetInstance(). \
		GetViewportClient()->NotifyViewVolumeCullerDirtyToClientCamera();
}

/* Primitive�� ������ �˾ƾ� �ϴ� �ٸ� ��ü�鿡�� �˸���. */
void Optimization::NotifyPrimitiveDeletionToOthers()
{
	URenderer::GetInstance(). \
		GetViewportClient()->NotifyViewVolumeCullerDirtyToClientCamera();
}

/* Primitive�� ���� ������ �˾ƾ� �ϴ� �ٸ� ��ü�鿡�� �˸���. */
void Optimization::NotifyPrimitiveDirtyToOthers()
{
	URenderer::GetInstance(). \
		GetViewportClient()->NotifyViewVolumeCullerDirtyToClientCamera();
}