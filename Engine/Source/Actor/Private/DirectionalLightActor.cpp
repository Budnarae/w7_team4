#include "pch.h"
#include "Actor/Public/DirectionalLightActor.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(ADirectionalLightActor, AActor)

ADirectionalLightActor::ADirectionalLightActor()
{
	bCanEverTick = true;
	bTickInEditor = true;
}

ADirectionalLightActor::~ADirectionalLightActor() = default;

UClass* ADirectionalLightActor::GetDefaultRootComponent()
{
	return UDirectionalLightComponent::StaticClass();
}

void ADirectionalLightActor::InitializeComponents()
{
	Super::InitializeComponents();

	// DirectionalLightComponent는 루트로 자동 생성됨
	DirectionalLightComponent = Cast<UDirectionalLightComponent>(GetRootComponent());
	if (DirectionalLightComponent)
	{
		DirectionalLightComponent->SetWorldLocation(GetActorLocation());

		// 기본 라이트 설정: 따뜻한 햇빛 (연한 노란색)
		DirectionalLightComponent->SetLightColor(FVector(1.0f, 0.95f, 0.8f));
		DirectionalLightComponent->SetIntensity(1.0f);
	}
}
