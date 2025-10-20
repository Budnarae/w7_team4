#include "pch.h"
#include "Actor/Public/SpotLightActor.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(ASpotLightActor, AActor)

ASpotLightActor::ASpotLightActor()
{
	bCanEverTick = true;
	bTickInEditor = true;
}

ASpotLightActor::~ASpotLightActor() = default;

UClass* ASpotLightActor::GetDefaultRootComponent()
{
	return USpotLightComponent::StaticClass();
}

void ASpotLightActor::InitializeComponents()
{
	Super::InitializeComponents();

	// SpotLightComponent는 루트로 자동 생성됨
	if (SpotLightComponent)
	{
		SpotLightComponent->SetWorldLocation(GetActorLocation());

		// 기본 라이트 설정: 따뜻한 주황색
		SpotLightComponent->SetLightColor({1.0f, 0.6f, 0.2f});
		SpotLightComponent->SetIntensity(3.0f);
		SpotLightComponent->SetInnerConeAngle(20.0f);
		SpotLightComponent->SetOuterConeAngle(45.0f);
	}
}
