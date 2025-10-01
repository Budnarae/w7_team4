#include "pch.h"
#include "Actor/Public/SphereActor.h"
#include "Component/Mesh/Public/SphereComponent.h"

IMPLEMENT_CLASS(ASphereActor, AActor)

ASphereActor::ASphereActor()
{
	SphereComponent = CreateDefaultSubobject<USphereComponent>("SphereComponent");
	SphereComponent->SetParentAttachment(GetRootComponent());

	{
		SphereComponent = CreateDefaultSubobject<USphereComponent>("SphereComponent");
		SphereComponent->SetRelativeLocation({ 1,3,3, });
		SphereComponent->SetParentAttachment(GetRootComponent());
	}
	{
		SphereComponent = CreateDefaultSubobject<USphereComponent>("SphereComponent");
		SphereComponent->SetRelativeLocation({ 3,1,1, });
		SphereComponent->SetParentAttachment(GetRootComponent());
	}
}

void ASphereActor::PostDuplicate(const TMap<UObject*, UObject*>& InDuplicationMap)
{
	// 1. �θ� Ŭ����(AActor)�� PostDuplicate�� ���� ȣ���մϴ�.
	// �� ȣ��� RootComponent �����Ϳ� �⺻ �������� ��� ó���˴ϴ�.
	Super::PostDuplicate(InDuplicationMap);

	// 2. ���� ��ü���� ���� SphereComponent �����͸� ã���ϴ�.
	if (const ASphereActor* OriginalActor = Cast<const ASphereActor>(SourceObject))
	{
		// 3. DuplicationMap�� ����� ���� ������Ʈ�� �ش��ϴ� '���ο�' ������Ʈ�� ã���ϴ�.
		if (auto It = InDuplicationMap.find(OriginalActor->SphereComponent); It != InDuplicationMap.end())
		{
			// 4. ã�� ���ο� ������Ʈ�� ���� SphereComponent �����͸� �����մϴ�.
			this->SphereComponent = Cast<USphereComponent>(It->second);
		}
	}
}
