#pragma once
#include "Component/Light/Public/LightComponent.h"

/**
 * @brief Point Light 컴포넌트
 */
UCLASS()
class UPointLightComponent :
    public ULightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(UPointLightComponent, ULightComponent)

public:
    UPointLightComponent();
    ~UPointLightComponent() override;

    void BeginPlay() override;
    void TickComponent() override;

    // UI 호환 Getters/Setters
    float GetAttenuationRadius() const { return AttenuationRadius; }
    void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }

    float GetLightFalloffExponent() const { return LightFalloffExponent; }
    void SetLightFalloffExponent(float InFalloff) { LightFalloffExponent = InFalloff; }

    // Widget 연결
    UClass* GetSpecificWidgetClass() const override;

    // Serialization
    void Serialize(bool bInIsLoading, JSON& InOutHandle) override;

    // Override for Duplicate
    UObject* Duplicate() override;
    void DuplicateSubObjects(UObject* DuplicatedObject) override;

private:
    float AttenuationRadius = 10.0f; // 영향 반경 (월드 단위) - UI 변수명
    float LightFalloffExponent = 2.0f; // 감쇠 지수 (1.0 = 선형, 2.0 = 제곱) - UI 변수명

    bool bHasBegunPlay = false;
};
