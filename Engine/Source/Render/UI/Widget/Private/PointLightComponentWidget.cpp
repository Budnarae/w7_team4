#include "pch.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"

IMPLEMENT_CLASS(UPointLightComponentWidget, ULightComponentBaseWidget)

void UPointLightComponentWidget::RenderWidget()
{
	Super::RenderWidget();

	PointLightComponent = Cast<UPointLightComponent>(LightComponentBase);

	float AttenuationRadius = PointLightComponent->GetAttenuationRadius();
	if (ImGui::DragFloat("AttenuationRadius", &AttenuationRadius, 5.0f, 0.0f, 5000.0f, "%.1f"))
	{
		PointLightComponent->SetAttenuationRadius(AttenuationRadius);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set radius of point light\nUnreal Default: 1000\nTypical Range: 100-5000");
	}

	float LightFalloffExponent = PointLightComponent->GetLightFalloffExponent();
	if (ImGui::DragFloat("LightFalloffExponent", &LightFalloffExponent, 0.01f, 0.0f, 16.0f, "%.2f"))
	{
		PointLightComponent->SetLightFalloffExponent(LightFalloffExponent);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Light attenuation falloff exponent\nUnreal Default: 2.0 (Physically Based)\n1.0 = Linear\n2.0 = Inverse Square\n8.0 = Sharp falloff");
	}
}
