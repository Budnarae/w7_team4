#include "pch.h"
#include "Component/Light/Public/DirectionalLightComponent.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent);

UDirectionalLightComponent::UDirectionalLightComponent
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	ULightComponent(InIntensity, InLightColor, InbVisible)
{
}

/*
	Widget Spawnder
*/
UClass* UDirectionalLightComponent::GetSpecificWidgetClass() const
{
	return Super::GetSpecificWidgetClass();
}