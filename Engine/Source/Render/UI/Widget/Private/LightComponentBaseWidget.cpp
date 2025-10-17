#include "pch.h"
#include "Component/Light/Public/LightComponentBase.h"
#include "Render/UI/Widget/Public/LightComponentBaseWidget.h"

#include "Level/Public/Level.h"

IMPLEMENT_CLASS(ULightComponentBaseWidget, UWidget)

void ULightComponentBaseWidget::RenderWidget()
{
	ULevel* CurrentLevel = GWorld->GetLevel();

	if (!CurrentLevel)
	{
		ImGui::TextUnformatted("No Level Loaded");
		return;
	}

	AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
	if (!SelectedActor)
	{
		ImGui::TextUnformatted("No Object Selected");
		return;
	}

	for (UActorComponent* Component : SelectedActor->GetOwnedComponents())
	{
		UActorComponent* SelectedComponent = GEditor->GetEditorModule()->GetSelectedComponent();

		if (SelectedComponent && SelectedComponent->GetOwner() == SelectedActor)
		{
			if (ULightComponentBase* SelectedLightComponentBase = Cast<ULightComponentBase>(SelectedComponent))
			{
				LightComponentBase = SelectedLightComponentBase;
			}
		}

		if (!LightComponentBase)
		{
			ImGui::TextUnformatted("Selected Component isnt's LightComponentBase");
			return;
		}

	}

	if (!LightComponentBase)
	{
		return;
	}

	ImGui::Separator();

	bool bIsVisible = LightComponentBase->IsVisible();
	if (ImGui::Checkbox("Visibility", &bIsVisible))
	{
		LightComponentBase->SetVisible(bIsVisible);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set Visibility of light");
	}

	float Intensity = LightComponentBase->GetIntensity();
	if (ImGui::DragFloat("Intensity", &Intensity, 0.05f, 0.0f, 1.0f, "%.1f"))
	{
		LightComponentBase->SetIntensity(Intensity);
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set Intensity of light");
	}
	
	FVector LightColor = LightComponentBase->GetLightColor();
	float LightRGB[3] = { LightColor.X, LightColor.Y, LightColor.Z };
	if (ImGui::DragFloat3("Light Color", LightRGB, 0.05f, 0.0f, 1.0f, "%.1f"))
	{
		LightComponentBase->SetLightColor(FVector(LightRGB[0], LightRGB[1], LightRGB[2]));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set Color of light");
	}
}
