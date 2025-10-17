#pragma once
#include "Render/UI/Widget/Public/LightComponentBaseWidget.h"

class USpotLightComponent;

UCLASS()
class USpotLightComponentWidget : public ULightComponentBaseWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(USpotLightComponentWidget, ULightComponentBaseWidget)

public:
	void RenderWidget() override;

protected:
	USpotLightComponent* SpotLightComponent{};
};