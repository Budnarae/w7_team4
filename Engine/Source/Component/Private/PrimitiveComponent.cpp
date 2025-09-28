#include "pch.h"
#include "Component/Public/PrimitiveComponent.h"

#include "Manager/Asset/Public/AssetManager.h"
#include "Physics/Public/AABB.h"

IMPLEMENT_CLASS(UPrimitiveComponent, USceneComponent)

UPrimitiveComponent::UPrimitiveComponent()
{
	ComponentType = EComponentType::Primitive;
	bCanEverTick = true;
}

void UPrimitiveComponent::TickComponent()
{
	Super::TickComponent();
}

void UPrimitiveComponent::OnSelected()
{
	SetColor({ 1.f, 0.8f, 0.2f, 0.4f });
}

void UPrimitiveComponent::OnDeselected()
{
	SetColor({ 0.f, 0.f, 0.f, 0.f });
}

void USceneComponent::SetUniformScale(bool bIsUniform)
{
	bIsUniformScale = bIsUniform;
}

bool USceneComponent::IsUniformScale() const
{
	return bIsUniformScale;
}

const FVector& USceneComponent::GetRelativeLocation() const
{
	return RelativeLocation;
}
const FVector& USceneComponent::GetRelativeRotation() const
{
	return RelativeRotation;
}
const FVector& USceneComponent::GetRelativeScale3D() const
{
	return RelativeScale3D;
}

const FMatrix& USceneComponent::GetWorldTransformMatrix() const
{
	if (bIsTransformDirty)
	{
		WorldTransformMatrix = FMatrix::GetModelMatrix(RelativeLocation, FVector::GetDegreeToRadian(RelativeRotation), RelativeScale3D);

		if (ParentAttachment)
		{
			WorldTransformMatrix *= ParentAttachment->GetWorldTransformMatrix();
		}

		bIsTransformDirty = false;
	}

	return WorldTransformMatrix;
}

const FMatrix& USceneComponent::GetWorldTransformMatrixInverse() const
{

	if (bIsTransformDirtyInverse)
	{
		WorldTransformMatrixInverse = FMatrix::Identity();

		if (ParentAttachment)
		{
			WorldTransformMatrix *= ParentAttachment->GetWorldTransformMatrixInverse();
		}

		WorldTransformMatrixInverse *= FMatrix::GetModelMatrixInverse(RelativeLocation, FVector::GetDegreeToRadian(RelativeRotation), RelativeScale3D);

		bIsTransformDirtyInverse = false;
	}

	return WorldTransformMatrixInverse;
}

const TArray<FNormalVertex>* UPrimitiveComponent::GetVerticesData() const
{
	return Vertices;
}

const TArray<uint32>* UPrimitiveComponent::GetIndicesData() const
{
	return Indices;
}

ID3D11Buffer* UPrimitiveComponent::GetVertexBuffer() const
{
	return VertexBuffer;
}

ID3D11Buffer* UPrimitiveComponent::GetIndexBuffer() const
{
	return IndexBuffer;
}

const uint32 UPrimitiveComponent::GetNumVertices() const
{
	return NumVertices;
}

const uint32 UPrimitiveComponent::GetNumIndices() const
{
	return NumIndices;
}

void UPrimitiveComponent::SetTopology(D3D11_PRIMITIVE_TOPOLOGY InTopology)
{
	Topology = InTopology;
}

D3D11_PRIMITIVE_TOPOLOGY UPrimitiveComponent::GetTopology() const
{
	return Topology;
}

void UPrimitiveComponent::GetWorldAABB(FVector& OutMin, FVector& OutMax) const
{
	if (!BoundingBox)
	{
		OutMin = FVector(); OutMax = FVector();
		return;
	}

	if (bIsAABBCacheDirty)
	{
		if (BoundingBox->GetType() == EBoundingVolumeType::AABB)
		{
			const FAABB* LocalAABB = static_cast<const FAABB*>(BoundingBox);
			FVector LocalCorners[8] =
			{
				FVector(LocalAABB->Min.X, LocalAABB->Min.Y, LocalAABB->Min.Z), FVector(LocalAABB->Max.X, LocalAABB->Min.Y, LocalAABB->Min.Z),
				FVector(LocalAABB->Min.X, LocalAABB->Max.Y, LocalAABB->Min.Z), FVector(LocalAABB->Max.X, LocalAABB->Max.Y, LocalAABB->Min.Z),
				FVector(LocalAABB->Min.X, LocalAABB->Min.Y, LocalAABB->Max.Z), FVector(LocalAABB->Max.X, LocalAABB->Min.Y, LocalAABB->Max.Z),
				FVector(LocalAABB->Min.X, LocalAABB->Max.Y, LocalAABB->Max.Z), FVector(LocalAABB->Max.X, LocalAABB->Max.Y, LocalAABB->Max.Z)
			};

			const FMatrix& WorldTransform = GetWorldTransformMatrix();
			FVector WorldMin(+FLT_MAX, +FLT_MAX, +FLT_MAX);
			FVector WorldMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

			UE_LOG("Matrix T: %f, %f, %f", WorldTransform.Data[3][0], WorldTransform.Data[3][1], WorldTransform.Data[3][2]);
			for (int32 i = 0; i < 8; i++)
			{
				FVector4 WorldCorner = FVector4(LocalCorners[i].X, LocalCorners[i].Y, LocalCorners[i].Z, 1.0f) * WorldTransform;
				WorldMin.X = min(WorldMin.X, WorldCorner.X);
				WorldMin.Y = min(WorldMin.Y, WorldCorner.Y);
				WorldMin.Z = min(WorldMin.Z, WorldCorner.Z);
				WorldMax.X = max(WorldMax.X, WorldCorner.X);
				WorldMax.Y = max(WorldMax.Y, WorldCorner.Y);
				WorldMax.Z = max(WorldMax.Z, WorldCorner.Z);
			}

			CachedWorldMin = WorldMin;
			CachedWorldMax = WorldMax;
			bIsAABBCacheDirty = false;
		}
	}

	// 캐시된 값 반환
	OutMin = CachedWorldMin;
	OutMax = CachedWorldMax;
}

void UPrimitiveComponent::MarkAsDirty()
{
	bIsAABBCacheDirty = true;
	Super::MarkAsDirty();
}
