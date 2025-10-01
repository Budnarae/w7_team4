#include "pch.h"
#include "Render/UI/Widget/Public/SpriteSelectionWidget.h"

#include "Level/Public/Level.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Component/Public/BillBoardComponent.h"

#include <climits>

IMPLEMENT_CLASS(USpriteSelectionWidget, UWidget)

USpriteSelectionWidget::USpriteSelectionWidget()
	: UWidget("Sprite Selection Widget")
{
}

USpriteSelectionWidget::~USpriteSelectionWidget() = default;

void USpriteSelectionWidget::Initialize()
{
	// Do Nothing Here
}

void USpriteSelectionWidget::Update()
{
	// �� ������ Level�� ���õ� Actor�� Ȯ���ؼ� ���� �ݿ�
	ULevel* CurrentLevel = GWorld->GetLevel();

	if (CurrentLevel)
	{
		AActor* NewSelectedActor = CurrentLevel->GetSelectedActor();

		// Update Current Selected Actor
		if (SelectedActor != NewSelectedActor)
		{
			SelectedActor = NewSelectedActor;

			for (const TObjectPtr<UActorComponent>& Component : SelectedActor->GetOwnedComponents())
			{
				TObjectPtr<UBillBoardComponent> UUIDTextComponent = Cast<UBillBoardComponent>(Component);
				if (UUIDTextComponent)
					SelectedBillBoard = UUIDTextComponent.Get();
			}
		}
	}
}

void USpriteSelectionWidget::RenderWidget()
{
	// Memory Information
	// ImGui::Text("���� �޸� ����");
	// ImGui::Text("Level Object Count: %u", LevelObjectCount);
	// ImGui::Text("Level Memory: %.3f KB", static_cast<float>(LevelMemoryByte) / KILO);
	// ImGui::Separator();

	if (!SelectedActor)
		return;
	
	ImGui::Separator();
	ImGui::Text("Select Sprite");

	ImGui::Spacing();
		
	static int current_item = 0; // ���� ���õ� �ε���

	// ���� ���ڿ� ���
	TArray<const char*> items;
	const TMap<FName, ID3D11ShaderResourceView*>& TextureCache = \
		UAssetManager::GetInstance().GetTextureCache();

	int i = 0;
	for (auto Itr = TextureCache.begin(); Itr != TextureCache.end(); Itr++, i++)
	{
		if (Itr->first == SelectedBillBoard->GetSprite().first)
			current_item = i;

		items.push_back(Itr->first.ToString().c_str());
	}

	if (ImGui::BeginCombo("Sprite", items[current_item])) // Label�� ���� �� ǥ��
	{
		for (int n = 0; n < items.size(); n++)
		{
			bool is_selected = (current_item == n);
			if (ImGui::Selectable(items[n], is_selected))
			{
				current_item = n;
				SetSpriteOfActor(items[current_item]);
			}

			if (is_selected)
				ImGui::SetItemDefaultFocus(); // �⺻ ��Ŀ��
		}
		ImGui::EndCombo();
	}

	ImGui::Separator();

	WidgetNum = (WidgetNum + 1) % std::numeric_limits<uint32>::max();
}

void USpriteSelectionWidget::SetSpriteOfActor(FString NewSprite)
{
	if (!SelectedActor)
		return;
	if (!SelectedBillBoard)
		return;

	const TMap<FName, ID3D11ShaderResourceView*>& TextureCache = \
		UAssetManager::GetInstance().GetTextureCache();

	SelectedBillBoard->SetSprite(*TextureCache.find(NewSprite));
}
