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

void APointLightActor::SetLightColor(const FVector4& InColor)
{
	if (PointLightComponent)
	{
		PointLightComponent->SetLightColor(FVector(InColor.X, InColor.Y, InColor.Z));
	}
}

void APointLightActor::SetIntensity(float InIntensity)
{
	if (PointLightComponent)
	{
		PointLightComponent->SetIntensity(InIntensity);
	}
}

void APointLightActor::SetRadius(float InRadius)
{
	if (PointLightComponent)
	{
		PointLightComponent->SetRadius(InRadius);
	}
}

void APointLightActor::SetRadiusFallOff(float InFallOff)
{
	if (PointLightComponent)
	{
		PointLightComponent->SetRadiusFalloff(InFallOff);
	}
}

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
		PointLightComponent->SetIntensity(20.0f);
		PointLightComponent->SetRadius(10.0f);
		PointLightComponent->SetRadiusFalloff(2.0f);
	}

	// 에디터 아이콘 생성
	IconComponent = CreateDefaultSubobject<UBillBoardComponent>(FName("IconComponent"));
	if (IconComponent)
	{
		UTexture* Icon = UAssetManager::GetInstance().CreateTexture("Asset/Icon/PointLight_64x.png");
		IconComponent->SetSprite(Icon);
		IconComponent->SetParentAttachment(GetRootComponent());
	}
}
