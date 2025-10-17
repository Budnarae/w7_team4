#pragma once
#include "Render/UI/Widget/Public/LightComponentBaseWidget.h"

class UDirectionalLightComponent;

UCLASS()
class UDirectionalLightComponentWidget : public ULightComponentBaseWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UDirectionalLightComponentWidget, ULightComponentBaseWidget)

public:
	void RenderWidget() override;

protected:
	UDirectionalLightComponent* DirectionalLightComponent{};
};
