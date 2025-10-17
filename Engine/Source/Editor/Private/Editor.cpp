﻿#include "pch.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Camera.h"
#include "Editor/Public/Viewport.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/UI/Widget/Public/FPSWidget.h"
#include "Render/UI/Widget/Public/SceneHierarchyWidget.h"
#include "Render/UI/Widget/Public/SplitterDebugWidget.h"
#include "Render/UI/Widget/Public/CameraControlWidget.h"
#include "Render/UI/Widget/Public/ViewportMenuBarWidget.h"

#include "Manager/UI/Public/UIManager.h"
#include "Manager/Input/Public/InputManager.h"
#include "Manager/Config/Public/ConfigManager.h"
#include "Manager/Time/Public/TimeManager.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Level/Public/Level.h"
#include "Global/Quaternion.h"
#include "Utility/Public/ScopeCycleCounter.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Component/Public/DecalComponent.h"
#include "ImGui/imgui.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Public/SemiLightComponent.h"
#include "Component/Public/FireBallComponent.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Component/Light/Public/SpotLightComponent.h"

UEditor::UEditor()
{
	const TArray<float>& SplitterRatio = UConfigManager::GetInstance().GetSplitterRatio();
	RootSplitter.SetRatio(SplitterRatio[0]);
	LeftSplitter.SetRatio(SplitterRatio[1]);
	RightSplitter.SetRatio(SplitterRatio[2]);

	auto& UIManager = UUIManager::GetInstance();

	if (auto* FPSWidget = reinterpret_cast<UFPSWidget*>(UIManager.FindWidget("FPS Widget")))
	{
		FPSWidget->SetBatchLine(&BatchLines);
	}

	// Splitter UI에게 Splitter 정보를 전달합니다.
	if (auto* SplitterWidget = reinterpret_cast<USplitterDebugWidget*>(UIManager.FindWidget("Splitter Widget")))
	{
		SplitterWidget->SetSplitters(&RootSplitter, &LeftSplitter, &RightSplitter);
	}

	if (auto* ViewportWidget = reinterpret_cast<UViewportMenuBarWidget*>(UIManager.FindWidget("ViewportMenuBar Widget")))
	{
		ViewportWidget->SetEdtior(this);
	}

	InitializeLayout();
}

UEditor::~UEditor()
{
	UConfigManager::GetInstance().SetSplitterRatio(RootSplitter.GetRatio(), LeftSplitter.GetRatio(), RightSplitter.GetRatio());
	SafeDelete(DraggedSplitter);
	SafeDelete(InteractionViewport);
}

void UEditor::Update()
{
	URenderer& Renderer = URenderer::GetInstance();
	FViewport* Viewport = Renderer.GetViewportClient();

	// 1. 마우스 위치를 기반으로 활성 뷰포트를 결정합니다.
	Viewport->UpdateActiveViewportClient(UInputManager::GetInstance().GetMousePosition());

	// 2. 활성 뷰포트의 카메라의 제어만 업데이트합니다.
	if (UCamera* ActiveCamera = Viewport->GetActiveCamera())
	{
		// 만약 이동량이 있고, 직교 카메라라면 ViewportClient에 알립니다.
		const FVector MovementDelta = ActiveCamera->UpdateInput();
		if (MovementDelta.LengthSquared() > 0.f && ActiveCamera->GetCameraType() == ECameraType::ECT_Orthographic)
		{
			Viewport->UpdateOrthoFocusPointByDelta(MovementDelta);
		}
	}

	BatchLines.ResetLines();  // 이전 프레임 데이터 초기화
	UpdateBatchLines();
	BatchLines.UpdateVertexBuffer();

	ProcessMouseInput();

	// SceneBVH 디버그 데이터 업데이트
	if (ULevel* CurrentLevel = GWorld->GetLevel())
	{
		CurrentLevel->RenderSceneBVHDebug();
	}

	UpdateLayout();
}

void UEditor::RenderEditor(UCamera* InCamera)
{
	RenderDebugPrimitives(InCamera);
	RenderGizmo(InCamera);
}

void UEditor::RenderDebugPrimitives(UCamera* InCamera)
{
	if (GEditor->IsPIESessionActive())
	{
		return;
	}

	BatchLines.Render();
	Axis.Render();

	// SceneBVH 디버그 렌더링
	RenderSceneBVH();
}

void UEditor::RenderGizmo(UCamera* InCamera)
{
	if (GEditor->IsPIESessionActive()) { return; }

	if (InCamera)
	{
		Gizmo.RenderGizmo(Cast<USceneComponent>(GetSelectedComponent()), InCamera);
	}
}

