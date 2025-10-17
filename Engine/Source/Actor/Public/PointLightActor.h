#pragma once
#include "Actor/Public/Actor.h"
#include "Core/Public/Class.h"

class UPointLightComponent;
class UBillBoardComponent;

/**
 * @brief 포인트 라이트 액터 (Deferred Volume Lighting)
 * UPointLightComponent를 루트로 사용하는 Actor
 */
UCLASS()
class APointLightActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(APointLightActor, AActor)

public:
	APointLightActor();
	~APointLightActor() override;

	UClass* GetDefaultRootComponent() override;
	void InitializeComponents() override;

private:
	UPointLightComponent* PointLightComponent = nullptr;
	UBillBoardComponent* IconComponent = nullptr;
};
