#include "pch.h"
#include "Editor/Public/BatchLines.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Physics/Public/AABB.h"
#include "Physics/Public/OBB.h"

UBatchLines::UBatchLines() : Grid(), BoundingBoxLines(), ConeLines()
{
	Vertices.resize(MaxVerticesNum);
	Indices.resize(MaxIndicesNum);

	Primitive.NumVertices = static_cast<uint32>(Vertices.size());
	Primitive.NumIndices = static_cast<uint32>(Indices.size());
	Primitive.IndexBuffer = FRenderResourceFactory::CreateIndexBuffer(Indices.data(), Primitive.NumIndices * sizeof(uint32), true);
	//Primitive.Color = FVector4(1, 1, 1, 0.2f);
	Primitive.Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	Primitive.Vertexbuffer = FRenderResourceFactory::CreateVertexBuffer(
		Vertices.data(), Primitive.NumVertices * sizeof(FVector), true);
	Primitive.Location = FVector(0, 0, 0);
	Primitive.Rotation = FVector(0, 0, 0);
	Primitive.Scale = FVector(1, 1, 1);
	Primitive.VertexShader = UAssetManager::GetInstance().GetVertexShader(EShaderType::BatchLine);
	Primitive.InputLayout = UAssetManager::GetInstance().GetIputLayout(EShaderType::BatchLine);
	Primitive.PixelShader = UAssetManager::GetInstance().GetPixelShader(EShaderType::BatchLine);
}

UBatchLines::~UBatchLines()
{
	SafeRelease(Primitive.Vertexbuffer);
	Primitive.InputLayout->Release();
	Primitive.VertexShader->Release();
	Primitive.IndexBuffer->Release();
	Primitive.PixelShader->Release();
}

void UBatchLines::AddLines(const TArray<FVector>& InVertices, const TArray<uint32>& InIndices)
{
	if (CurrentNumVertices + InVertices.size() >= MaxVerticesNum ||
		CurrentNumIndices + InIndices.size() >= MaxIndicesNum)
	{
		UE_LOG("Warning : BatchLine이 렌더할 수 있는 Line 개수 상한을 초과했습니다.");
		return;
	}

	for (uint32 i = 0; i < InVertices.size(); i++)
	{
		Vertices[CurrentNumVertices + i] = InVertices[i];
	}
	for (uint32 i = 0; i < InIndices.size(); i++)
	{
		Indices[CurrentNumIndices + i] = CurrentNumVertices + InIndices[i];
	}

	CurrentNumVertices += InVertices.size();
	CurrentNumIndices += InIndices.size();

	bChangedVertices = true;
}

void UBatchLines::AddGridLines(const float NewCellSize)
{
	float LineLength = NewCellSize * static_cast<float>(GridLineSegmentNumHalf) / 2.f;

	TArray<FVector> GridVertices;
	uint32 NumVertices = GridLineSegmentNumHalf * 4;
	GridVertices.resize(NumVertices);

	uint32 vertexIndex = 0;
	// z축 라인 업데이트
	for (int32 LineCount = -GridLineSegmentNumHalf / 2; LineCount < GridLineSegmentNumHalf / 2; ++LineCount)
	{
		if (LineCount == 0)
		{
			GridVertices[vertexIndex++] = { static_cast<float>(LineCount) * NewCellSize, -LineLength, 0.0f };
			GridVertices[vertexIndex++] = { static_cast<float>(LineCount) * NewCellSize, 0.f, 0.f };
		}
		else
		{
			GridVertices[vertexIndex++] = { static_cast<float>(LineCount) * NewCellSize, -LineLength, 0.0f };
			GridVertices[vertexIndex++] = { static_cast<float>(LineCount) * NewCellSize, LineLength, 0.0f };
		}
	}

	// x축 라인 업데이트
	for (int32 LineCount = -GridLineSegmentNumHalf / 2; LineCount < GridLineSegmentNumHalf / 2; ++LineCount)
	{
		if (LineCount == 0)
		{
			GridVertices[vertexIndex++] = { -LineLength, static_cast<float>(LineCount) * NewCellSize, 0.0f };
			GridVertices[vertexIndex++] = { 0.f, static_cast<float>(LineCount) * NewCellSize, 0.0f };
		}
		else
		{
			GridVertices[vertexIndex++] = { -LineLength, static_cast<float>(LineCount) * NewCellSize, 0.0f };
			GridVertices[vertexIndex++] = { LineLength, static_cast<float>(LineCount) * NewCellSize, 0.0f };
		}
	}

	// Grid 인덱스 생성 (LineList이므로 순차적으로)
	TArray<uint32> GridIndices;
	for (uint32 i = 0; i < NumVertices; ++i)
	{
		GridIndices.push_back(i);
	}

	AddLines(GridVertices, GridIndices);
}

