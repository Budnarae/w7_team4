#include "pch.h"
#include "Optimization/Public/ViewVolumeCuller.h"
#include "Core/Public/Object.h"
#include "Global/Octree.h"

void ViewVolumeCuller::Cull(FOctree* StaticOctree, FOctree* DynamicOctree, const FViewProjConstants& ViewProjConstants)
{
	// ������ Cull�ߴ� ������ �����.
	RenderableObjects.clear();
	CurrentFrustum.Clear();

	// 1. ����ü 'Key' ���� 
	FFrustum CurrentFrustum;
	FMatrix VP = ViewProjConstants.View * ViewProjConstants.Projection;
	CurrentFrustum.Planes[0] = VP[3] + VP[0]; // Left
	CurrentFrustum.Planes[1] = VP[3] - VP[0]; // Right
	CurrentFrustum.Planes[2] = VP[3] + VP[1]; // Bottom
	CurrentFrustum.Planes[3] = VP[3] - VP[1]; // Top
	CurrentFrustum.Planes[4] = VP[2];           // Near
	CurrentFrustum.Planes[5] = VP[3] - VP[2]; // Far
	for (int i = 0; i < 6; i++) { CurrentFrustum.Planes[i].Normalize(); }

	// 2. ��Ʈ���� �̿��� ���̴� ��ü�� RenderableObjects�� �����Ѵ�.
	TArray<UPrimitiveComponent*>& TempArray = reinterpret_cast<TArray<UPrimitiveComponent*>&>(RenderableObjects);
	if (StaticOctree)
	{
		StaticOctree->FindVisiblePrimitives(CurrentFrustum, TempArray);
	}
	if (DynamicOctree)
	{
		DynamicOctree->FindVisiblePrimitives(CurrentFrustum, TempArray);
	}
}

const TArray<TObjectPtr<UPrimitiveComponent>>& ViewVolumeCuller::GetRenderableObjects() const
{
	return RenderableObjects;
}