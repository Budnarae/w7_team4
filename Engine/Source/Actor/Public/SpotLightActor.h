#pragma once
#include "Actor/Public/Actor.h"

class UBillBoardComponent;
class USpotLightComponent;

/**
 * @brief 스팟 라이트 액터
 * USpotLightComponent를 루트로 사용하는 Actor
 */
UCLASS()
class ASpotLightActor :
    public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(ASpotLightActor, AActor)

public:
    ASpotLightActor();
    ~ASpotLightActor() override;

    UClass* GetDefaultRootComponent() override;
    void InitializeComponents() override;

private:
    USpotLightComponent* SpotLightComponent = nullptr;
    UBillBoardComponent* IconComponent = nullptr;
};
