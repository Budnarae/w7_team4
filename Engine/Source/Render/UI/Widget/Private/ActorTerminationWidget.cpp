﻿#include "pch.h"
#include "Render/UI/Widget/Public/ActorTerminationWidget.h"

#include "Level/Public/Level.h"
#include "Manager/Input/Public/InputManager.h"
#include "Manager/UI/Public/UIManager.h"
#include "Render/UI/Window/Public/UIWindow.h"

UActorTerminationWidget::UActorTerminationWidget()
	: UWidget("Actor Termination Widget")
	  , ActorDetailWidget(nullptr)
{
}

UActorTerminationWidget::UActorTerminationWidget(UActorDetailWidget* InActorDetailWidget)
	: UWidget("Actor Termination Widget")
      , ActorDetailWidget(InActorDetailWidget)
{
}

UActorTerminationWidget::~UActorTerminationWidget() = default;

void UActorTerminationWidget::Initialize()
{
	// Do Nothing Here
}

void UActorTerminationWidget::Update()
{
	// Do Nothing Here
}

void UActorTerminationWidget::RenderWidget()
{
    auto& InputManager = UInputManager::GetInstance();
    if (!InputManager.IsKeyPressed(EKeyInput::Delete))
    {
        return;
    }
    // 현재 포커스된 윈도우 확인
    UUIWindow* Focused = UUIManager::GetInstance().GetFocusedWindow();
    if (!Focused)
    {
        return;
    }

    const FString FocusedTitle = Focused->GetWindowTitle().ToString();

    AActor* SelectedActor = GEditor->GetEditorModule()->GetSelectedActor();
    if (!SelectedActor)
    {
        return;
    }

    // Details에 포커스 → 컴포넌트 삭제 시도
    if (FocusedTitle == "Details")
    {
        if (ActorDetailWidget)
        {
			// 삭제 불가능 컴포넌트인지 확인 후 삭제
            UActorComponent* Comp = ActorDetailWidget->GetSelectedComponent();
            if (Comp)
            {
                if (Comp->GetClass()->MetaEquals("bIsRemovable", "false"))
                {
                    UE_LOG_WARNING("ActorTerminationWidget: 선택된 컴포넌트는 삭제할 수 없습니다: %s",
                                   Comp->GetName().ToString().data());
                    return;
				}
                DeleteSelectedComponent(SelectedActor, Comp);
            }
        }

        // 컴포넌트 선택이 없으면 아무 것도 하지 않음(원하시면 액터 삭제로 폴백 가능)
        // DeleteSelectedActor(SelectedActor);
        // return;
    }

    // 기타 윈도우 포커스면 아무것도 하지 않음
}

/**
 * @brief Selected Actor 삭제 함수
 */
void UActorTerminationWidget::DeleteSelectedActor(AActor* InSelectedActor)
{
	UE_LOG("ActorTerminationWidget: 삭제를 위한 Actor Marking 시작");
	if (!InSelectedActor)
	{
		UE_LOG("ActorTerminationWidget: 삭제를 위한 Actor가 선택되지 않았습니다");
		return;
	}

	ULevel* CurrentLevel = GWorld->GetLevel();

	if (!CurrentLevel)
	{
		UE_LOG_ERROR("ActorTerminationWidget: No Current Level To Delete Actor From");
		return;
	}

	UE_LOG_INFO("ActorTerminationWidget: 선택된 Actor를 삭제를 위해 마킹 처리: %s",
	       InSelectedActor->GetName() == FName::GetNone() ? "UnNamed" : InSelectedActor->GetName().ToString().data());

	// 지연 삭제를 사용하여 안전하게 다음 틱에서 삭제
	GWorld->DestroyActor(InSelectedActor);
}

void UActorTerminationWidget::DeleteSelectedComponent(AActor* InSelectedActor, UActorComponent* InSelectedComponent)
{
    // 먼저 에디터 선택 해제 → Gizmo도 SelectComponent(nullptr)에서 ClearTarget 호출됨
    if (GEditor->GetEditorModule()->GetSelectedComponent() == InSelectedComponent)
    {
        GEditor->GetEditorModule()->SelectComponent(nullptr);
    }

    bool bSuccess = InSelectedActor->RemoveComponent(InSelectedComponent);

    if (bSuccess && ActorDetailWidget)
    {
        ActorDetailWidget->SetSelectedComponent(nullptr);
    }
}