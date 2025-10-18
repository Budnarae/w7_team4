#include "pch.h"
#include "Component/Public/BillBoardComponent.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Physics/Public/AABB.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/UI/Widget/Public/SpriteSelectionWidget.h"
#include "Texture/Public/Texture.h"
#include "Texture/Public/TextureRenderProxy.h"
#include "Utility/Public/JsonSerializer.h"

IMPLEMENT_CLASS(UBillBoardComponent, UPrimitiveComponent)

UBillBoardComponent::UBillBoardComponent()
{
	Type = EPrimitiveType::Sprite;

    UAssetManager& ResourceManager = UAssetManager::GetInstance();

	Vertices = ResourceManager.GetVertexData(Type);
	VertexBuffer = ResourceManager.GetVertexbuffer(Type);
	NumVertices = ResourceManager.GetNumVertices(Type);

	Indices = ResourceManager.GetIndexData(Type);
	IndexBuffer = ResourceManager.GetIndexbuffer(Type);
	NumIndices = ResourceManager.GetNumIndices(Type);

	RenderState.CullMode = ECullMode::None;
	RenderState.FillMode = EFillMode::Solid;
	BoundingBox = &ResourceManager.GetAABB(Type);

    Sampler = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);
    if (!Sampler)
    {
        assert(false);
    }

    const TMap<FName, ID3D11ShaderResourceView*>& TextureCache = \
        UAssetManager::GetInstance().GetTextureCache();
    if (!TextureCache.empty())
        Sprite = *TextureCache.begin();
}

UBillBoardComponent::~UBillBoardComponent()
{
    if (Sampler)
        Sampler->Release();
}

void UBillBoardComponent::FaceCamera(
    const FVector& CameraPosition,
    const FVector& CameraUp,
    const FVector& FallbackUp
)
{
    // Front
    FVector Front = CameraPosition - GetRelativeLocation();
    Front.Normalize();

    // Right
    FVector Right = CameraUp.Cross(Front);
    if (Right.Length() <= 0.0001f)
    {
        // CameraUp Front FallbackUp
        Right = FallbackUp.Cross(Front);
    }
    Right.Normalize();

    // Up
    FVector Up = Front.Cross(Right);
    Up.Normalize();

    float XAngle = atan2(Up.Y, Up.Z);
    float YAngle = -asin(Up.X);
    float ZAngle = -atan2(-Right.X, Front.X);

    SetRelativeRotation(
        FVector(
            FVector::GetRadianToDegree(XAngle),
            FVector::GetRadianToDegree(YAngle),
            FVector::GetRadianToDegree(ZAngle)
        )
    );
}

const TPair<FName, ID3D11ShaderResourceView*>& UBillBoardComponent::GetSprite() const
{
    return Sprite;
}

void UBillBoardComponent::SetSprite(const TPair<FName, ID3D11ShaderResourceView*>& InSprite)
{
    Sprite = InSprite;
}

void UBillBoardComponent::SetSprite(const UTexture* InSprite)
{
    FName SpriteName = InSprite->GetFilePath();        // 파일 경로 FName 사용
    Sprite = { SpriteName, InSprite->GetRenderProxy()->GetSRV() };
}

ID3D11SamplerState* UBillBoardComponent::GetSampler() const
{
    return Sampler;
};

UClass* UBillBoardComponent::GetSpecificWidgetClass() const
{
    return USpriteSelectionWidget::StaticClass();
}

const FRenderState& UBillBoardComponent::GetClassDefaultRenderState()
{
    static FRenderState DefaultRenderState { ECullMode::None, EFillMode::Solid };
    return DefaultRenderState;
}

void UBillBoardComponent::UpdateBillboardMatrix(const FMatrix& InViewMatrix)
{
    const FVector BasePos = GetWorldLocation();

    // View 행렬의 역행렬에서 회전 부분만 추출
    FVector CameraRight(InViewMatrix.Data[0][0], InViewMatrix.Data[1][0], InViewMatrix.Data[2][0]);
    FVector CameraUp(InViewMatrix.Data[0][1], InViewMatrix.Data[1][1], InViewMatrix.Data[2][1]);
    FVector CameraForward(InViewMatrix.Data[0][2], InViewMatrix.Data[1][2], InViewMatrix.Data[2][2]);

    // 오프셋 적용 (카메라 Up 방향으로 띄우기)
    const FVector FinalPosition = BasePos + CameraUp * ZOffset;

    // 빌보드 회전 행렬 = 카메라 회전의 역행렬 (화면에 평행)
    const FMatrix Rotation = FMatrix(CameraForward, CameraRight, CameraUp);
    const FMatrix Translation = FMatrix::TranslationMatrix(FinalPosition);
    RTMatrix = Rotation * Translation;
}

void UBillBoardComponent::Serialize(const bool bInIsLoading, JSON& InOutHandle)
{
    Super::Serialize(bInIsLoading, InOutHandle);

    if (bInIsLoading)
    {
        // SpritePath 로드
        FString SpritePath;
        if (FJsonSerializer::ReadString(InOutHandle, "Sprite", SpritePath))
        {
            FName PathName(SpritePath);
            auto& AM = UAssetManager::GetInstance();

            // 캐시에 없으면 로드/생성
            if (!AM.HasTexture(PathName)) {
                AM.LoadTexture(PathName);
            }

            const auto& Cache = AM.GetTextureCache();
            if (auto it = Cache.find(PathName); it != Cache.end()) {
                SetSprite(*it); // (FName, SRV*) 적용
            }
        }

        // 오프셋(선택)
        float Offset = 0.f;
        FJsonSerializer::ReadFloat(InOutHandle, "ZOffset", Offset, 0.f, false);
        ZOffset = Offset;
    }
    else
    {
        // 파일 경로 FName을 문자열로 저장
        InOutHandle["Sprite"] = Sprite.first.ToString();
        InOutHandle["ZOffset"] = ZOffset;
    }
}

UObject* UBillBoardComponent::Duplicate()
{
    UBillBoardComponent* BillBoardComponent = Cast<UBillBoardComponent>(Super::Duplicate());

    // 고유 필드 복사
    BillBoardComponent->Sprite = Sprite;
    BillBoardComponent->ZOffset = ZOffset;
    BillBoardComponent->RTMatrix = RTMatrix;

    // COM 리소스 공유 시 AddRef 필요(둘 다 Release 호출하므로)
    BillBoardComponent->Sampler = Sampler;
    if (BillBoardComponent->Sampler)
    {
        BillBoardComponent->Sampler->AddRef();
    }

    return BillBoardComponent;
}

void UBillBoardComponent::DuplicateSubObjects(UObject* DuplicatedObject)
{
    Super::DuplicateSubObjects(DuplicatedObject);
}
