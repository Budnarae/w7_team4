#include "pch.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Render/UI/Widget/Public/SpotLightComponentWidget.h"

IMPLEMENT_CLASS(USpotLightComponentWidget, ULightComponentBaseWidget)

void USpotLightComponentWidget::RenderWidget()
{
	Super::RenderWidget();

	SpotLightComponent = Cast<USpotLightComponent>(LightComponentBase);

	float InnerConeAngle = SpotLightComponent->GetInnerConeAngle();
	if (ImGui::DragFloat("InnerConeAngle", &InnerConeAngle, 0.05f, -360.0f, 360.0f, "%.1f"))
	{
		SpotLightComponent->SetInnerConeAngle(InnerConeAngle);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set inner cone angle of Spot light");
	}

	float OuterConeAngle = SpotLightComponent->GetOuterConeAngle();
	if (ImGui::DragFloat("OuterConeAngle", &OuterConeAngle, 0.05f, -360.0f, 360.0f, "%.1f"))
	{
		SpotLightComponent->SetOuterConeAngle(OuterConeAngle);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set outer cone angle of Spot light");
	}
}