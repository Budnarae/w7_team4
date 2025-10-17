#pragma once
#include "Render/UI/Widget/Public/Widget.h"

class ULightComponentBase;

UCLASS()
class ULightComponentBaseWidget : public UWidget
{
	GENERATED_BODY()
	DECLARE_CLASS(ULightComponentBaseWidget, UWidget)

public:
	void RenderWidget() override;

protected:
	ULightComponentBase* LightComponentBase{};
};
