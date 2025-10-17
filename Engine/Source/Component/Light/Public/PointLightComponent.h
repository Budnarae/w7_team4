#pragma once

#include "Component/Light/Public/LightComponent.h"

class UPointLightComponent : public ULightComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(UPointLightComponent, ULightComponent)

private:
	float AttenuationRadius = 0.0f;
	float LightFalloffExponent = 0.0f;
public:
	UPointLightComponent() = default;
	UPointLightComponent(float InIntensity, const FVector& InLightColor, bool InbVisible);
	~UPointLightComponent() override = default;

	/* Getter Setter */
	float GetAttenuationRadius() const;
	void SetAttenuationRadius(float InAttenuationRadius);

	float GetLightFalloffExponent() const;
	void SetLightFalloffExponent(float InLightFalloffExponent);

	UClass* GetSpecificWidgetClass() const override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
};