void UEditor::SetSingleViewportLayout(int InActiveIndex)
{
	if (ViewportLayoutState == EViewportLayoutState::Animating) return;

	if (ViewportLayoutState == EViewportLayoutState::Multi)
	{
		SavedRootRatio = RootSplitter.GetRatio();
		SavedLeftRatio = LeftSplitter.GetRatio();
		SavedRightRatio = RightSplitter.GetRatio();
	}

	SourceRootRatio = RootSplitter.GetRatio();
	SourceLeftRatio = LeftSplitter.GetRatio();
	SourceRightRatio = RightSplitter.GetRatio();

	TargetRootRatio = SourceRootRatio;
	TargetLeftRatio = SourceLeftRatio;
	TargetRightRatio = SourceRightRatio;

	switch (InActiveIndex)
	{
	case 0: // 좌상단
		TargetRootRatio = 1.0f;
		TargetLeftRatio = 1.0f;
		break;
	case 1: // 좌하단
		TargetRootRatio = 1.0f;
		TargetLeftRatio = 0.0f;
		break;
	case 2: // 우상단
		TargetRootRatio = 0.0f;
		TargetRightRatio = 1.0f;
		break;
	case 3: // 우하단
		TargetRootRatio = 0.0f;
		TargetRightRatio = 0.0f;
		break;
	default:
		RestoreMultiViewportLayout();
		return;
	}

	ViewportLayoutState = EViewportLayoutState::Animating;
	TargetViewportLayoutState = EViewportLayoutState::Single;
	AnimationStartTime = UTimeManager::GetInstance().GetGameTime();
}

/**
 * @brief 저장된 비율을 사용하여 4분할 뷰포트 레이아웃으로 복원합니다.
 */
void UEditor::RestoreMultiViewportLayout()
{
	if (ViewportLayoutState == EViewportLayoutState::Animating) return;

	SourceRootRatio = RootSplitter.GetRatio();
	SourceLeftRatio = LeftSplitter.GetRatio();
	SourceRightRatio = RightSplitter.GetRatio();

	TargetRootRatio = SavedRootRatio;
	TargetLeftRatio = SavedLeftRatio;
	TargetRightRatio = SavedRightRatio;

	ViewportLayoutState = EViewportLayoutState::Animating;
	TargetViewportLayoutState = EViewportLayoutState::Multi;
	AnimationStartTime = UTimeManager::GetInstance().GetGameTime();
}

void UEditor::InitializeLayout()
{
	// 1. 루트 스플리터의 자식으로 2개의 수평 스플리터를 '주소'로 연결합니다.
	RootSplitter.SetChildren(&LeftSplitter, &RightSplitter);
	RootSplitter.SetRatio(0);

	// 2. 각 수평 스플리터의 자식으로 뷰포트 윈도우들을 '주소'로 연결합니다.
	LeftSplitter.SetChildren(&ViewportWindows[0], &ViewportWindows[1]);
	LeftSplitter.SetRatio(0);
	RightSplitter.SetChildren(&ViewportWindows[2], &ViewportWindows[3]);
	RightSplitter.SetRatio(0);

	// 3. 초기 레이아웃 계산
	const D3D11_VIEWPORT& ViewportInfo = URenderer::GetInstance().GetDeviceResources()->GetViewportInfo();
	FRect FullScreenRect = { ViewportInfo.TopLeftX, ViewportInfo.TopLeftY, ViewportInfo.Width, ViewportInfo.Height };
	RootSplitter.Resize(FullScreenRect);
}

