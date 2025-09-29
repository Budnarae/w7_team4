#pragma once

#include "Physics/Public/AABB.h"

class UPrimitiveComponent;

constexpr int MAX_PRIMITIVES = 32; 
constexpr int MAX_DEPTH = 5;      

struct FLinearOctreeNode
{
	// �� ��尡 ����ϴ� ������Ƽ����� ���� �ε���
	uint32 StartIndex = 0;
	// �� ��尡 ����ϴ� ������Ƽ����� ����
	uint32 PrimitiveCount = 0;
	// �ڽ� ������ �ε���. -1�̸� ���� ���
	int32 Children[8] = { -1, -1, -1, -1, -1, -1, -1, -1 };
};

class FOctree
{
public:
	FOctree();
	FOctree(const FVector& InPosition, float InSize, int InDepth);
	FOctree(const FAABB& InBoundingBox, int InDepth);
	~FOctree();

	bool Insert(UPrimitiveComponent* InPrimitive);
	bool Remove(UPrimitiveComponent* InPrimitive);
	void Clear();

	void GetAllPrimitives(TArray<UPrimitiveComponent*>& OutPrimitives) const;
	TArray<UPrimitiveComponent*> FindNearestPrimitives(const FVector& FindPos, uint32 MaxPrimitiveCount);

	const FAABB& GetBoundingBox() const { return BoundingBox; }
	bool IsLeafNode() const { return IsLeaf(); }
	const TArray<UPrimitiveComponent*>& GetPrimitives() const { return Primitives; }
	TArray<FOctree*>& GetChildren() { return Children; }

private:
	bool IsLeaf() const { return Children[0] == nullptr; }
	void Subdivide(UPrimitiveComponent* InPrimitive);
	void TryMerge();

	FAABB BoundingBox;
	int Depth;                       
	TArray<UPrimitiveComponent*> Primitives;
	TArray<FOctree*> Children;
};

using FNodeQueue = std::priority_queue<
	std::pair<float, FOctree*>,
	std::vector<std::pair<float, FOctree*>>,
	std::greater<std::pair<float, FOctree*>>
>;

class FLinearOctree
{
public:
	FLinearOctree();
	FLinearOctree(const FVector& InPosition, float InSize, int InMaxDepth = MAX_DEPTH);
	~FLinearOctree();

	// �������̽� ������ ���� Insert �Լ� (���������δ� Rebuild�� ȣ��)
	bool Insert(UPrimitiveComponent* InPrimitive);
	// �������̽� ������ ���� Remove �Լ� (���� ������ �ֹ��� �� �� ����)
	bool Remove(UPrimitiveComponent* InPrimitive);

	// ��� ������Ƽ�긦 �޾� Ʈ���� �����ϴ� �ٽ� �Լ�
	void Build(const TArray<UPrimitiveComponent*>& InPrimitives);

	// ���� �������̽��� ������ ���� �Լ���
	void GetAllPrimitives(TArray<UPrimitiveComponent*>& OutPrimitives) const;
	// FindNearestPrimitives�� ���� ��Ʈ������ ������ �����ϹǷ�, Ư�� ���� ���� ������Ƽ�긦 ã�� �Լ��� ��ü ����
	void FindPrimitivesInBounds(const FAABB& InBounds, TArray<UPrimitiveComponent*>& OutPrimitives) const;

	const FAABB& GetBoundingBox() const { return RootBoundingBox; }

private:
	// ��������� ��带 �����ϴ� �Լ�
	void BuildRecursive(uint32 NodeIndex, uint32 MortonCodeStart, uint32 MortonCodeEnd, int InDepth);
	// Ư�� ���� �� ������Ƽ�긦 ��������� ã�� �Լ�
	void FindPrimitivesInBoundsRecursive(uint32 NodeIndex, const FAABB& InBounds, TArray<UPrimitiveComponent*>& OutPrimitives) const;

	// ��ư �ڵ带 ����ϴ� ���� �Լ�
	uint32 MortonCode3D(const FVector& Point);
	// 10��Ʈ ������ 30��Ʈ�� Ȯ�� (��ư �ڵ� ����)
	uint32 ExpandBits(uint32 v);

private:
	// ������ ��ü ����
	FAABB RootBoundingBox;
	// �ִ� ����
	int MaxDepth;

	// ��� ��带 �����ϴ� �迭 (������ ��� �ε��� ���)
	TArray<FLinearOctreeNode> Nodes;

	// ���� ������Ƽ�� ������ �迭 (�ܺο��� ����)
	TArray<UPrimitiveComponent*> Primitives;
	// ������Ƽ���� ��ư �ڵ�� �ε����� �����ϴ� �迭 (���Ŀ� ���)
	TArray<std::pair<uint32, int>> MortonSortedPrimitives;
};