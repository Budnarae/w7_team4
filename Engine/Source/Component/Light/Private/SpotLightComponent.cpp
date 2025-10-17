#include "pch.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Render/UI/Widget/Public/SpotLightComponentWidget.h"

IMPLEMENT_CLASS(USpotLightComponent, ULightComponent);

USpotLightComponent::USpotLightComponent
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	ULightComponent(InIntensity, InLightColor, InbVisible)
{
}

/* Getter Setter */
float USpotLightComponent::GetInnerConeAngle() {
	return InnerConeAngle;
}
void USpotLightComponent::SetInnerConeAngle(float InInnerConeAngle)
{
	InnerConeAngle = InInnerConeAngle;
}

float USpotLightComponent::GetOuterConeAngle()
{
	return OuterConeAngle;
}
void USpotLightComponent::SetOuterConeAngle(float InOuterConeAngle)
{
	OuterConeAngle = InOuterConeAngle;
}

UClass* USpotLightComponent::GetSpecificWidgetClass() const
{
	return USpotLightComponentWidget::StaticClass();
}