void UBatchLines::AddAABBLines(const IBoundingVolume* NewBoundingVolume)
{
	TArray<FVector> BoxVertices;
	BoxVertices.resize(8);

	switch (NewBoundingVolume->GetType())
	{
	case EBoundingVolumeType::AABB:
	{
		const FAABB* AABB = static_cast<const FAABB*>(NewBoundingVolume);

		float MinX = AABB->Min.X, MinY = AABB->Min.Y, MinZ = AABB->Min.Z;
		float MaxX = AABB->Max.X, MaxY = AABB->Max.Y, MaxZ = AABB->Max.Z;

		BoxVertices[0] = { MinX, MinY, MinZ }; // Front-Bottom-Left
		BoxVertices[1] = { MaxX, MinY, MinZ }; // Front-Bottom-Right
		BoxVertices[2] = { MaxX, MaxY, MinZ }; // Front-Top-Right
		BoxVertices[3] = { MinX, MaxY, MinZ }; // Front-Top-Left
		BoxVertices[4] = { MinX, MinY, MaxZ }; // Back-Bottom-Left
		BoxVertices[5] = { MaxX, MinY, MaxZ }; // Back-Bottom-Right
		BoxVertices[6] = { MaxX, MaxY, MaxZ }; // Back-Top-Right
		BoxVertices[7] = { MinX, MaxY, MaxZ }; // Back-Top-Left
		break;
	}
	case EBoundingVolumeType::OBB:
	{
		const FOBB* OBB = static_cast<const FOBB*>(NewBoundingVolume);
		const FVector& Extents = OBB->Extents;

		FMatrix OBBToWorld = OBB->ScaleRotation;
		OBBToWorld *= FMatrix::TranslationMatrix(OBB->Center);

		FVector LocalCorners[8] =
		{
			FVector(-Extents.X, -Extents.Y, -Extents.Z), // 0: FBL
			FVector(+Extents.X, -Extents.Y, -Extents.Z), // 1: FBR
			FVector(+Extents.X, +Extents.Y, -Extents.Z), // 2: FTR
			FVector(-Extents.X, +Extents.Y, -Extents.Z), // 3: FTL

			FVector(-Extents.X, -Extents.Y, +Extents.Z), // 4: BBL
			FVector(+Extents.X, -Extents.Y, +Extents.Z), // 5: BBR
			FVector(+Extents.X, +Extents.Y, +Extents.Z), // 6: BTR
			FVector(-Extents.X, +Extents.Y, +Extents.Z)  // 7: BTL
		};

		for (uint32 Idx = 0; Idx < 8; ++Idx)
		{
			FVector WorldCorner = OBBToWorld.TransformPosition(LocalCorners[Idx]);

			BoxVertices[Idx] = { WorldCorner.X, WorldCorner.Y, WorldCorner.Z };
		}
		break;
	}
	default:
		break;
	}

	// Bounding Box 라인 인덱스 (LineList)
	TArray<uint32> BoxIndices = {
		// 앞면
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		// 뒷면
		4, 5,
		5, 6,
		6, 7,
		7, 4,

		// 옆면 연결
		0, 4,
		1, 5,
		2, 6,
		3, 7
	};

	AddLines(BoxVertices, BoxIndices);
}

