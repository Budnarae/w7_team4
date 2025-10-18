#pragma once

#include "Component/Light/Public/LightComponent.h"

/**
 * @brief SpotLight Component
 * Cone 각도 제한으로 방향성 조명 구현
 */
class USpotLightComponent :
    public ULightComponent
{
    GENERATED_BODY()
    DECLARE_CLASS(USpotLightComponent, ULightComponent)

public:
    USpotLightComponent();
    USpotLightComponent(float InIntensity, const FVector& InLightColor, bool InbVisible);
    ~USpotLightComponent() override;

    void BeginPlay() override;

    // UI 호환 Getters/Setters
    float GetInnerConeAngle() const { return InnerConeAngle; }
    void SetInnerConeAngle(float InInnerConeAngle) { InnerConeAngle = InInnerConeAngle; }

    float GetOuterConeAngle() const { return OuterConeAngle; }
    void SetOuterConeAngle(float InOuterConeAngle) { OuterConeAngle = InOuterConeAngle; }

    float GetAttenuationRadius() const { return AttenuationRadius; }
    void SetAttenuationRadius(float InRadius) { AttenuationRadius = InRadius; }

    float GetLightFalloffExponent() const { return LightFalloffExponent; }
    void SetLightFalloffExponent(float InFalloff) { LightFalloffExponent = InFalloff; }

    float GetRadius() const { return AttenuationRadius; }
    void SetRadius(float InRadius) { AttenuationRadius = InRadius; }
    
    float GetRadiusFalloff() const { return LightFalloffExponent; }

    // Widget
    UClass* GetSpecificWidgetClass() const override;

    // Serialization
    void Serialize(bool bInIsLoading, JSON& InOutHandle) override;

    // Duplicate (PIE 지원)
    UObject* Duplicate() override;

private:
    // Cone Angles
    float InnerConeAngle = 45.0f; // 내부 cone 각도 (도 단위, 완전 밝음)
    float OuterConeAngle = 60.0f; // 외부 cone 각도 (도 단위, 완전 어두움)

    // Light Properties
    float AttenuationRadius = 10.0f;
    float LightFalloffExponent = 2.0f;

    // Runtime flag
    bool bHasBegunPlay = false;
};
