#include "pch.h"
#include "Editor/Public/Lines.h"

void ULines::MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex)
{
	// �ε��� ���� ����
	InsertStartIndex = std::min(InsertStartIndex, DestVertices.size());

	// �̸� �޸� Ȯ��
	DestVertices.reserve(DestVertices.size() + std::distance(Vertices.begin(), Vertices.end()));

	// ��� �� �ִ� ���� ���
	size_t OverwriteCount = std::min(
		Vertices.size(),
		DestVertices.size() - InsertStartIndex
	);

	// ���� ��� �����
	std::copy(
		Vertices.begin(),
		Vertices.begin() + OverwriteCount,
		DestVertices.begin() + InsertStartIndex
	);
}