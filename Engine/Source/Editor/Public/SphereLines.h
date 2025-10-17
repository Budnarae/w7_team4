#pragma once
#include "Core/Public/Object.h"
#include "Physics/Public/AABB.h"
#include "Editor/Public/Lines.h"

class USphereLines : public ULines
{
public:
	USphereLines();
	~USphereLines() = default;

	void UpdateVertices(const FVector& CenterPosition, float Radius);
	void DisableSphere()
	{
		Vertices.clear();
		for (int i = 0; i < Segments * 3; i++)
		{
			Vertices.push_back(FVector(0.0f, 0.0f, 0.0f));
		}
	}

private:
	inline static const uint32 Segments = 64; // 구를 표현할 선분의 개수
public:
	static uint32 GetSegments() { return Segments; }
};