#include "pch.h"
#include "Editor/Public/Lines.h"

void ULines::MergeVerticesAt(TArray<FVector>& DestVertices, size_t InsertStartIndex)
{
	// 인덱스 범위 보정
	InsertStartIndex = std::min(InsertStartIndex, DestVertices.size());

	// 미리 메모리 확보
	DestVertices.reserve(DestVertices.size() + std::distance(Vertices.begin(), Vertices.end()));

	// 덮어쓸 수 있는 개수 계산
	size_t OverwriteCount = std::min(
		Vertices.size(),
		DestVertices.size() - InsertStartIndex
	);

	// 기존 요소 덮어쓰기
	std::copy(
		Vertices.begin(),
		Vertices.begin() + OverwriteCount,
		DestVertices.begin() + InsertStartIndex
	);
}