void UEditor::UpdateBatchLines()
{
	// 매 프레임 Grid 라인 추가
	BatchLines.AddGridLines(BatchLines.GetCellSize());

	if (UActorComponent* Component = GetSelectedComponent())
	{
		uint64 ShowFlags = GWorld->GetLevel()->GetShowFlags();
		if ((ShowFlags & EEngineShowFlags::SF_Primitives) && (ShowFlags & EEngineShowFlags::SF_Bounds))
		{
			if (UPrimitiveComponent* PrimitiveComponent = Cast<UPrimitiveComponent>(Component))
			{
				if (PrimitiveComponent->IsA(UFireBallComponent::StaticClass()))
				{
					return;
				}
				if (PrimitiveComponent->GetBoundingBox()->GetType() == EBoundingVolumeType::AABB)
				{
					FVector WorldMin, WorldMax; PrimitiveComponent->GetWorldAABB(WorldMin, WorldMax);
					FAABB AABB(WorldMin, WorldMax);
					BatchLines.AddAABBLines(&AABB);
				}
				else
				{
					BatchLines.AddAABBLines(PrimitiveComponent->GetBoundingBox());
				}
				return;
			}
			else if (UDecalComponent* DecalComponent = Cast<UDecalComponent>(Component))
			{
				// 데칼도 바운딩 갱신 후 라인 업데이트
				BatchLines.AddAABBLines(DecalComponent->GetBoundingBox());
				return;
			}
			else if (USemiLightComponent* SemiLightComponent = Cast<USemiLightComponent>(Component))
			{
				// SemiLight의 Cone 와이어프레임 표시
				const FVector Apex = SemiLightComponent->GetWorldLocation();
				const float Angle = SemiLightComponent->GetSpotAngle();
				const FVector DecalBoxSize = SemiLightComponent->GetDecalBoxSize();

				// DecalComponent의 WorldTransform 행렬에서 직접 축 벡터 추출
				UDecalComponent* DecalComponent = SemiLightComponent->GetDecalComponent();
				if (DecalComponent)
				{
					const FMatrix& DecalWorldMatrix = DecalComponent->GetWorldTransformMatrix();

					// Row-major 행렬: Data[row][column]
					// X축 = Data[0][0~2], Y축 = Data[1][0~2], Z축 = Data[2][0~2]
					FVector Direction = FVector(DecalWorldMatrix.Data[0][0],
					                            DecalWorldMatrix.Data[0][1],
					                            DecalWorldMatrix.Data[0][2]);
					FVector UpVector = FVector(DecalWorldMatrix.Data[1][0],
					                          DecalWorldMatrix.Data[1][1],
					                          DecalWorldMatrix.Data[1][2]);

					Direction.Normalize();
					UpVector.Normalize();

					BatchLines.AddConeLines(Apex, Direction, UpVector, Angle, DecalBoxSize);
				}

				return;
			}
			else if (UDirectionalLightComponent* DirectionalLightComponent = Cast<UDirectionalLightComponent>(Component))
			{

			}
			else if (UPointLightComponent* PointLightComponent = Cast<UPointLightComponent>(Component))
			{
				const FVector Center = PointLightComponent->GetWorldLocation();
				const float Radius = PointLightComponent->GetAttenuationRadius();

				BatchLines.AddSphereLines(Center, Radius);

				return;
			}
			else if (USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(Component))
			{
				const FVector Center = SpotLightComponent->GetWorldLocation();
				const float Radius = SpotLightComponent->GetAttenuationRadius();

				// Cone visualization
				const FMatrix& WorldMatrix = SpotLightComponent->GetWorldTransformMatrix();

				// WorldTransform 행렬에서 축 벡터 추출
				// X축 = Forward (Direction), Z축 = Up
				FVector Direction = FVector(WorldMatrix.Data[0][0],
				                           WorldMatrix.Data[0][1],
				                           WorldMatrix.Data[0][2]);
				FVector UpVector = FVector(WorldMatrix.Data[2][0],
				                          WorldMatrix.Data[2][1],
				                          WorldMatrix.Data[2][2]);

				Direction.Normalize();
				UpVector.Normalize();

				// Cone 각도는 반각 기준이므로 전체 각도는 2배
				const float InnerAngle = SpotLightComponent->GetInnerConeAngle() * 2.0f;
				const float OuterAngle = SpotLightComponent->GetOuterConeAngle() * 2.0f;

				// Radius는 Cone의 빗면 길이 (UE에서 이렇게 쓰는 듯)
				// Cone 높이(Depth) = Radius * cos(HalfAngle)
				const float InnerHalfAngleRad = FVector::GetDegreeToRadian(InnerAngle * 0.5f);
				const float OuterHalfAngleRad = FVector::GetDegreeToRadian(OuterAngle * 0.5f);
				const float InnerDepth = Radius * cosf(InnerHalfAngleRad);
				const float OuterDepth = Radius * cosf(OuterHalfAngleRad);

				BatchLines.AddConeLines
				(
					Center,
					Direction,
					UpVector,
					InnerAngle,
					FVector(InnerDepth, InnerDepth, InnerDepth)
				);

				BatchLines.AddConeLines
				(
					Center,
					Direction,
					UpVector,
					OuterAngle,
					FVector(OuterDepth, OuterDepth, OuterDepth)
				);

				return;
			}
		}
	}
}

