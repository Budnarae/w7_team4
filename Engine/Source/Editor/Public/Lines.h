#pragma once
#include "Core/Public/Object.h"
#include "Global/CoreTypes.h"

class ULines : public UObject
{
public:
	ULines() = default;
	~ULines() override = default;

	/**
	 * @brief �������� ��� �迭�� Ư�� ��ġ�� ����
	 * @param DestVertices ��� �迭
	 * @param InsertStartIndex ���� ���� �ε���
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