#pragma once

#include "Component/Light/Public/LightComponent.h"

UCLASS()
class UDirectionalLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UDirectionalLightComponent, ULightComponent)

public:
	UDirectionalLightComponent() = default;
	UDirectionalLightComponent(float InIntensity, const FVector& InLightColor, bool InbVisible);
	~UDirectionalLightComponent() override = default;

	UClass* GetSpecificWidgetClass() const override;
};