void UBatchLines::AddConeLines(
	const FVector& Apex,
	const FVector& Direction,
	const FVector& UpVector,
	float Angle,
	const FVector& ConeSize
)
{
	TArray<FVector> ConeVertices;
	ConeVertices.resize(ConeBottomLineSegmentNum + 1);
	// 첫 번째 정점: Cone의 꼭지점 (Apex) - 광원 위치
	ConeVertices[0] = Apex;

	// DecalBoxSize로부터 깊이와 반지름 추출
	const float Depth = ConeSize.X;
	const float RadiusY = ConeSize.Y * 0.5f; // DecalComponent Y축 반지름
	const float RadiusZ = ConeSize.Z * 0.5f; // DecalComponent Z축 반지름

	// Angle에 따른 실제 투사 반지름 계산
	const float HalfAngleRad = (Angle * PI / 180.0f) * 0.5f;
	const float NormalizedRadius = tanf(HalfAngleRad);
	const float CalculatedRadius = Depth * NormalizedRadius;

	// DecalBox를 넘지 않도록 제한
	float ActualRadiusY = std::min(CalculatedRadius, RadiusY);
	float ActualRadiusZ = std::min(CalculatedRadius, RadiusZ);

	// Direction 벡터로 로컬 좌표계 구성
	FVector Forward = Direction;
	Forward.Normalize();

	FVector Up = UpVector;
	Up.Normalize();

	// Right 벡터 계산 (Forward x Up)
	FVector Right = Forward.Cross(Up);
	Right.Normalize();

	// Up 벡터 재계산 (Right x Forward) -> Gram-Schmidt 직교화
	Up = Right.Cross(Forward);
	Up.Normalize();

	// 바닥면의 중심 (Apex에서 Direction 방향으로 Depth만큼)
	FVector BottomCenter = Apex + Forward * Depth;

	// 바닥면의 타원 위의 점들 계산
	const float AngleStep = (2.0f * PI) / static_cast<float>(ConeBottomLineSegmentNum);

	for (uint32 i = 0; i < ConeBottomLineSegmentNum; ++i)
	{
		float CurrentAngle = static_cast<float>(i) * AngleStep;

		// 로컬 좌표계에서 타원 위의 점 계산
		// DecalComponent의 Y축이 부모의 Y축, Z축이 부모의 X축과 대응됨
		// ConeLine의 Up/Right는 부모 월드 기준이므로, RadiusY/Z를 적절히 매핑
		float LocalX = ActualRadiusZ * cosf(CurrentAngle); // 월드 X/Z 평면 회전 -> Right 벡터 방향
		float LocalY = ActualRadiusY * sinf(CurrentAngle); // 월드 Y축 -> Up 벡터 방향

		// 로컬 좌표를 월드 좌표로 변환
		FVector WorldPos = BottomCenter + Right * LocalX + Up * LocalY;

		ConeVertices[1 + i] = WorldPos;
	}

	TArray<uint32> ConeIndices;

	// Apex(0)에서 바닥면 각 정점으로 연결하는 선들
	for (uint32 i = 0; i < ConeBottomLineSegmentNum; ++i)
	{
		ConeIndices.push_back(0);        // Apex
		ConeIndices.push_back(1 + i);    // 바닥면 정점
	}

	// 바닥면의 원을 구성하는 선들
	for (uint32 i = 0; i < ConeBottomLineSegmentNum; ++i)
	{
		ConeIndices.push_back(1 + i);                    // 현재 정점
		ConeIndices.push_back(1 + ((i + 1) % ConeBottomLineSegmentNum)); // 다음 정점 (마지막은 첫 번째로)
	}

	AddLines(ConeVertices, ConeIndices);
}

