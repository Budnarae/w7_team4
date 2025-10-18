#pragma once
#include "Component/Public/BillBoardComponent.h"

UCLASS()
class UIconComponent : public UBillBoardComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UIconComponent, UBillBoardComponent)

public:
	void Initialize(USceneComponent* Parent);

	FVector GetIconColor() const;
	float GetIconIntensity() const;


	UObject* Duplicate() override;
	void DuplicateSubObjects(UObject* DuplicatedObject) override;
};