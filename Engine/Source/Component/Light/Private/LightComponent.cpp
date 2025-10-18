#include "pch.h"
#include "Component/Light/Public/LightComponent.h"
#include "Actor/Public/Actor.h"

IMPLEMENT_CLASS(ULightComponent, ULightComponentBase)

ULightComponent::ULightComponent
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	ULightComponentBase(InIntensity, InLightColor, InbVisible)
{
}

/*
	Widget Spawnder
*/
UClass* ULightComponent::GetSpecificWidgetClass() const
{
	return Super::GetSpecificWidgetClass();
}

void ULightComponent::BeginPlay()
{
	Super::BeginPlay();

	// IconComponent 동적 생성 및 초기화
	if (!IconComponent)
	{
		IconComponent = new UIconComponent();
		IconComponent->Initialize(this);  // Initialize가 알아서 Owner 설정 및 등록
	}
}