#include "pch.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent);

UPointLightComponent::UPointLightComponent
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	ULightComponent(InIntensity, InLightColor, InbVisible)
{
}

/* Getter Setter */
float UPointLightComponent::GetAttenuationRadius()
{
	return AttenuationRadius;
}
void UPointLightComponent::SetAttenuationRadius(float InAttenuationRadius)
{
	AttenuationRadius = InAttenuationRadius;
}

float UPointLightComponent::GetLightFalloffExponent()
{
	return LightFalloffExponent;
}
void UPointLightComponent::SetLightFalloffExponent(float InLightFalloffExponent)
{
	LightFalloffExponent = InLightFalloffExponent;
}

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
	return UPointLightComponentWidget::StaticClass();
}
