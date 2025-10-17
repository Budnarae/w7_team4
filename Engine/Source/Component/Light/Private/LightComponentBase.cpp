#include "pch.h"
#include "Component/Light/Public/LightComponentBase.h"
#include "Render/UI/Widget/Public/LightComponentBaseWidget.h"

IMPLEMENT_CLASS(ULightComponentBase, USceneComponent)

ULightComponentBase::ULightComponentBase
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	Intensity(InIntensity),
	LightColor(InLightColor),
	bVisible(InbVisible)
{ }


/* Getter Setter */

float ULightComponentBase::GetIntensity() const
{
	return Intensity;
}
void ULightComponentBase::SetIntensity(float InIntensity)
{
	Intensity = InIntensity;
}

FVector ULightComponentBase::GetLightColor() const
{
	return LightColor;
}
void ULightComponentBase::SetLightColor(const FVector& InLightColor)
{
	LightColor = InLightColor;
}

bool ULightComponentBase::IsVisible() const
{
	return bVisible;
}
void ULightComponentBase::SetVisible(bool InbVisible)
{
	bVisible = InbVisible;
}

/*
	Widget Spawnder
*/
UClass* ULightComponentBase::GetSpecificWidgetClass() const
{
	return ULightComponentBaseWidget::StaticClass();
}