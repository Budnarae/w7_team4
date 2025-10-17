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

	// ColorEdit3 with Picker and Palette support
	if (ImGui::ColorEdit3("Light Color", LightRGB, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_DisplayRGB))
	{
		LightComponentBase->SetLightColor(FVector(LightRGB[0], LightRGB[1], LightRGB[2]));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Set Color of light (Click square for color picker)");
	}

	// Light Color Presets (Palette)
	ImGui::Text("Presets:");
	ImGui::SameLine();

	// Warm Light (Candle/Fire)
	if (ImGui::ColorButton("Warm##preset", ImVec4(1.0f, 0.6f, 0.2f, 1.0f), 0, ImVec2(20, 20)))
	{
		LightComponentBase->SetLightColor(FVector(1.0f, 0.6f, 0.2f));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Warm Light (Fire)");
	}

	ImGui::SameLine();

	// Neutral White
	if (ImGui::ColorButton("White##preset", ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0, ImVec2(20, 20)))
	{
		LightComponentBase->SetLightColor(FVector(1.0f, 1.0f, 1.0f));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Neutral White");
	}

	ImGui::SameLine();

	// Cool White (Daylight)
	if (ImGui::ColorButton("Cool##preset", ImVec4(0.8f, 0.9f, 1.0f, 1.0f), 0, ImVec2(20, 20)))
	{
		LightComponentBase->SetLightColor(FVector(0.8f, 0.9f, 1.0f));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Cool White (Daylight)");
	}

	ImGui::SameLine();

	// Blue (Ice)
	if (ImGui::ColorButton("Blue##preset", ImVec4(0.2f, 0.4f, 1.0f, 1.0f), 0, ImVec2(20, 20)))
	{
		LightComponentBase->SetLightColor(FVector(0.2f, 0.4f, 1.0f));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Blue (Ice/Magic)");
	}

	ImGui::SameLine();

	// Green (Toxic)
	if (ImGui::ColorButton("Green##preset", ImVec4(0.2f, 1.0f, 0.2f, 1.0f), 0, ImVec2(20, 20)))
	{
		LightComponentBase->SetLightColor(FVector(0.2f, 1.0f, 0.2f));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Green (Toxic/Nature)");
	}

	ImGui::SameLine();

	// Red (Danger)
	if (ImGui::ColorButton("Red##preset", ImVec4(1.0f, 0.1f, 0.1f, 1.0f), 0, ImVec2(20, 20)))
	{
		LightComponentBase->SetLightColor(FVector(1.0f, 0.1f, 0.1f));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Red (Danger/Alert)");
	}

	ImGui::SameLine();

	// Purple (Magic)
	if (ImGui::ColorButton("Purple##preset", ImVec4(0.8f, 0.2f, 1.0f, 1.0f), 0, ImVec2(20, 20)))
	{
		LightComponentBase->SetLightColor(FVector(0.8f, 0.2f, 1.0f));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Purple (Magic/Mystery)");
	}

	ImGui::SameLine();

	// Yellow (Electric)
	if (ImGui::ColorButton("Yellow##preset", ImVec4(1.0f, 1.0f, 0.2f, 1.0f), 0, ImVec2(20, 20)))
	{
		LightComponentBase->SetLightColor(FVector(1.0f, 1.0f, 0.2f));
	}
	if (ImGui::IsItemHovered())
	{
		ImGui::SetTooltip("Yellow (Electric/Lightning)");
	}
}
