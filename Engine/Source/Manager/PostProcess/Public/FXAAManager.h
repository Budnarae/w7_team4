#pragma once
#include "Core/Public/Object.h"

/**
 * @brief FXAA (Fast Approximate Anti-Aliasing) 설정을 관리하는 싱글톤 클래스
 *
 * FXAA 품질과 활성화 상태를 제어합니다.
 */
UCLASS()
class UFXAAManager : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UFXAAManager, UObject)

public:

	// FXAA 활성화/비활성화
	void SetEnabled(bool bEnabled) { bEnableFXAA = bEnabled; }
	bool IsEnabled() const { return bEnableFXAA; }

	// FXAA 품질 파라미터
	void SetSubpixelBlend(float Value) { SubpixelBlend = clamp(Value, 0.0f, 1.0f); }
	void SetEdgeThreshold(float Value) { EdgeThreshold = clamp(Value, 0.0312f, 0.25f); }
	void SetEdgeThresholdMin(float Value) { EdgeThresholdMin = clamp(Value, 0.0f, 0.0833f); }

	float GetSubpixelBlend() const { return SubpixelBlend; }
	float GetEdgeThreshold() const { return EdgeThreshold; }
	float GetEdgeThresholdMin() const { return EdgeThresholdMin; }

	// 프리셋 적용
	void ApplyLowQualityPreset();
	void ApplyMediumQualityPreset();
	void ApplyHighQualityPreset();

private:
	// FXAA 활성화 플래그
	bool bEnableFXAA = true;

	// FXAA 품질 파라미터
	float SubpixelBlend = 0.5f;      // 권장 0.5 (값이 높을수록 부드러우나 흐릿해짐)
	float EdgeThreshold = 0.125f;    // 권장 0.125 (낮을수록 더 많은 에지를 처리)
	float EdgeThresholdMin = 0.0312f; // 권장 0.0312 (낮을수록 약한 에지도 처리)
};
