#include "pch.h"
#include "Component/Light/Public/AmbientLightComponent.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent);

UAmbientLightComponent::UAmbientLightComponent
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	ULightComponent(InIntensity, InLightColor, InbVisible)
{ }

/*
	Widget Spawnder
*/
UClass* UAmbientLightComponent::GetSpecificWidgetClass() const
{
	return Super::GetSpecificWidgetClass();
}