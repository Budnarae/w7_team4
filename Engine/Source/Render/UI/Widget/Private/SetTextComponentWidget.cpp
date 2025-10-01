#include "pch.h"
#include "Render/UI/Widget/Public/SetTextComponentWidget.h"

#include "Level/Public/Level.h"
#include "Manager/Level/Public/LevelManager.h"
#include "Actor/Public/CubeActor.h"
#include "Actor/Public/SphereActor.h"
#include "Actor/Public/SquareActor.h"
#include "Actor/Public/TriangleActor.h"
#include "Actor/Public/StaticMeshActor.h"
#include "Actor/Public/BillBoardActor.h"
#include "Actor/Public/TextActor.h"
#include "Component/Public/TextComponent.h"

IMPLEMENT_CLASS(USetTextComponentWidget, UWidget)

USetTextComponentWidget::USetTextComponentWidget()
	: UWidget("Set TextComponent Widget")
{
}

USetTextComponentWidget::~USetTextComponentWidget() = default;

void USetTextComponentWidget::Initialize()
{
	// Do Nothing Here
}

void USetTextComponentWidget::Update()
{
	// �� ������ Level�� ���õ� Actor�� Ȯ���ؼ� ���� �ݿ�
	// TODO(KHJ): ������ ��ġ�� ã�� ��
	ULevelManager& LevelManager = ULevelManager::GetInstance();
	ULevel* CurrentLevel = LevelManager.GetCurrentLevel();

	if (CurrentLevel)
	{
		AActor* NewSelectedActor = CurrentLevel->GetSelectedActor();

		// Update Current Selected Actor
		if (SelectedActor != NewSelectedActor)
		{
			SelectedActor = NewSelectedActor;
			SelectedTextComponent = nullptr;
		}

		// Get Current Selected Actor Information
		if (SelectedActor)
			UpdateTextFromActor();
	}
}

void USetTextComponentWidget::RenderWidget()
{
	if (!SelectedActor)
		return;

	ImGui::Separator();
	ImGui::Text("Type Text");

	ImGui::Spacing();

	// ���۸� ���� �����ؾ� ��
	
	static char buf[256] = "";
	const FString& TextOfComponent = SelectedTextComponent->GetText();
	memcpy(buf, TextOfComponent.c_str(), std::min(sizeof(buf), TextOfComponent.size()));
	const char* TagName = (FString("Text of ") + SelectedActor->GetName().ToString()).c_str();

	if (ImGui::InputText(TagName, buf, IM_ARRAYSIZE(buf)))
	{
		SelectedTextComponent->SetText(FString(buf));
	}
	
	ImGui::Separator();
}

void USetTextComponentWidget::UpdateTextFromActor()
{
	for (const TObjectPtr<UActorComponent>& Comp : SelectedActor->GetOwnedComponents())
	{
		TObjectPtr<UTextComponent> TextComp = Cast<UTextComponent>(Comp);
		if (TextComp)
			SelectedTextComponent = TextComp.Get();
	}
}