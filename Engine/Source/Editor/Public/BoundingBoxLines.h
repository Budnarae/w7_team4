#pragma once
#include "Core/Public/Object.h"
#include "Physics/Public/AABB.h"
#include "Editor/Public/Lines.h"

class UBoundingBoxLines : public ULines
{
public:
	UBoundingBoxLines();
	~UBoundingBoxLines() = default;

	void UpdateVertices(const IBoundingVolume* NewBoundingVolume);

	FAABB* GetDisabledBoundingBox() { return &DisabledBoundingBox; }

private:
	FAABB DisabledBoundingBox = FAABB(FVector(0, 0, 0), FVector(0, 0, 0));
};

