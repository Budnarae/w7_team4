#pragma once
#include "Actor/Public/Actor.h"

class UBillBoardComponent;
class UAmbientLightComponent;

/**
 * @brief 앰비언트 라이트 액터
 * UAmbientLightComponent를 루트로 사용하는 Actor
 */
UCLASS()
class AAmbientLightActor :
    public AActor
{
    GENERATED_BODY()
    DECLARE_CLASS(AAmbientLightActor, AActor)

public:
    AAmbientLightActor();
    ~AAmbientLightActor() override;

    UClass* GetDefaultRootComponent() override;
    void InitializeComponents() override;

private:
    UAmbientLightComponent* AmbientLightComponent = nullptr;
    UBillBoardComponent* IconComponent = nullptr;
};
