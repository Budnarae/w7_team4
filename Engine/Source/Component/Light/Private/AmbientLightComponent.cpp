#include "pch.h"
#include "Component/Light/Public/AmbientLightComponent.h"

#include "Level/Public/Level.h"

IMPLEMENT_CLASS(UAmbientLightComponent, ULightComponent);

UAmbientLightComponent::UAmbientLightComponent
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	ULightComponent(InIntensity, InLightColor, InbVisible)
{
	// 기본값: 0.3 White Ambient Light
	if (InIntensity == 0.0f && InLightColor == FVector::ZeroVector())
	{
		SetIntensity(0.3f);
		SetLightColor(FVector(1.0f, 1.0f, 1.0f));
	}
}

UAmbientLightComponent::~UAmbientLightComponent()
{
	// Level에서 AmbientLight 등록 해제
	if (GWorld && GWorld->GetLevel())
	{
		GWorld->GetLevel()->UnregisterAmbientLight(this);
	}
}

void UAmbientLightComponent::BeginPlay()
{
	Super::BeginPlay();

	// Level에 AmbientLight 등록
	if (GWorld && GWorld->GetLevel())
	{
		GWorld->GetLevel()->RegisterAmbientLight(this);
	}
}

/*
	Widget Spawnder
*/
UClass* UAmbientLightComponent::GetSpecificWidgetClass() const
{
	return Super::GetSpecificWidgetClass();
}

UObject* UAmbientLightComponent::Duplicate()
{
	UAmbientLightComponent* AmbientLightComponent = Cast<UAmbientLightComponent>(Super::Duplicate());
	return AmbientLightComponent;
}
void UAmbientLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}
