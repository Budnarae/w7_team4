#include "pch.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"

IMPLEMENT_CLASS(UPointLightComponentWidget, ULightComponentBaseWidget)

void UPointLightComponentWidget::RenderWidget()
{
	Super::RenderWidget();

	PointLightComponent = Cast<UPointLightComponent>(LightComponentBase);

	float AttenuationRadius = PointLightComponent->GetAttenuationRadius();
	if (ImGui::DragFloat("AttenuationRadius", &AttenuationRadius, 0.05f, 0.0f, 100.0f, "%.1f"))
	{
		PointLightComponent->SetAttenuationRadius(AttenuationRadius);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set radius of point light");
	}

	float LightFalloffExponent = PointLightComponent->GetLightFalloffExponent();
	if (ImGui::DragFloat("LightFalloffExponent", &LightFalloffExponent, 0.05f, 0.0f, 100.0f, "%.1f"))
	{
		PointLightComponent->SetLightFalloffExponent(LightFalloffExponent);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set LightFalloffExponent of point light");
	}
}