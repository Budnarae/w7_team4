#pragma once

#include "Component/Light/Public/LightComponent.h"

UCLASS()
class UAmbientLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UAmbientLightComponent, ULightComponent)

public:
	UAmbientLightComponent() = default;
	UAmbientLightComponent(float InIntensity, const FVector& InLightColor, bool InbVisible);
	~UAmbientLightComponent() override = default;

	UClass* GetSpecificWidgetClass() const override;
};