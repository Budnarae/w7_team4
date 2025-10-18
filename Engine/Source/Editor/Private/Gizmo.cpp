#include "pch.h"
#include "Editor/Public/Gizmo.h"
#include "Editor/Public/Camera.h"

#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Actor/Public/Actor.h"
#include "Global/Quaternion.h"

UGizmo::UGizmo()
{
	UAssetManager& ResourceManager = UAssetManager::GetInstance();
	Primitives.resize(3);
	GizmoColor.resize(3);

	/* *
	* @brief 0: Forward(x), 1: Right(y), 2: Up(z)
	*/
	GizmoColor[0] = FVector4(1, 0, 0, 1);
	GizmoColor[1] = FVector4(0, 1, 0, 1);
	GizmoColor[2] = FVector4(0, 0, 1, 1);

	/* *
	* @brief Translation Setting
	*/
	const float ScaleT = TranslateCollisionConfig.Scale;
	Primitives[0].Vertexbuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Arrow);
	Primitives[0].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Arrow);
	Primitives[0].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[0].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[0].bShouldAlwaysVisible = true;

	/* *
	* @brief Rotation Setting
	*/
	Primitives[1].Vertexbuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::Ring);
	Primitives[1].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::Ring);
	Primitives[1].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[1].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[1].bShouldAlwaysVisible = true;

	/* *
	* @brief Scale Setting
	*/
	Primitives[2].Vertexbuffer = ResourceManager.GetVertexbuffer(EPrimitiveType::CubeArrow);
	Primitives[2].NumVertices = ResourceManager.GetNumVertices(EPrimitiveType::CubeArrow);
	Primitives[2].Topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	Primitives[2].Scale = FVector(ScaleT, ScaleT, ScaleT);
	Primitives[2].bShouldAlwaysVisible = true;

	/* *
	* @brief Render State
	*/
	RenderState.FillMode = EFillMode::Solid;
	RenderState.CullMode = ECullMode::None;
}

UGizmo::~UGizmo() = default;

void UGizmo::UpdateScale(UCamera* InCamera)
{
	if (!TargetComponent || !InCamera ||
		!TargetComponent->GetOwner())
	{
		ClearTarget();
		return;
	}
	{
		auto& Owned = TargetComponent->GetOwner()->GetOwnedComponents();
		if (std::find(Owned.begin(), Owned.end(), TargetComponent) == Owned.end())
		{
			ClearTarget();
			return;
		}
	}

	float Scale;
	if (InCamera->GetCameraType() == ECameraType::ECT_Perspective)
	{
		Scale = CalculateScreenSpaceScale(InCamera, TargetComponent->GetWorldLocation());
	}
	else // Orthographic
	{
		Scale = OrthoScaleFactor;
	}

	TranslateCollisionConfig.Scale = Scale;
	RotateCollisionConfig.Scale = Scale;
}

void UGizmo::RenderGizmo(USceneComponent* SceneComponent, UCamera* InCamera)
{
	TargetComponent = SceneComponent;
	if (!TargetComponent || !InCamera || !TargetComponent->GetOwner())
	{
		ClearTarget();
		return;
	}
	{
		auto& Owned = TargetComponent->GetOwner()->GetOwnedComponents();
		if (std::find(Owned.begin(), Owned.end(), TargetComponent) == Owned.end())
		{
			ClearTarget();
			return;
		}
	}

	float RenderScale;
	if (InCamera->GetCameraType() == ECameraType::ECT_Perspective)
	{
		RenderScale = CalculateScreenSpaceScale(InCamera, TargetComponent->GetWorldLocation());
	}
	else // Orthographic
	{
		RenderScale = OrthoScaleFactor;
	}

	URenderer& Renderer = URenderer::GetInstance();
	const int Mode = static_cast<int>(GizmoMode);
	auto& P = Primitives[Mode];
	P.Location = TargetComponent->GetWorldLocation();

	P.Scale = FVector(RenderScale, RenderScale, RenderScale);

	// 2) 드래그 중에는 나머지 축 유지되는 모드 (회전 후 새로운 로컬 기즈모 보여줌)
	FQuaternion LocalRot;
	if (GizmoMode == EGizmoMode::Rotate && !bIsWorld && bIsDragging)
	{
		LocalRot = FQuaternion::FromEuler(DragStartActorRotation);
	}
	else if (GizmoMode == EGizmoMode::Scale)
	{
		LocalRot = FQuaternion::FromEuler(TargetComponent->GetWorldRotation());
	}
	else
	{
		LocalRot = bIsWorld ? FQuaternion::Identity() : FQuaternion::FromEuler(TargetComponent->GetWorldRotation());
	}

	// X축 (Forward) - 빨간색
	FQuaternion RotX = LocalRot * FQuaternion::Identity();
	P.Rotation = RotX.ToEuler();
	P.Color = ColorFor(EGizmoDirection::Forward);
	Renderer.RenderEditorPrimitive(P, RenderState);

	// Y축 (Right) - 초록색 (Z축 주위로 90도 회전)
	FQuaternion RotY = LocalRot * FQuaternion::FromAxisAngle(FVector::UpVector(), 90.0f * (PI / 180.0f));
	P.Rotation = RotY.ToEuler();
	P.Color = ColorFor(EGizmoDirection::Right);
	Renderer.RenderEditorPrimitive(P, RenderState);

	// Z축 (Up) - 파란색 (Y축 주위로 -90도 회전)
	FQuaternion RotZ = LocalRot * FQuaternion::FromAxisAngle(FVector::RightVector(), -90.0f * (PI / 180.0f));
	P.Rotation = RotZ.ToEuler();
	P.Color = ColorFor(EGizmoDirection::Up);
	Renderer.RenderEditorPrimitive(P, RenderState);
}

