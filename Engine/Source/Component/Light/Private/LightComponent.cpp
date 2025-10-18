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

/*
	여기서 IconComponent를 Initialize하는 이유는 BeginPlay()가
	선언될 시점엔 Actor의 등록이 확정되어 있기 떄문이다.
*/
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

UObject* ULightComponent::Duplicate()
{
	// IconComponent는 Actor 등록 때문에 BeginPlay 때 별도로 초기화
	ULightComponent* LightComponent = Cast<ULightComponent>(Super::Duplicate());
	return LightComponent;
}

void ULightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);

	ULightComponent* LightComponent = Cast<ULightComponent>(DuplicatedObject);
	LightComponent->IconComponent = Cast<UIconComponent>(IconComponent->Duplicate());
	LightComponent->IconComponent->Initialize(LightComponent);
}