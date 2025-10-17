#pragma once
#include "Global/Types.h"
#include "Global/CoreTypes.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Editor/Public/Grid.h"
#include "Editor/Public/BoundingBoxLines.h"
#include "Editor/Public/ConeLines.h"
#include "Editor/Public/SphereLines.h"

struct FVertex;

class UBatchLines : UObject
{
public:
	UBatchLines();
	~UBatchLines();

	void AddLines(const TArray<FVector>& InVertices, const TArray<uint32>& InIndices);

	void AddGridLines(const float NewCellSize);
	void AddAABBLines(const IBoundingVolume* NewBoundingVolume);
	void AddConeLines(
		const FVector& Apex,
		const FVector& Direction,
		const FVector& UpVector,
		float Angle,
		const FVector& DecalBoxSize
	);
	void AddSphereLines(const FVector& CenterPosition, float Radius);

	// 종류별 Vertices 업데이트
	//void AddAABBLines(const IBoundingVolume* NewBoundingVolume);
	/*
	void AddConeLines(const FVector& Apex, const FVector& Direction, const FVector& UpVector,
							float Angle, const FVector& DecalBoxSize);
							
	void AddSphereLines(const FVector& CenterPosition, float Radius);
	*/

	// GPU VertexBuffer에 복사
	void UpdateVertexBuffer();

	void ResetLines();

	float GetCellSize() const
	{
		return Grid.GetCellSize();
	}

	/*void SetCellSize(const float newCellSize)
	{
		Grid.SetCellSize(newCellSize);
	}*/

	void DisableRenderBoundingBox()
	{
		AddAABBLines(BoundingBoxLines.GetDisabledBoundingBox());
	}

	void DisableRenderCone()
	{
		ConeLines.Disable();
		ConeLines.MergeVerticesAt(Vertices, Grid.GetNumVertices() + BoundingBoxLines.GetNumVertices());
		bChangedVertices = true;
	}

	void DisableRenderSphere()
	{
		SphereLines.DisableSphere();
		SphereLines.MergeVerticesAt(Vertices, Grid.GetNumVertices() + BoundingBoxLines.GetNumVertices() + ConeLines.GetNumVertices());
		bChangedVertices = true;
	}

	//void UpdateConstant(FBoundingBox boundingBoxInfo);

	//void Update();

	void Render();

private:
	//void SetIndices();

	/*void AddWorldGridVerticesAndConstData();
	void AddBoundingBoxVertices();*/

	bool bChangedVertices = false;

	TArray<FVector> Vertices; // 그리드 라인 정보 + (offset 후)디폴트 바운딩 박스 라인 정보(minx, miny가 0,0에 정의된 크기가 1인 cube)
	TArray<uint32> Indices; // 월드 그리드는 그냥 정점 순서, 바운딩 박스는 실제 인덱싱

	FEditorPrimitive Primitive;

	UGrid Grid;
	UBoundingBoxLines BoundingBoxLines;
	UConeLines ConeLines;
	USphereLines SphereLines;

	bool bRenderBox;

	int32 CurrentNumVertices = 0;
	int32 CurrentNumIndices = 0;
	inline static const int32 MaxVerticesNum = 16384;
	// 렌더하는 대상마다 인덱싱 방식이 다르기 때문에 정확한 인덱스 상한을 정의할 수 없다.
	// 따라서 Vertex 보다 넉넉히 할당한다.
	inline static const int32 MaxIndicesNum = 32768;

	inline static const int32 ConeBottomLineSegmentNum = 32;
	inline static const int32 SphereLineSegmentNum = 64;
	inline static const int32 GridLineSegmentNumHalf = 250;
};
