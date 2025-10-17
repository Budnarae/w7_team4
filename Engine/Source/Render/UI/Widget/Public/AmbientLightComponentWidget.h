#pragma once
#include "Render/UI/Widget/Public/LightComponentBaseWidget.h"

class UAmbientLightComponent;

UCLASS()
class UAmbientLightComponentWidget : public ULightComponentBaseWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(UAmbientLightComponentWidget, ULightComponentBaseWidget)

public:
	void RenderWidget() override;

protected:
	UAmbientLightComponent* AmbientLightComponent{};
};
