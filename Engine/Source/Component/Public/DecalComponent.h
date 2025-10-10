#pragma once
#include "Component/Public/PrimitiveComponent.h"


UCLASS()
class UDecalComponent : public USceneComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UDecalComponent, USceneComponent)

public:
	UDecalComponent();
	~UDecalComponent();

	void SetTexture(class UTexture* InTexture) { DecalTexture = InTexture; }
	class UTexture* GetTexture() const { return DecalTexture; }

	// Primitive���� ���� �������̽� �ּ� ����
	bool IsVisible() const { return bVisible; }
	void SetVisibility(bool bVisibility) { bVisible = bVisibility; }

	// DecalPass�� ���� �ٿ�� ����
	const IBoundingVolume* GetBoundingBox();
protected:
	class UTexture* DecalTexture = nullptr;

	// ���� ���� (Primitive�� �ƴϹǷ� ���� ����)
	IBoundingVolume* BoundingBox = nullptr;
	bool bVisible = true;
};