void UEditor::UpdateLayout()
{
	URenderer& Renderer = URenderer::GetInstance();
	UInputManager& Input = UInputManager::GetInstance();
	const FPoint MousePosition = { Input.GetMousePosition().X, Input.GetMousePosition().Y };
	bool bIsHoveredOnSplitter = false;

	// 뷰포트를 전환 중이라면 애니메이션을 적용합니다.
	if (ViewportLayoutState == EViewportLayoutState::Animating)
	{
		float ElapsedTime = UTimeManager::GetInstance().GetGameTime() - AnimationStartTime;
		float Alpha = clamp(ElapsedTime / AnimationDuration, 0.0f, 1.0f);

		float NewRootRatio = Lerp(SourceRootRatio, TargetRootRatio, Alpha);
		float NewLeftRatio = Lerp(SourceLeftRatio, TargetLeftRatio, Alpha);
		float NewRightRatio = Lerp(SourceRightRatio, TargetRightRatio, Alpha);

		RootSplitter.SetRatio(NewRootRatio);
		LeftSplitter.SetRatio(NewLeftRatio);
		RightSplitter.SetRatio(NewRightRatio);

		if (Alpha >= 1.0f)
		{
			ViewportLayoutState = TargetViewportLayoutState;
		}
	}

	// 1. 드래그 상태가 아니라면 커서의 상태를 감지합니다.
	if (DraggedSplitter == nullptr)
	{
		if (LeftSplitter.IsHovered(MousePosition) || RightSplitter.IsHovered(MousePosition))
		{
			bIsHoveredOnSplitter = true;
		}
		else if (RootSplitter.IsHovered(MousePosition))
		{
			bIsHoveredOnSplitter = true;
		}
	}

	// 2. 스플리터 위에 커서가 있으며 클릭을 한다면, 드래그 상태로 활성화합니다.
	if (UInputManager::GetInstance().IsKeyPressed(EKeyInput::MouseLeft) && bIsHoveredOnSplitter)
	{
		// 호버 상태에 따라 드래그할 스플리터를 결정합니다.
		if (LeftSplitter.IsHovered(MousePosition)) { DraggedSplitter = &LeftSplitter; }				// 좌상, 좌하
		else if (RightSplitter.IsHovered(MousePosition)) { DraggedSplitter = &RightSplitter; }		// 우상, 우하
		else if (RootSplitter.IsHovered(MousePosition)) { DraggedSplitter = &RootSplitter; }
	}

	// 3. 드래그 상태라면 스플리터 기능을 이행합니다.
	if (DraggedSplitter)
	{
		const ImGuiViewport* Viewport = ImGui::GetMainViewport();
		FRect WorkableRect = { Viewport->WorkPos.x, Viewport->WorkPos.y, Viewport->WorkSize.x, Viewport->WorkSize.y };

		FRect ParentRect;

		if (DraggedSplitter == &RootSplitter)
		{
			ParentRect = WorkableRect;
		}
		else
		{
			if (DraggedSplitter == &LeftSplitter)
			{
				ParentRect.Left = WorkableRect.Left;
				ParentRect.Top = WorkableRect.Top;
				ParentRect.Width = WorkableRect.Width * RootSplitter.GetRatio();
				ParentRect.Height = WorkableRect.Height;
			}
			else if (DraggedSplitter == &RightSplitter)
			{
				ParentRect.Left = WorkableRect.Left + WorkableRect.Width * RootSplitter.GetRatio();
				ParentRect.Top = WorkableRect.Top;
				ParentRect.Width = WorkableRect.Width * (1.0f - RootSplitter.GetRatio());
				ParentRect.Height = WorkableRect.Height;
			}
		}

		// 마우스 위치를 부모 영역에 대한 비율(0.0 ~ 1.0)로 변환합니다.
		float NewRatio = 0.5f;
		if (dynamic_cast<SSplitterV*>(DraggedSplitter)) // 수직 스플리터
		{
			if (ParentRect.Width > 0)
			{
				NewRatio = (MousePosition.X - ParentRect.Left) / ParentRect.Width;
			}
		}
		else // 수평 스플리터
		{
			if (ParentRect.Height > 0)
			{
				NewRatio = (MousePosition.Y - ParentRect.Top) / ParentRect.Height;
			}
		}

		// 계산된 비율을 스플리터에 적용합니다.
		DraggedSplitter->SetRatio(NewRatio);
	}

	// 4. 매 프레임 현재 비율에 맞게 전체 레이아웃 크기를 다시 계산하고, 그 결과를 실제 FViewport에 반영합니다.
	const ImGuiViewport* Viewport = ImGui::GetMainViewport(); // 사용자에게만 보이는 영역의 정보를 가져옵니다. 
	FRect WorkableRect = { Viewport->WorkPos.x, Viewport->WorkPos.y, Viewport->WorkSize.x, Viewport->WorkSize.y };
	RootSplitter.Resize(WorkableRect);

	if (FViewport* ViewportClient = URenderer::GetInstance().GetViewportClient())
	{
		auto& Viewports = ViewportClient->GetViewports();
		for (int i = 0; i < 4; ++i)
		{
			if (i < Viewports.size())
			{
				const FRect& Rect = ViewportWindows[i].Rect;
				Viewports[i].SetViewportInfo({ Rect.Left, Rect.Top, Rect.Width, Rect.Height, 0.0f, 1.0f });
			}
		}
	}

	// 마우스 클릭 해제를 하면 드래그 비활성화
	if (UInputManager::GetInstance().IsKeyReleased(EKeyInput::MouseLeft)) { DraggedSplitter = nullptr; }
}

