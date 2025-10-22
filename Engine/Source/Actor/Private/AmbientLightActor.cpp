#include "pch.h"
#include "Actor/Public/AmbientLightActor.h"
#include "Component/Light/Public/AmbientLightComponent.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(AAmbientLightActor, AActor)

AAmbientLightActor::AAmbientLightActor()
{
	bCanEverTick = true;
	bTickInEditor = true;
}

AAmbientLightActor::~AAmbientLightActor() = default;

UClass* AAmbientLightActor::GetDefaultRootComponent()
{
	return UAmbientLightComponent::StaticClass();
}

void AAmbientLightActor::InitializeComponents()
{
	Super::InitializeComponents();

	// AmbientLightComponent는 루트로 자동 생성됨
	AmbientLightComponent = Cast<UAmbientLightComponent>(GetRootComponent());
	if (AmbientLightComponent)
	{
		AmbientLightComponent->SetWorldLocation(GetActorLocation());

		// 기본 앰비언트 라이트 설정: 은은한 파란빛 (밤하늘 느낌)
		AmbientLightComponent->SetLightColor(FVector(0.5f, 0.6f, 0.8f));
		AmbientLightComponent->SetIntensity(0.3f);
	}
}
