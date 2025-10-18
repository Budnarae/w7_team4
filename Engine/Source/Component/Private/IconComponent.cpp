#include "pch.h"
#include "Component/Public/IconComponent.h"
#include "Component/Light/Public/LightComponent.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
#include "Manager/Asset/Public/AssetManager.h"

IMPLEMENT_CLASS(UIconComponent, UBillBoardComponent)
SET_CLASS_META_IN_GLOBAL(UIconComponent, bIsRemovable, false)
SET_CLASS_META_IN_GLOBAL(UIconComponent, bVisibleInComponentTree, false)
SET_CLASS_META_IN_GLOBAL(UIconComponent, bShouldSerialize, false)

void UIconComponent::Initialize(USceneComponent* Parent)
{
	if (!Parent)
	{
		SetSprite(nullptr);
		return;
	}

	SetParentAttachment(Parent);

	// Owner 설정 및 등록 (null 체크)
	AActor* Owner = Parent->GetOwner();
	if (!Owner)
	{
		return;
	}

	SetOwner(Owner);
	Owner->RegisterComponent(this);

	if (UPointLightComponent* PointLight = Cast<UPointLightComponent>(Parent))
	{
		SetSprite(UAssetManager::GetInstance().CreateTexture("Asset/Icon/PointLight_64x.png"));
	}
	else if (USpotLightComponent* SpotLight = Cast<USpotLightComponent>(Parent))
	{
		SetSprite(UAssetManager::GetInstance().CreateTexture("Asset/Icon/SpotLight_64x.png"));
	}
	else if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(Parent))
	{
		SetSprite(UAssetManager::GetInstance().CreateTexture("Asset/Icon/DirectionalLight_64x.png"));
	}
	else
	{
		SetSprite(UAssetManager::GetInstance().CreateTexture("Asset/Icon/Pawn_64x.png"));
	}
}

FVector UIconComponent::GetIconColor() const
{
	USceneComponent* Parent = GetParentComponent();
	ULightComponent* LightComp = Cast<ULightComponent>(Parent);

	if (!Parent || !LightComp)
	{
		return FVector(1.0f, 1.0f, 1.0f);
	}

	return LightComp->GetLightColor();
}

float UIconComponent::GetIconIntensity() const
{
	USceneComponent* Parent = GetParentComponent();
	ULightComponent* LightComp = Cast<ULightComponent>(Parent);

	if (!Parent || !LightComp)
	{
		return 1.0f;
	}

	return LightComp->GetIntensity();
}

UObject* UIconComponent::Duplicate()
{
	UIconComponent* IconComp = Cast<UIconComponent>(Super::Duplicate());
	return IconComp;
}

void UIconComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
	Super::DuplicateSubObjects(DuplicatedObject);
}
