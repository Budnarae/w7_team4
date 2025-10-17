#pragma once
#include "Render/UI/Widget/Public/LightComponentBaseWidget.h"

class UPointLightComponent;

UCLASS()
class UPointLightComponentWidget : public ULightComponentBaseWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UPointLightComponentWidget, ULightComponentBaseWidget)

public:
	void RenderWidget() override;

protected:
	UPointLightComponent* PointLightComponent{};
};