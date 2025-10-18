#include "pch.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Render/UI/Widget/Public/SpotLightComponentWidget.h"

IMPLEMENT_CLASS(USpotLightComponentWidget, ULightComponentBaseWidget)

void USpotLightComponentWidget::RenderWidget()
{
	Super::RenderWidget();

	SpotLightComponent = Cast<USpotLightComponent>(LightComponentBase);

	// AttenuationRadius
	float AttenuationRadius = SpotLightComponent->GetAttenuationRadius();
	if (ImGui::DragFloat("AttenuationRadius", &AttenuationRadius, 0.05f, 0.0f, 100.0f, "%.1f"))
	{
		SpotLightComponent->SetAttenuationRadius(AttenuationRadius);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set radius of spot light");
	}

	// LightFalloffExponent
	float LightFalloffExponent = SpotLightComponent->GetLightFalloffExponent();
	if (ImGui::DragFloat("LightFalloffExponent", &LightFalloffExponent, 0.5f, 0.5f, 10.0f, "%.2f"))
	{
		SpotLightComponent->SetLightFalloffExponent(LightFalloffExponent);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Light attenuation falloff exponent\n1.0 = Linear\n2.0 = Physically based (Inverse Square)\n>2.0 = Sharper falloff");
	}

	// OuterConeAngle (InnerConeAngle의 max 값으로 사용하기 위해 우선 설정)
	float OuterConeAngle = SpotLightComponent->GetOuterConeAngle();
	if (ImGui::DragFloat("OuterConeAngle", &OuterConeAngle, 0.5f, 1.0f, 89.0f, "%.1f"))
	{
		SpotLightComponent->SetOuterConeAngle(OuterConeAngle);

		// InnerConeAngle이 OuterConeAngle보다 크면 자동 조정
		if (SpotLightComponent->GetInnerConeAngle() > OuterConeAngle)
		{
			SpotLightComponent->SetInnerConeAngle(OuterConeAngle);
		}
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Outer cone angle (edge of light)\nRange: 1-89 degrees");
	}

	// InnerConeAngle (OuterConeAngle보다 작도록 처리)
	float InnerConeAngle = SpotLightComponent->GetInnerConeAngle();
	float MaxInnerAngle = OuterConeAngle;
	if (ImGui::DragFloat("InnerConeAngle", &InnerConeAngle, 0.5f, 0.0f, MaxInnerAngle, "%.1f"))
	{
		SpotLightComponent->SetInnerConeAngle(InnerConeAngle);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Inner cone angle (fully bright)\nMust be <= OuterConeAngle");
	}
}