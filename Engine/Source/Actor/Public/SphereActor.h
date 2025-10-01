#pragma once

#include "Actor/Public/Actor.h"

class USphereComponent;

UCLASS()
class ASphereActor : public AActor
{
	GENERATED_BODY()
	DECLARE_CLASS(ASphereActor, AActor)

public:
	ASphereActor();

	// AActor�� PostDuplicate�� �������Ͽ� SphereComponent �����͸� �����մϴ�.
	virtual void PostDuplicate(const TMap<UObject*, UObject*>& InDuplicationMap) override;

private:
	USphereComponent* SphereComponent = nullptr;
};