void UEditor::ProcessMouseInput()
{
	// 선택된 뷰포트의 정보들을 가져옵니다.
	FViewport* ViewportClient = URenderer::GetInstance().GetViewportClient();
	FViewportClient* CurrentViewport = nullptr;
	UCamera* CurrentCamera = nullptr;

	// 이미 선택된 뷰포트 영역이 존재한다면 선택된 뷰포트 처리를 진행합니다.
	if (InteractionViewport) { CurrentViewport = InteractionViewport; }
	// 선택된 뷰포트 영역이 존재하지 않는다면 현재 마우스 위치의 뷰포트를 선택합니다.
	else { CurrentViewport = ViewportClient->GetActiveViewportClient(); }

	// 처리할 영역이 존재하지 않으면 진행을 중단합니다.
	if (CurrentViewport == nullptr) { return; }

	CurrentCamera = &CurrentViewport->Camera;

	AActor* ActorPicked = GetSelectedActor();

	if (ActorPicked)
	{
		// 피킹 전 현재 카메라에 맞는 기즈모 스케일 업데이트
		Gizmo.UpdateScale(CurrentCamera);
		// 빌보드 갱신
		PickedBillboard = ActorPicked->GetUUIDTextComponent();
	}
	else
	{
		PickedBillboard = nullptr;
	}

	const UInputManager& InputManager = UInputManager::GetInstance();
	const FVector& MousePos = InputManager.GetMousePosition();
	const D3D11_VIEWPORT& ViewportInfo = CurrentViewport->GetViewportInfo();

	const float NdcX = ((MousePos.X - ViewportInfo.TopLeftX) / ViewportInfo.Width) * 2.0f - 1.0f;
	const float NdcY = -(((MousePos.Y - ViewportInfo.TopLeftY) / ViewportInfo.Height) * 2.0f - 1.0f);

	FRay WorldRay = CurrentCamera->ConvertToWorldRay(NdcX, NdcY);

	static EGizmoDirection PreviousGizmoDirection = EGizmoDirection::None;
	FVector CollisionPoint;
	float ActorDistance = -1;

	if (InputManager.IsKeyPressed(EKeyInput::Tab))
	{
		Gizmo.IsWorldMode() ? Gizmo.SetLocal() : Gizmo.SetWorld();
	}
	if (InputManager.IsKeyPressed(EKeyInput::Space))
	{
		Gizmo.ChangeGizmoMode();
	}
	if (InputManager.IsKeyReleased(EKeyInput::MouseLeft))
	{
		// 기즈모 드래그가 끝났을 때 BVH 업데이트 (자식 컴포넌트 포함)
		if (Gizmo.IsDragging() && Gizmo.GetSelectedComponent())
		{
			if (USceneComponent* SceneComp = Cast<USceneComponent>(Gizmo.GetSelectedComponent()))
			{
				ULevel* Level = GWorld->GetLevel();
				if (Level)
				{
					Level->UpdateSceneBVHComponent(SceneComp);
				}
			}
		}

		Gizmo.EndDrag();
		// 드래그가 끝나면 선택된 뷰포트를 비활성화 합니다.
		InteractionViewport = nullptr;
	}
	// Gizmo 타깃 유효성 2차 체크 (삭제/분리로 인한 댕글링 방지)
	if (Gizmo.HasComponent())
	{
		USceneComponent* TC = Gizmo.GetSelectedComponent();
		if (!TC || !TC->GetOwner())
		{
			Gizmo.ClearTarget();
		}
		else
		{
			auto& Owned = TC->GetOwner()->GetOwnedComponents();
			if (std::find(Owned.begin(), Owned.end(), TC) == Owned.end())
			{
				Gizmo.ClearTarget();
			}
		}
	}
	if (Gizmo.IsDragging() && Gizmo.GetSelectedComponent())
	{
		switch (Gizmo.GetGizmoMode())
		{
		case EGizmoMode::Translate:
		{
			FVector GizmoDragLocation = GetGizmoDragLocation(CurrentCamera, WorldRay);
			Gizmo.SetLocation(GizmoDragLocation);
			break;
		}
		case EGizmoMode::Rotate:
		{
			FVector GizmoDragRotation = GetGizmoDragRotation(CurrentCamera, WorldRay);
			Gizmo.SetComponentRotation(GizmoDragRotation);
			break;
		}
		case EGizmoMode::Scale:
		{
			FVector GizmoDragScale = GetGizmoDragScale(CurrentCamera, WorldRay);
			Gizmo.SetComponentScale(GizmoDragScale);
		}
		}
	}
	else
	{
		if (GetSelectedActor() && Gizmo.HasComponent())
		{
			ObjectPicker.PickGizmo(CurrentCamera, WorldRay, Gizmo, CollisionPoint);
		}
		else
		{
			Gizmo.SetGizmoDirection(EGizmoDirection::None);
		}

		if (!ImGui::GetIO().WantCaptureMouse && InputManager.IsKeyPressed(EKeyInput::MouseLeft))
		{
			if (GWorld->GetLevel()->GetShowFlags() & EEngineShowFlags::SF_Primitives)
			{
				TArray<UPrimitiveComponent*> Candidate;

				ULevel* CurrentLevel = GWorld->GetLevel();
				ObjectPicker.FindCandidateFromOctree(CurrentLevel->GetStaticOctree(), WorldRay, Candidate);

				TArray<UPrimitiveComponent*>& DynamicCandidates = CurrentLevel->GetDynamicPrimitives();
				if (!DynamicCandidates.empty())
				{
					Candidate.insert(Candidate.end(), DynamicCandidates.begin(), DynamicCandidates.end());
				}
				
				FScopeCycleCounter PickCounter;
				UPrimitiveComponent* PrimitiveCollided = ObjectPicker.PickPrimitive(CurrentCamera, WorldRay, Candidate, &ActorDistance);
				ActorPicked = PrimitiveCollided ? PrimitiveCollided->GetOwner() : nullptr;
				float ElapsedMs = static_cast<float>(PickCounter.Finish()); // 피킹 시간 측정 종료
				UStatOverlay::GetInstance().RecordPickingStats(ElapsedMs);
			}
		}

		if (Gizmo.GetGizmoDirection() == EGizmoDirection::None)
		{
			SelectActor(ActorPicked);
			if (PreviousGizmoDirection != EGizmoDirection::None)
			{
				Gizmo.OnMouseRelease(PreviousGizmoDirection);
			}
		}
		else
		{
			PreviousGizmoDirection = Gizmo.GetGizmoDirection();
			if (InputManager.IsKeyPressed(EKeyInput::MouseLeft))
			{
				Gizmo.OnMouseDragStart(CollisionPoint);
				// 드래그가 활성화하면 뷰포트를 고정합니다.
				InteractionViewport = CurrentViewport;
			}
			else
			{
				Gizmo.OnMouseHovering();
			}
		}
	}
}

