#pragma once

#include "Component/Light/Public/LightComponentBase.h"
#include "Component/Public/IconComponent.h"

UCLASS()
class ULightComponent : public ULightComponentBase
{
	GENERATED_BODY()
	DECLARE_CLASS(ULightComponent, ULightComponentBase)

public:
	ULightComponent() = default;
	ULightComponent(float InIntensity, const FVector& InLightColor, bool InbVisible);
	~ULightComponent() override = default;


	/*
	Widget Spawnder
*/
	UClass* GetSpecificWidgetClass() const override;
	void BeginPlay() override;
	UObject* Duplicate() override;
	void DuplicateSubObjects(UObject* DuplicatedObject) override;
private:
	UIconComponent* IconComponent = nullptr;
};