#pragma once

#include "Component/Public/SceneComponent.h"

UCLASS()
class ULightComponentBase : public USceneComponent
{
	GENERATED_BODY()
	DECLARE_CLASS(ULightComponentBase, USceneComponent)

protected:
	float Intensity = 0.0f;
	FVector LightColor{0.0f, 0.0f, 0.0f};
	bool bVisible = true;
public:
	ULightComponentBase() = default;
	ULightComponentBase(float InIntensity, const FVector& InLightColor, bool InbVisible);

	~ULightComponentBase() override = default;

	/*
		Getter Setter
	*/
	float GetIntensity() const;
	void SetIntensity(float InIntensity);
	
	FVector GetLightColor() const;
	void SetLightColor(const FVector& InLightColor);
	
	bool IsVisible() const;
	void SetVisible(bool InbVisible);

	/*
		Widget Spawnder
	*/
	UClass* GetSpecificWidgetClass() const override;

	void Serialize(const bool bInIsLoading, JSON& InOutHandle) override;
	UObject* Duplicate() override;
	void DuplicateSubObjects(UObject* DuplicatedObject) override;
};