void UBatchLines::AddSphereLines(const FVector& CenterPosition, float Radius)
{
	TArray<FVector> SphereVertices;
	SphereVertices.resize(SphereLineSegmentNum * 3); // 각 축마다 SphereLineSegmentNum 개의 점

	// 3개의 축(XY, XZ, YZ)에 대한 원을 그려 구를 표현합니다.
	for (int32 i = 0; i < SphereLineSegmentNum; ++i)
	{
		const float Angle = static_cast<float>(i) / SphereLineSegmentNum * 2.0f * PI;
		const float Sin = sinf(Angle);
		const float Cos = cosf(Angle);

		// XY 평면의 원
		FVector Point_XY = CenterPosition + FVector(Radius * Cos, Radius * Sin, 0.f);
		SphereVertices[i * 3] = Point_XY;

		// XZ 평면의 원
		FVector Point_XZ = CenterPosition + FVector(Radius * Cos, 0.f, Radius * Sin);
		SphereVertices[i * 3 + 1] = Point_XZ;

		// YZ 평면의 원
		FVector Point_YZ = CenterPosition + FVector(0.f, Radius * Cos, Radius * Sin);
		SphereVertices[i * 3 + 2] = Point_YZ;
	}

	TArray<uint32> SphereIndices;

	// 올바른 인덱스 설정
	for (uint32 i = 0; i < SphereLineSegmentNum; ++i)
	{
		uint32 nextI = (i + 1) % SphereLineSegmentNum;

		// XY 평면 원 (0, 3, 6, 9, ... 연결)
		SphereIndices.push_back(i * 3);           // 현재 XY점
		SphereIndices.push_back(nextI * 3);       // 다음 XY점

		// XZ 평면 원 (1, 4, 7, 10, ... 연결)  
		SphereIndices.push_back(i * 3 + 1);       // 현재 XZ점
		SphereIndices.push_back(nextI * 3 + 1);   // 다음 XZ점

		// YZ 평면 원 (2, 5, 8, 11, ... 연결)
		SphereIndices.push_back(i * 3 + 2);       // 현재 YZ점
		SphereIndices.push_back(nextI * 3 + 2);   // 다음 YZ점
	}

	AddLines(SphereVertices, SphereIndices);
}

/*
void UBatchLines::AddGridLines(const float newCellSize)
{
	if (newCellSize == Grid.GetCellSize())
	{
		return;
	}
	Grid.UpdateVerticesBy(newCellSize);
	Grid.MergeVerticesAt(Vertices, 0);
	bChangedVertices = true;
}
*/

/*
void UBatchLines::AddAABBLines(const IBoundingVolume* NewBoundingVolume)
{
	BoundingBoxLines.UpdateVertices(NewBoundingVolume);
	BoundingBoxLines.MergeVerticesAt(Vertices, Grid.GetNumVertices());
	bChangedVertices = true;
}
*/

/*
void UBatchLines::AddConeLines(const FVector& Apex, const FVector& Direction, const FVector& UpVector,
                                     float Angle, const FVector& DecalBoxSize)
{
	ConeLines.UpdateVertices(Apex, Direction, UpVector, Angle, DecalBoxSize);
	ConeLines.MergeVerticesAt(Vertices, Grid.GetNumVertices() + BoundingBoxLines.GetNumVertices());
	bChangedVertices = true;
}
*/

/*
void UBatchLines::UpdateSphereVertices(const FVector& CenterPosition, float Radius)
{
	SphereLines.UpdateVertices(CenterPosition, Radius);
	SphereLines.MergeVerticesAt(SphereVertices, Grid.GetNumVertices() + BoundingBoxLines.GetNumVertices() + ConeLines.GetNumVertices());
	bChangedVertices = true;
}
*/

void UBatchLines::UpdateVertexBuffer()
{
	if (bChangedVertices)
	{
		FRenderResourceFactory::UpdateVertexBufferData(Primitive.Vertexbuffer, Vertices);
		FRenderResourceFactory::UpdateIndexBufferData(Primitive.IndexBuffer, Indices);
	}
	bChangedVertices = false;
}

