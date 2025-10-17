#include "pch.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

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
float UPointLightComponent::GetAttenuationRadius() const
{
	return AttenuationRadius;
}
void UPointLightComponent::SetAttenuationRadius(float InAttenuationRadius)
{
	AttenuationRadius = InAttenuationRadius;
}

float UPointLightComponent::GetLightFalloffExponent() const
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

void UPointLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	// �ҷ�����
	if (bInIsLoading)
	{
		// bool ���� �ε�
		FJsonSerializer::ReadFloat(InOutHandle, "AttenuationRadius", AttenuationRadius, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "LightFalloffExponent", LightFalloffExponent, 0.0f);
	}
	// ����
	else
	{
		// bool ���� ����
		InOutHandle["AttenuationRadius"] = AttenuationRadius;
		InOutHandle["LightFalloffExponent"] = LightFalloffExponent;
	}
}