TArray<UPrimitiveComponent*> UEditor::FindCandidatePrimitives(ULevel* InLevel)
{
	TArray<UPrimitiveComponent*> Candidate;
	for (AActor* Actor : InLevel->GetActors())
	{
		for (auto& ActorComponent : Actor->GetOwnedComponents())
		{
			if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(ActorComponent))
			{
				Candidate.push_back(Primitive);
			}
		}
	}
	return Candidate;
}

FVector UEditor::GetGizmoDragLocation(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin{ Gizmo.GetGizmoLocation() };
	FVector GizmoAxis = Gizmo.GetGizmoAxis();

	if (!Gizmo.IsWorldMode())
	{
		FVector4 GizmoAxis4{ GizmoAxis.X, GizmoAxis.Y, GizmoAxis.Z, 0.0f };
		FVector RadRotation = FVector::GetDegreeToRadian(Gizmo.GetComponentRotation());
		GizmoAxis = GizmoAxis4 * FMatrix::RotationMatrix(RadRotation);
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, InActiveCamera->CalculatePlaneNormal(GizmoAxis).Cross(GizmoAxis), MouseWorld))
	{
		FVector MouseDistance = MouseWorld - Gizmo.GetDragStartMouseLocation();
		return Gizmo.GetDragStartActorLocation() + GizmoAxis * MouseDistance.Dot(GizmoAxis);
	}
	return Gizmo.GetGizmoLocation();
}

