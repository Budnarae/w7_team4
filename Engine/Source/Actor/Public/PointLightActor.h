#pragma once
#include "Actor/Public/Actor.h"
#include "Core/Public/Class.h"

class UPointLightComponent;
class UBillBoardComponent;

/**
 * @brief 포인트 라이트 액터 (Deferred Volume Lighting)
 * - UPointLightComponent를 루트로 사용하여 UI 연동 및 구 볼륨 기반 조명 계산
 * - Depth 텍스처에서 월드 위치 복원 후 픽셀별 조명 적용
 * - w6_teaM6의 FireBall 시스템 + w7_team4의 UI Light 시스템 통합
 */
UCLASS()
class APointLightActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(APointLightActor, AActor)

public:
	APointLightActor();
	~APointLightActor() override;

	// Getter
	UPointLightComponent* GetPointLightComponent() const { return PointLightComponent; }
	UBillBoardComponent* GetIconComponent() const { return IconComponent; }

	// Forwarding API
	void SetLightColor(const FVector4& InColor);
	void SetIntensity(float InIntensity);
	void SetRadius(float InRadius);
	void SetRadiusFallOff(float InFallOff);

	UClass* GetDefaultRootComponent() override;
	void InitializeComponents() override;

private:
	UPointLightComponent* PointLightComponent = nullptr;
	UBillBoardComponent* IconComponent = nullptr;
};