void UBatchLines::ResetLines()
{
	// 다음 프레임을 위한 초기화
	CurrentNumVertices = 0;
	CurrentNumIndices = 0;
}

void UBatchLines::Render()
{
	URenderer& Renderer = URenderer::GetInstance();

	// 렌더링할 정점/인덱스 개수를 Primitive에 업데이트
	Primitive.NumVertices = CurrentNumVertices;
	Primitive.NumIndices = CurrentNumIndices;

	// to do: 아래 함수를 batch에 맞게 수정해야 함.
	Renderer.RenderEditorPrimitive(Primitive, Primitive.RenderState, sizeof(FVector), sizeof(uint32));
}

/*
void UBatchLines::SetIndices()
{
	const uint32 numGridVertices = Grid.GetNumVertices();

	// 기존 그리드 라인 인덱스
	for (uint32 index = 0; index < numGridVertices; ++index)
	{
		Indices.push_back(index);
	}

	// Bounding Box 라인 인덱스 (LineList)
	uint32 boundingBoxLineIdx[] = {
		// 앞면
		0, 1,
		1, 2,
		2, 3,
		3, 0,

		// 뒷면
		4, 5,
		5, 6,
		6, 7,
		7, 4,

		// 옆면 연결
		0, 4,
		1, 5,
		2, 6,
		3, 7
	};

	// numGridVertices 이후에 추가된 8개의 꼭짓점에 맞춰 오프셋 적용
	for (uint32 i = 0; i < std::size(boundingBoxLineIdx); ++i)
	{
		Indices.push_back(numGridVertices + boundingBoxLineIdx[i]);
	}

	// Cone 라인 인덱스 (LineList)
	const uint32 coneBaseOffset = numGridVertices + BoundingBoxLines.GetNumVertices();
	const uint32 ConeSegmentNum = ConeLines.GetNumSegments();

	// Apex(0)에서 바닥면 각 정점으로 연결하는 선들
	for (uint32 i = 0; i < ConeSegmentNum; ++i)
	{
		Indices.push_back(coneBaseOffset + 0);        // Apex
		Indices.push_back(coneBaseOffset + 1 + i);    // 바닥면 정점
	}

	// 바닥면의 원을 구성하는 선들
	for (uint32 i = 0; i < ConeSegmentNum; ++i)
	{
		Indices.push_back(coneBaseOffset + 1 + i);                    // 현재 정점
		Indices.push_back(coneBaseOffset + 1 + ((i + 1) % ConeSegmentNum)); // 다음 정점 (마지막은 첫 번째로)
	}

	// Sphere 라인 인덱스
	const uint32 SphereBaseOffset = coneBaseOffset + ConeLines.GetNumVertices();
	// 만약 다음 도형을 넘긴다면 원 3개를 렌더하므로 Offset에 SphereSegmentNum * 3의 값을 적용해야 한다.
	const uint32 SphereSegmentNum = USphereLines::GetSegments();

	// 올바른 인덱스 설정
	for (uint32 i = 0; i < SphereSegmentNum; ++i)
	{
		uint32 nextI = (i + 1) % SphereSegmentNum;

		// XY 평면 원 (0, 3, 6, 9, ... 연결)
		Indices.push_back(SphereBaseOffset + i * 3);           // 현재 XY점
		Indices.push_back(SphereBaseOffset + nextI * 3);       // 다음 XY점

		// XZ 평면 원 (1, 4, 7, 10, ... 연결)  
		Indices.push_back(SphereBaseOffset + i * 3 + 1);       // 현재 XZ점
		Indices.push_back(SphereBaseOffset + nextI * 3 + 1);   // 다음 XZ점

		// YZ 평면 원 (2, 5, 8, 11, ... 연결)
		Indices.push_back(SphereBaseOffset + i * 3 + 2);       // 현재 YZ점
		Indices.push_back(SphereBaseOffset + nextI * 3 + 2);   // 다음 YZ점
	}
}
*/