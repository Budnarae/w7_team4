#include "pch.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Render/UI/Widget/Public/SpotLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

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

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// 불러오기
	if (bInIsLoading)
	{
		// bool 변수 로드
		FJsonSerializer::ReadFloat(InOutHandle, "InnerConeAngle", InnerConeAngle, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "OuterConeAngle", OuterConeAngle, 0.0f);
	}
	// 저장
	else
	{
		// bool 변수 저장
		InOutHandle["InnerConeAngle"] = InnerConeAngle;
		InOutHandle["OuterConeAngle"] = OuterConeAngle;
	}
}