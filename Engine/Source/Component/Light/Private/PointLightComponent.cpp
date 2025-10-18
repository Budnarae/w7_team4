#include "pch.h"
#include "Component/Light/Public/PointLightComponent.h"

#include "Level/Public/Level.h"
#include "Render/UI/Widget/Public/PointLightComponentWidget.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UPointLightComponent, ULightComponent)

UPointLightComponent::UPointLightComponent()
{
	// 정적 라이트라면 Tick 불필요
	bCanEverTick = false;
}

UPointLightComponent::~UPointLightComponent()
{
	// Level에서 등록 해제
	if (bHasBegunPlay && GWorld && GWorld->GetLevel())
	{
		GWorld->GetLevel()->UnregisterPointLight(this);
	}
}

void UPointLightComponent::BeginPlay()
{
	Super::BeginPlay();
	bHasBegunPlay = true;

	UE_LOG("PointLightComponent: BeginPlay: Owner: %s, Intensity: %.1f, Radius: %.1f",
		GetOwner() ? GetOwner()->GetName().ToString().data() : "nullptr", Intensity, GetAttenuationRadius());

	// Level에 PointLight 등록
	if (GWorld && GWorld->GetLevel())
	{
		GWorld->GetLevel()->RegisterPointLight(this);
	}
}

void UPointLightComponent::TickComponent()
{
	Super::TickComponent();
}

UClass* UPointLightComponent::GetSpecificWidgetClass() const
{
	return UPointLightComponentWidget::StaticClass();
}

void UPointLightComponent::Serialize(bool bInIsLoading, JSON& InOutHandle)
{
	ULightComponent::Serialize(bInIsLoading, InOutHandle);

	if (bInIsLoading)
	{
		// bool 변수 로드
		FJsonSerializer::ReadFloat(InOutHandle, "AttenuationRadius", AttenuationRadius, 0.0f);
		FJsonSerializer::ReadFloat(InOutHandle, "LightFalloffExponent", LightFalloffExponent, 0.0f);
	}
	else
	{
		// bool 변수 저장
		InOutHandle["AttenuationRadius"] = AttenuationRadius;
		InOutHandle["LightFalloffExponent"] = LightFalloffExponent;
	}
}

UObject* UPointLightComponent::Duplicate()
{
	UPointLightComponent* PointLightComp = Cast<UPointLightComponent>(Super::Duplicate());
	PointLightComp->AttenuationRadius = AttenuationRadius;
	PointLightComp->LightFalloffExponent = LightFalloffExponent;

	return PointLightComp;
}

void UPointLightComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}
