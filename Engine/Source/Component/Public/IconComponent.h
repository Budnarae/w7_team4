#pragma once
#include "Component/Public/BillBoardComponent.h"

UCLASS()
class UIconComponent : public UBillBoardComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UIconComponent, UBillBoardComponent)

public:
	void Initialize(USceneComponent* Parent);
};