#pragma once
#include "Core/Public/Object.h"
#include "Global/CoreTypes.h"

class ULines : public UObject
{
public:
	ULines() = default;
	~ULines() override = default;

	/**
	 * @brief 정점들을 대상 배열의 특정 위치에 병합
	 * @param DestVertices 대상 배열
	 * @param InsertStartIndex 삽입 시작 인덱스
	 */
	void MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex);

	uint32 GetNumVertices() const
	{
		return NumVertices;
	}

protected:
	TArray<FVector> Vertices;
	uint32 NumVertices = 0;
};