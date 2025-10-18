#pragma once

struct FRenderingContext
{
    FRenderingContext(const FViewProjConstants* InViewProj, class UCamera* InCurrentCamera, EViewModeIndex InViewMode, uint64 InShowFlags)
        : ViewProjConstants(InViewProj), CurrentCamera(InCurrentCamera), ViewMode(InViewMode), ShowFlags(InShowFlags) {}

    const FViewProjConstants* ViewProjConstants;
    UCamera* CurrentCamera;
    EViewModeIndex ViewMode;
    uint64 ShowFlags;

    // LightPass용 추가 정보
    D3D11_VIEWPORT Viewport = {};
    FVector2 SceneRTSize = FVector2::Zero();

    TArray<class UPrimitiveComponent*> AllPrimitives;
    TArray<class UStaticMeshComponent*> StaticMeshes;
    TArray<class UBillBoardComponent*> BillBoards;
	TArray<class UIconComponent*> Icons;
	TArray<class UTextComponent*> Texts;
	TArray<class UDecalComponent*> AlphaDecals;
	TArray<class UDecalComponent*> AdditiveDecals;
	TArray<class UPrimitiveComponent*> DefaultPrimitives;
    TArray<class UFireBallComponent*> FireBalls;
	TArray<class ULightComponent*> Lights;
};
