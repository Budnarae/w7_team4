#pragma once

#include "Component/Light/Public/LightComponent.h"

class USpotLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(USpotLightComponent, ULightComponent)

private:
	float InnerConeAngle = 0.0f;
	float OuterConeAngle = 0.0f;
public:
	USpotLightComponent() = default;
	USpotLightComponent(float InIntensity, const FVector& InLightColor, bool InbVisible);
	~USpotLightComponent() override = default;

	/* Getter Setter */
	float GetInnerConeAngle();
	void SetInnerConeAngle(float InInnerConeAngle);

	float GetOuterConeAngle();
	void SetOuterConeAngle(float InOuterConeAngle);

	UClass* GetSpecificWidgetClass() const override;
};