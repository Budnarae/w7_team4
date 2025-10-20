#include "pch.h"
#include "Actor/Public/PointLightActor.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(APointLightActor, AActor)

APointLightActor::APointLightActor()
{
	bCanEverTick = true;
	bTickInEditor = true;
}

APointLightActor::~APointLightActor() = default;

UClass* APointLightActor::GetDefaultRootComponent()
{
	return UPointLightComponent::StaticClass();
}

void APointLightActor::InitializeComponents()
{
	Super::InitializeComponents();

	// PointLightComponent는 루트로 자동 생성됨
	PointLightComponent = Cast<UPointLightComponent>(GetRootComponent());
	if (PointLightComponent)
	{
		PointLightComponent->SetWorldLocation(GetActorLocation());

		// 기본 라이트 설정: 따뜻한 주황색
		PointLightComponent->SetLightColor(FVector(1.0f, 0.6f, 0.2f));
		PointLightComponent->SetIntensity(3.0f);
		PointLightComponent->SetAttenuationRadius(10.0f);
		PointLightComponent->SetLightFalloffExponent(2.0f);
	}
}
