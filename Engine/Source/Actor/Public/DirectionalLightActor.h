#pragma once
#include "Actor/Public/Actor.h"

class UBillBoardComponent;
class UDirectionalLightComponent;

/**
 * @brief 방향성 라이트 액터
 * UDirectionalLightComponent를 루트로 사용하는 Actor
 */
UCLASS()
class ADirectionalLightActor :
    public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(ADirectionalLightActor, AActor)

public:
    ADirectionalLightActor();
    ~ADirectionalLightActor() override;

    UClass* GetDefaultRootComponent() override;
    void InitializeComponents() override;

private:
    UDirectionalLightComponent* DirectionalLightComponent = nullptr;
    UBillBoardComponent* IconComponent = nullptr;
};
