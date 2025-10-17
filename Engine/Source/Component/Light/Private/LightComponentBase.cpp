#include "pch.h"
#include "Component/Light/Public/LightComponentBase.h"
#include "Render/UI/Widget/Public/LightComponentBaseWidget.h"
#include "Utility/Public/JsonSerializer.h"

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

void ULightComponentBase::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 불러오기
	if (bInIsLoading)
	{
		 // bool 변수 로드
		FJsonSerializer::ReadFloat(InOutHandle, "Intensity", Intensity, 0.0f);
		FJsonSerializer::ReadVector(InOutHandle, "LightColor", LightColor, FVector::ZeroVector());
		FJsonSerializer::ReadBool(InOutHandle, "bVisible", bVisible, true);
	}
	// 저장
	else
	{
		// bool 변수 저장
		InOutHandle["Intensity"] = Intensity;
		InOutHandle["LightColor"] = FJsonSerializer::VectorToJson(LightColor);
		InOutHandle["bVisible"] = bVisible;
	}
}