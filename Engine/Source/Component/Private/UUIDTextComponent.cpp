#include "pch.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Editor/Public/Editor.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(UUUIDTextComponent, UTextComponent)

/**
 * @brief Level에서 각 Actor마다 가지고 있는 UUID를 출력해주기 위한 빌보드 클래스
 * Actor has a UBillBoardComponent
 */

UUUIDTextComponent::UUUIDTextComponent() : ZOffset(0.0f) {};

UUUIDTextComponent::~UUUIDTextComponent()
{
}

void UUUIDTextComponent::OnSelected()
{
	SetVisibility(true);
}

void UUUIDTextComponent::OnDeselected()
{
	SetVisibility(false);
}

void UUUIDTextComponent::UpdateRotationMatrix(const FMatrix& InViewMatrix)
{
    const FVector BasePosition = GetOwner()->GetActorLocation();

    // View 행렬의 역행렬에서 회전 부분만 추출 (카메라 방향)
    FVector CameraRight(InViewMatrix.Data[0][0], InViewMatrix.Data[1][0], InViewMatrix.Data[2][0]);
    FVector CameraUp(InViewMatrix.Data[0][1], InViewMatrix.Data[1][1], InViewMatrix.Data[2][1]);
    FVector CameraForward(InViewMatrix.Data[0][2], InViewMatrix.Data[1][2], InViewMatrix.Data[2][2]);

    const FVector FinalPosition = BasePosition + CameraUp * ZOffset;

    const FMatrix Rotation = FMatrix(CameraForward, CameraRight, CameraUp);
    const FMatrix Transition = FMatrix::TranslationMatrix(FinalPosition);
    RTMatrix = Rotation * Transition;
}

void UUUIDTextComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	UTextComponent::Serialize(bInIsLoading, InOutHandle);
	if (bInIsLoading)
	{
		GetOwner()->SetUUIDTextComponent(this);
		SetOffset(5);
	}
}

UClass* UUUIDTextComponent::GetSpecificWidgetClass() const
{
	return nullptr;
}
