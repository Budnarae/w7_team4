#include "pch.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Level/Public/Level.h"
#include "Level/Public/World.h"

IMPLEMENT_CLASS(UDirectionalLightComponent, ULightComponent);

UDirectionalLightComponent::UDirectionalLightComponent
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	ULightComponent(InIntensity, InLightColor, InbVisible)
{
}

UDirectionalLightComponent::~UDirectionalLightComponent()
{
	// Level에서 DirectionalLight 등록 해제
	if (GWorld && GWorld->GetLevel())
	{
		GWorld->GetLevel()->UnregisterDirectionalLight(this);
	}
}

void UDirectionalLightComponent::BeginPlay()
{
	Super::BeginPlay();

	// Level에 DirectionalLight 등록
	if (GWorld && GWorld->GetLevel())
	{
		GWorld->GetLevel()->RegisterDirectionalLight(this);
	}
}

/*
	Widget Spawnder
*/
UClass* UDirectionalLightComponent::GetSpecificWidgetClass() const
{
	return Super::GetSpecificWidgetClass();
}

UObject* UDirectionalLightComponent::Duplicate()
{
	UDirectionalLightComponent* DirectionalLightComponent = Cast<UDirectionalLightComponent>(Super::Duplicate());
	return DirectionalLightComponent;
}

void UDirectionalLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}