FVector UEditor::GetGizmoDragRotation(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin{ Gizmo.GetGizmoLocation() };
	FVector GizmoAxisLocal = Gizmo.GetGizmoAxis();  // 로컬 축 (쿼터니언용)
	FVector GizmoAxisWorld = GizmoAxisLocal;         // 월드 축 (평면 계산용)

	if (!Gizmo.IsWorldMode())
	{
		// Local 모드: 평면 계산용으로만 world space로 변환
		FVector4 GizmoAxis4{ GizmoAxisLocal.X, GizmoAxisLocal.Y, GizmoAxisLocal.Z, 0.0f };
		FVector RadRotation = FVector::GetDegreeToRadian(Gizmo.GetDragStartActorRotation());
		GizmoAxisWorld = GizmoAxis4 * FMatrix::RotationMatrix(RadRotation);
	}

	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, GizmoAxisWorld, MouseWorld))
	{
		FVector PlaneOriginToMouse = MouseWorld - PlaneOrigin;
		FVector PlaneOriginToMouseStart = Gizmo.GetDragStartMouseLocation() - PlaneOrigin;
		PlaneOriginToMouse.Normalize();
		PlaneOriginToMouseStart.Normalize();
		float DotResult = (PlaneOriginToMouseStart).Dot(PlaneOriginToMouse);
		float Angle = acosf(std::max(-1.0f, std::min(1.0f, DotResult)));
		if ((PlaneOriginToMouse.Cross(PlaneOriginToMouseStart)).Dot(GizmoAxisWorld) < 0)
		{
			Angle = -Angle;
		}

		FQuaternion StartRotQuat = FQuaternion::FromEuler(Gizmo.GetDragStartActorRotation());
		// Local 모드: 로컬 축 사용, World 모드: 월드 축 사용
		FQuaternion DeltaRotQuat = FQuaternion::FromAxisAngle(GizmoAxisLocal, Angle);
		if (Gizmo.IsWorldMode())
		{
			return (DeltaRotQuat * StartRotQuat).ToEuler();
		}
		else
		{
			return (StartRotQuat * DeltaRotQuat).ToEuler();
		}
	}
	return Gizmo.GetComponentRotation();
}

FVector UEditor::GetGizmoDragScale(UCamera* InActiveCamera, FRay& WorldRay)
{
	FVector MouseWorld;
	FVector PlaneOrigin = Gizmo.GetGizmoLocation();
	FVector CardinalAxis = Gizmo.GetGizmoAxis();

	FVector4 GizmoAxis4{ CardinalAxis.X, CardinalAxis.Y, CardinalAxis.Z, 0.0f };
	FVector RadRotation = FVector::GetDegreeToRadian(Gizmo.GetComponentRotation());
	FVector GizmoAxis = Gizmo.GetGizmoAxis();
	GizmoAxis = GizmoAxis4 * FMatrix::RotationMatrix(RadRotation);


	FVector PlaneNormal = InActiveCamera->CalculatePlaneNormal(GizmoAxis).Cross(GizmoAxis);
	if (ObjectPicker.IsRayCollideWithPlane(WorldRay, PlaneOrigin, PlaneNormal, MouseWorld))
	{
		FVector PlaneOriginToMouse = MouseWorld - PlaneOrigin;
		FVector PlaneOriginToMouseStart = Gizmo.GetDragStartMouseLocation() - PlaneOrigin;
		float DragStartAxisDistance = PlaneOriginToMouseStart.Dot(GizmoAxis);
		float DragAxisDistance = PlaneOriginToMouse.Dot(GizmoAxis);
		float ScaleFactor = 1.0f;
		if (abs(DragStartAxisDistance) > 0.1f)
		{
			ScaleFactor = DragAxisDistance / DragStartAxisDistance;
		}

		FVector DragStartScale = Gizmo.GetDragStartActorScale();
		if (ScaleFactor > MinScale)
		{
			if (Gizmo.GetSelectedComponent()->IsUniformScale())
			{
				float UniformValue = DragStartScale.Dot(CardinalAxis);
				return FVector(1.0f, 1.0f, 1.0f) * UniformValue * ScaleFactor;
			}
			else
			{
				return DragStartScale + CardinalAxis * (ScaleFactor - 1.0f) * DragStartScale.Dot(CardinalAxis);
			}
		}
		return Gizmo.GetComponentScale();
	}
	return Gizmo.GetComponentScale();
}

void UEditor::ValidateSelectionForCurrentWorld()
{
	if (SelectionWorld && SelectionWorld != GWorld)
	{
		// 현재 월드가 변경되었으므로 기존 선택을 모두 해제
		if (SelectedComponent) { SelectedComponent->OnDeselected(); }
		SelectedComponent = nullptr;
		SelectedActor = nullptr;
		PickedBillboard = nullptr;
		Gizmo.ClearTarget();
		SelectionWorld = nullptr;
	}
}


