#include "pch.h"
#include "Component/Light/Public/LightComponent.h"

IMPLEMENT_CLASS(ULightComponent, ULightComponentBase)

ULightComponent::ULightComponent
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	ULightComponentBase(InIntensity, InLightColor, InbVisible)
{ }

/*
	Widget Spawnder
*/
UClass* ULightComponent::GetSpecificWidgetClass() const
{
	return Super::GetSpecificWidgetClass();
}