void UGizmo::ChangeGizmoMode()
{
	switch (GizmoMode)
	{
	case EGizmoMode::Translate:
		GizmoMode = EGizmoMode::Rotate; break;
	case EGizmoMode::Rotate:
		GizmoMode = EGizmoMode::Scale; break;
	case EGizmoMode::Scale:
		GizmoMode = EGizmoMode::Translate;
	}
}

void UGizmo::SetLocation(const FVector& Location)
{
	TargetComponent->SetWorldLocation(Location);
}

bool UGizmo::IsInRadius(float Radius)
{
	if (Radius >= RotateCollisionConfig.InnerRadius * RotateCollisionConfig.Scale && Radius <= RotateCollisionConfig.OuterRadius * RotateCollisionConfig.Scale)
		return true;
	return false;
}

void UGizmo::OnMouseDragStart(FVector& CollisionPoint)
{
	bIsDragging = true;
	DragStartMouseLocation = CollisionPoint;

	if (TargetComponent)
	{
		DragStartActorLocation = TargetComponent->GetWorldLocation();
		DragStartActorRotation = TargetComponent->GetWorldRotation();
		DragStartActorScale = TargetComponent->GetWorldScale3D();
	}
}

// 하이라이트 색상은 렌더 시점에만 계산 (상태 오염 방지)
FVector4 UGizmo::ColorFor(EGizmoDirection InAxis) const
{
	const int Idx = AxisIndex(InAxis);
	const FVector4& BaseColor = GizmoColor[Idx];
	const bool bIsHighlight = (InAxis == GizmoDirection);

	// 드래깅 중: 주황색
	if (bIsDragging && bIsHighlight)
	{
		return {1.0f, 0.5f, 0.0f, 1.0f}; // 주황색
	}
	// 호버링 중: 노란색
	else if (bIsHighlight)
	{
		return {1.0f, 1.0f, 0.0f, 1.0f}; // 노란색
	}
	// 기본 색상
	else
	{
		return BaseColor;
	}
}

void UGizmo::ClearTarget()
{
	TargetComponent = nullptr;
	bIsDragging = false;
	GizmoDirection = EGizmoDirection::None;
}

// FOV를 고려한 스크린 스페이스 스케일 계산
float UGizmo::CalculateScreenSpaceScale(UCamera* InCamera, const FVector& WorldPosition) const
{
	if (!InCamera)
	{
		return MinScaleFactor * ScaleFactor;
	}

	// 카메라 Forward 방향으로의 깊이 계산 (ViewZ)
	FVector ToGizmo = WorldPosition - InCamera->GetLocation();
	FVector CameraForward = InCamera->GetForward();
	float ViewZ = ToGizmo.Dot(CameraForward);

	// 최소 거리 클램프
	if (ViewZ < MinScaleFactor)
	{
		ViewZ = MinScaleFactor;
	}

	// FOV를 고려한 스케일 계산
	// ProjYY = cot(FOV_Y/2) = 1 / tan(FOV_Y/2)
	float FovYRadians = FVector::GetDegreeToRadian(InCamera->GetFovY());
	float ProjYY = 1.0f / std::tanf(FovYRadians * 0.5f);

	// 화면 공간에서 일정한 크기를 유지하도록 스케일 계산
	// 기본 ScaleFactor를 곱하여 최종 월드 스페이스 스케일 결정
	float ScreenSpaceScale = ViewZ / ProjYY;

	return ScreenSpaceScale * ScaleFactor;
}