void UEditor::SelectActor(AActor* InActor)
{
	if (InActor == SelectedActor) return;

	SelectedActor = InActor;
	SelectionWorld = InActor ? GWorld : nullptr;  // 현재 월드를 기록

	if (SelectedActor) 
	{ 
		SelectComponent(InActor->GetRootComponent());
	}
	else 
	{ 
		SelectComponent(nullptr); 
		PickedBillboard = nullptr;
	}

}
AActor* UEditor::GetSelectedActor()
{
	ValidateSelectionForCurrentWorld();
	return SelectedActor;
}

void UEditor::SelectComponent(UActorComponent* InComponent)
{
	if (InComponent == SelectedComponent) return;

	if (SelectedComponent) { SelectedComponent->OnDeselected(); }

	SelectedComponent = InComponent;

	if (!SelectedComponent)
	{
		Gizmo.ClearTarget();
		return;
	}

	// 컴포넌트 선택도 선택 월드 갱신
	SelectionWorld = GWorld;
	SelectedComponent->OnSelected();

}

UActorComponent* UEditor::GetSelectedComponent()
{
	ValidateSelectionForCurrentWorld();
	return SelectedComponent;
}
UUUIDTextComponent* UEditor::GetPickedBillboard() const
{
	return PickedBillboard;
}

void UEditor::RenderSceneBVH()
{
	ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel) return;

	const TArray<FAABB>& Boxes = CurrentLevel->GetCachedDebugBoxes();
	const TArray<FVector4>& Colors = CurrentLevel->GetCachedDebugColors();

	if (Boxes.empty()) return;

	// 각 박스 렌더링
	for (size_t i = 0; i < Boxes.size(); ++i)
	{
		RenderBVHBox(Boxes[i], Colors[i]);
	}
}

void UEditor::RenderBVHBox(const FAABB& Box, const FVector4& Color)
{
	// 8개의 코너 버텍스 생성
	FVector Corners[8];
	Corners[0] = FVector(Box.Min.X, Box.Min.Y, Box.Min.Z);
	Corners[1] = FVector(Box.Max.X, Box.Min.Y, Box.Min.Z);
	Corners[2] = FVector(Box.Max.X, Box.Max.Y, Box.Min.Z);
	Corners[3] = FVector(Box.Min.X, Box.Max.Y, Box.Min.Z);
	Corners[4] = FVector(Box.Min.X, Box.Min.Y, Box.Max.Z);
	Corners[5] = FVector(Box.Max.X, Box.Min.Y, Box.Max.Z);
	Corners[6] = FVector(Box.Max.X, Box.Max.Y, Box.Max.Z);
	Corners[7] = FVector(Box.Min.X, Box.Max.Y, Box.Max.Z);

	// 12개 엣지의 인덱스 (LineList)
	uint32 Indices[] = {
		// 앞면 (Z = Min)
		0, 1,  1, 2,  2, 3,  3, 0,
		// 뒷면 (Z = Max)
		4, 5,  5, 6,  6, 7,  7, 4,
		// 연결 엣지
		0, 4,  1, 5,  2, 6,  3, 7
	};

	// FEditorPrimitive 설정
	FEditorPrimitive LinePrimitive;
	LinePrimitive.NumVertices = 8;
	LinePrimitive.NumIndices = 24;
	LinePrimitive.Topology = D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	LinePrimitive.Color = Color;

	// 버텍스 버퍼 생성
	LinePrimitive.Vertexbuffer = FRenderResourceFactory::CreateVertexBuffer(
		Corners, LinePrimitive.NumVertices * sizeof(FVector), false);

	// 인덱스 버퍼 생성
	LinePrimitive.IndexBuffer = FRenderResourceFactory::CreateIndexBuffer(
		Indices, LinePrimitive.NumIndices * sizeof(uint32));

	// 셰이더 설정
	LinePrimitive.VertexShader = UAssetManager::GetInstance().GetVertexShader(EShaderType::BatchLine);
	LinePrimitive.InputLayout = UAssetManager::GetInstance().GetIputLayout(EShaderType::BatchLine);
	LinePrimitive.PixelShader = UAssetManager::GetInstance().GetPixelShader(EShaderType::BatchLine);

	// 렌더링
	URenderer& Renderer = URenderer::GetInstance();
	Renderer.RenderEditorPrimitive(LinePrimitive, LinePrimitive.RenderState, sizeof(FVector), sizeof(uint32));

	// 리소스 해제
	SafeRelease(LinePrimitive.Vertexbuffer);
	SafeRelease(LinePrimitive.IndexBuffer);
}
