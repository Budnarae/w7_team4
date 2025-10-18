#include "pch.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Render/UI/Widget/Public/SpotLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"
#include "Level/Public/Level.h"
#include "Level/Public/World.h"

IMPLEMENT_CLASS(USpotLightComponent, ULightComponent);

USpotLightComponent::USpotLightComponent()
	: ULightComponent(20.0f, FVector(1.0f, 1.0f, 1.0f), true)
{
	// 정적 라이트라면 Tick 불필요
	bCanEverTick = false;
}

USpotLightComponent::USpotLightComponent
(
	float InIntensity,
	const FVector& InLightColor,
	bool InbVisible
) :
	ULightComponent(InIntensity, InLightColor, InbVisible)
{
	bCanEverTick = false;
}

USpotLightComponent::~USpotLightComponent()
{
	// Level에서 등록 해제
	if (bHasBegunPlay && GWorld && GWorld->GetLevel())
	{
		GWorld->GetLevel()->UnregisterSpotLight(this);
	}
}

void USpotLightComponent::BeginPlay()
{
	Super::BeginPlay();
	bHasBegunPlay = true;

	UE_LOG("SpotLightComponent: BeginPlay: Owner: %s, Intensity: %.1f, Radius: %.1f, InnerAngle: %.1f, OuterAngle: %.1f",
		GetOwner() ? GetOwner()->GetName().ToString().data() : "nullptr",
		GetIntensity(), AttenuationRadius, InnerConeAngle, OuterConeAngle);

	// Level에 SpotLight 등록
	if (!GWorld)
	{
		UE_LOG("SpotLightComponent: BeginPlay: GWorld is nullptr! Cannot register SpotLight.");
		return;
	}

	if (!GWorld->GetLevel())
	{
		UE_LOG("SpotLightComponent: BeginPlay: GWorld->GetLevel() is nullptr! Cannot register SpotLight.");
		return;
	}

	GWorld->GetLevel()->RegisterSpotLight(this);
	UE_LOG("SpotLightComponent: BeginPlay: Successfully registered to Level.");
}

UClass* USpotLightComponent::GetSpecificWidgetClass() const
{
	return USpotLightComponentWidget::StaticClass();
}

void USpotLightComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
	Super::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		UE_LOG("SpotLightComponent: Serialize: LOADING");

		FJsonSerializer::ReadFloat(InOutHandle, "InnerConeAngle", InnerConeAngle, 25.0f, false);
		FJsonSerializer::ReadFloat(InOutHandle, "OuterConeAngle", OuterConeAngle, 45.0f, false);
		FJsonSerializer::ReadFloat(InOutHandle, "AttenuationRadius", AttenuationRadius, 10.0f, false);
		FJsonSerializer::ReadFloat(InOutHandle, "LightFalloffExponent", LightFalloffExponent, 2.0f, false);

		// Reset runtime flag
		bHasBegunPlay = false;

		UE_LOG("SpotLightComponent: Serialize: Loaded: Intensity=%.1f, Radius=%.1f, InnerAngle=%.1f, OuterAngle=%.1f",
			GetIntensity(), AttenuationRadius, InnerConeAngle, OuterConeAngle);
	}
	else
	{
		UE_LOG("SpotLightComponent: Serialize: SAVING: Intensity=%.1f, Radius=%.1f",
			GetIntensity(), AttenuationRadius);

		InOutHandle["InnerConeAngle"] = InnerConeAngle;
		InOutHandle["OuterConeAngle"] = OuterConeAngle;
		InOutHandle["AttenuationRadius"] = AttenuationRadius;
		InOutHandle["LightFalloffExponent"] = LightFalloffExponent;
	}
}

UObject* USpotLightComponent::Duplicate()
{
	USpotLightComponent* SpotLightComponent = Cast<USpotLightComponent>(Super::Duplicate());

	// Copy light properties
	SpotLightComponent->InnerConeAngle = InnerConeAngle;
	SpotLightComponent->OuterConeAngle = OuterConeAngle;
	SpotLightComponent->AttenuationRadius = AttenuationRadius;
	SpotLightComponent->LightFalloffExponent = LightFalloffExponent;

	// Reset runtime flag for PIE
	SpotLightComponent->bHasBegunPlay = false;

	return SpotLightComponent;
}