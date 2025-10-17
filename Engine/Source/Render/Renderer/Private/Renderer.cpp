#include "pch.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/FontRenderer/Public/FontRenderer.h"
#include "Component/Public/UUIDTextComponent.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Component/Mesh/Public/StaticMeshComponent.h"
#include "Component/Public/DecalComponent.h"
#include "Component/Public/HeightFogComponent.h"
#include "Component/Public/SemiLightComponent.h"
#include "Component/Public/FireBallComponent.h"
#include "Component/Light/Public/PointLightComponent.h"
#include "Component/Light/Public/SpotLightComponent.h"
#include "Editor/Public/Editor.h"
#include "Editor/Public/Viewport.h"
#include "Editor/Public/ViewportClient.h"
#include "Editor/Public/Camera.h"
#include "Level/Public/Level.h"
#include "Manager/UI/Public/UIManager.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"
#include "Render/RenderPass/Public/RenderPass.h"
#include "Component/Mesh/Public/StaticMesh.h"
#include "Manager/Asset/Public/AssetManager.h"
#include "Optimization/Public/OcclusionCuller.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/RenderPass/Public/BillboardPass.h"
#include "Render/RenderPass/Public/PrimitivePass.h"
#include "Render/RenderPass/Public/StaticMeshPass.h"
#include "Render/RenderPass/Public/TextPass.h"
#include "Render/RenderPass/Public/DecalPass.h"
#include "Render/RenderPass/Public/FireBallPass.h"
#include "Render/RenderPass/Public/FireBallForwardPass.h"

IMPLEMENT_SINGLETON_CLASS_BASE(URenderer)

URenderer::URenderer() = default;

URenderer::~URenderer() = default;

void URenderer::Init(HWND InWindowHandle)
{
	DeviceResources = new UDeviceResources(InWindowHandle);
	Pipeline = new UPipeline(GetDeviceContext());
	ViewportClient = new FViewport();

	// ?�더�??�태 �?리소???�성
	CreateDepthStencilState();
	CreateBlendState();
	CreateDefaultShader();
	CreateTextureShader();
	CreateDecalShader();
	CreateDepthShader();
	CreateFireBallShader();
	CreateFireBallForwardShader();
	CreateUberLightShader();
	CreateFullscreenQuad();
	CreateConstantBuffers();
	CreatePostProcessResources();

	// FontRenderer 초기??
	FontRenderer = new UFontRenderer();
	if (!FontRenderer->Initialize())
	{
		UE_LOG("FontRenderer 초기???�패");
		SafeDelete(FontRenderer);
	}

	ViewportClient->InitializeLayout(DeviceResources->GetViewportInfo());

	// Scene RT??ViewportClient 초기???�에 ?�성 (?�바�??�기 ?�용)
	CreateSceneRenderTargets();

	FStaticMeshPass* StaticMeshPass = new FStaticMeshPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		TextureVertexShader, TexturePixelShader, TextureInputLayout, DefaultDepthStencilState,
		DepthVertexShader, DepthPixelShader, DepthInputLayout);
	RenderPasses.push_back(StaticMeshPass);

	FPrimitivePass* PrimitivePass = new FPrimitivePass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		DefaultVertexShader, DefaultPixelShader, DefaultInputLayout, DefaultDepthStencilState,
		DepthVertexShader, DepthPixelShader, DepthInputLayout);
	RenderPasses.push_back(PrimitivePass);

	// ?�파 블렌?�을 ?�용?�는 ?�반 ?�칼 ?�스
	FDecalPass* AlphaDecalPass = new FDecalPass(Pipeline, ConstantBufferViewProj,
		DecalVertexShader, DecalPixelShader, DecalInputLayout, DecalDepthStencilState, AlphaBlendState, false);
	RenderPasses.push_back(AlphaDecalPass);

	// 가???�합???�용?�는 SemiLight ?�칼 ?�스
	FDecalPass* AdditiveDecalPass = new FDecalPass(Pipeline, ConstantBufferViewProj,
		DecalVertexShader, DecalPixelShader, DecalInputLayout, DecalDepthStencilState, AdditiveBlendState, true);
	RenderPasses.push_back(AdditiveDecalPass);

	FBillboardPass* BillboardPass = new FBillboardPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		TextureVertexShader, TexturePixelShader, TextureInputLayout, DefaultDepthStencilState);
	RenderPasses.push_back(BillboardPass);

	FTextPass* TextPass = new FTextPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels);
	RenderPasses.push_back(TextPass);

	// Deferred Volume Point Light (효율적인 구 볼륨 렌더링)
	FFireBallPass* FireBallPass = new FFireBallPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		FireBallVertexShader, FireBallPixelShader, FireBallInputLayout, DecalDepthStencilState, FireBallBlendState);
	RenderPasses.push_back(FireBallPass);

	// Forward Point Light (테스트용, 성능 낮음)
	// FFireBallForwardPass* FireBallForwardPass = new FFireBallForwardPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
	// 	FireBallFwdVertexShader, FireBallFwdPixelShader, FireBallFwdInputLayout, DecalDepthStencilState, AdditiveBlendState);
	// RenderPasses.push_back(FireBallForwardPass);
}

void URenderer::Release()
{
	ReleaseSceneRenderTargets();
	ReleaseConstantBuffers();
	ReleaseDefaultShader();
	ReleaseDepthShader();
	ReleaseFireBallShader();
	ReleaseFireBallForwardShader();
	ReleaseUberLightShader();
	ReleaseFullscreenQuad();
	ReleaseDepthStencilState();
	ReleaseBlendState();
	ReleasePostProcessResources();
	FRenderResourceFactory::ReleaseRasterizerState();
	for (auto& RenderPass : RenderPasses)
	{
		RenderPass->Release();
		SafeDelete(RenderPass);
	}

	SafeDelete(ViewportClient);
	SafeDelete(FontRenderer);
	SafeDelete(Pipeline);
	SafeDelete(DeviceResources);
}

void URenderer::CreateDepthStencilState()
{
	// 3D Default Depth Stencil (Depth O, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DefaultDescription = {};
	DefaultDescription.DepthEnable = TRUE;
	DefaultDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	DefaultDescription.DepthFunc = D3D11_COMPARISON_LESS;
	DefaultDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DefaultDescription, &DefaultDepthStencilState);

	// Decal Depth Stencil (Depth Read, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DecalDescription = {};
	DecalDescription.DepthEnable = TRUE;
	DecalDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	DecalDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	DecalDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DecalDescription, &DecalDepthStencilState);

	// Disabled Depth Stencil (Depth X, Stencil X)
	D3D11_DEPTH_STENCIL_DESC DisabledDescription = {};
	DisabledDescription.DepthEnable = FALSE;
	DisabledDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DisabledDescription, &DisabledDepthStencilState);

	// No Test But Write Depth (Depth Test X, Depth Write O) - ?�스???�로?�스??
	D3D11_DEPTH_STENCIL_DESC NoTestWriteDescription = {};
	NoTestWriteDescription.DepthEnable = TRUE;  // Depth ?�성??(write�??�해)
	NoTestWriteDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;  // Depth write ?�성??
	NoTestWriteDescription.DepthFunc = D3D11_COMPARISON_ALWAYS;  // ??�� ?�과 (test 비활?�화)
	NoTestWriteDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&NoTestWriteDescription, &NoTestButWriteDepthState);
}

void URenderer::CreateBlendState()
{
    // Alpha Blending
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    GetDevice()->CreateBlendState(&blendDesc, &AlphaBlendState);

    // Additive Blending (for lights)
    D3D11_BLEND_DESC additiveDesc = {};
    additiveDesc.RenderTarget[0].BlendEnable = TRUE;
    additiveDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    additiveDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
    additiveDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    additiveDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    additiveDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
    additiveDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    additiveDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    GetDevice()->CreateBlendState(&additiveDesc, &AdditiveBlendState);

	D3D11_BLEND_DESC fireBallDesc = {};
	additiveDesc.RenderTarget[0].BlendEnable = TRUE;
	additiveDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	additiveDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	additiveDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	additiveDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	additiveDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	additiveDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	additiveDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	GetDevice()->CreateBlendState(&additiveDesc, &FireBallBlendState);
}

void URenderer::CreateDefaultShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DefaultLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/SampleShader.hlsl", DefaultLayout, &DefaultVertexShader, &DefaultInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/SampleShader.hlsl", &DefaultPixelShader);
	Stride = sizeof(FNormalVertex);
}

void URenderer::CreateDecalShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DecalLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/DecalShader.hlsl", DecalLayout, &DecalVertexShader, &DecalInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/DecalShader.hlsl", &DecalPixelShader);
}

void URenderer::CreateTextureShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> TextureLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/TextureShader.hlsl", TextureLayout, &TextureVertexShader, &TextureInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/TextureShader.hlsl", &TexturePixelShader);
}

void URenderer::CreateDepthShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DepthLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0	},
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0	}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/DepthShader.hlsl", DepthLayout, &DepthVertexShader, &DepthInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/DepthShader.hlsl", &DepthPixelShader);
}

void URenderer::CreateConstantBuffers()
{
	ConstantBufferModels = FRenderResourceFactory::CreateConstantBuffer<FMatrix>();
	ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
	ConstantBufferViewProj = FRenderResourceFactory::CreateConstantBuffer<FViewProjConstants>();
}

void URenderer::ReleaseConstantBuffers()
{
	SafeRelease(ConstantBufferModels);
	SafeRelease(ConstantBufferColor);
	SafeRelease(ConstantBufferViewProj);
}

void URenderer::ReleaseDefaultShader()
{
	SafeRelease(DefaultInputLayout);
	SafeRelease(DefaultPixelShader);
	SafeRelease(DefaultVertexShader);
	SafeRelease(TextureInputLayout);
	SafeRelease(TexturePixelShader);
	SafeRelease(TextureVertexShader);
	SafeRelease(DecalVertexShader);
	SafeRelease(DecalPixelShader);
}

void URenderer::ReleaseDepthShader()
{
	SafeRelease(DepthInputLayout);
	SafeRelease(DepthPixelShader);
	SafeRelease(DepthVertexShader);
}

void URenderer::ReleaseDepthStencilState()
{
	SafeRelease(DefaultDepthStencilState);
	SafeRelease(DecalDepthStencilState);
	SafeRelease(DisabledDepthStencilState);
	SafeRelease(NoTestButWriteDepthState);
	if (GetDeviceContext())
	{
		GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);
	}
}

void URenderer::ReleaseBlendState()
{
    SafeRelease(AlphaBlendState);
    SafeRelease(AdditiveBlendState);
}

void URenderer::Update()
{
	RenderBegin();

	for (FViewportClient& ViewportClient : ViewportClient->GetViewports())
	{
		if (ViewportClient.GetViewportInfo().Width < 1.0f || ViewportClient.GetViewportInfo().Height < 1.0f) { continue; }

		UCamera* CurrentCamera = &ViewportClient.Camera;
		CurrentCamera->Update(ViewportClient.GetViewportInfo());
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj, CurrentCamera->GetFViewProjConstants());
		Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

		const D3D11_VIEWPORT& ClientViewport = ViewportClient.GetViewportInfo();

		// === Scene RT ?�더�? clientViewport?� ?�일??viewport ?�용 ===
		// Scene RT??SwapChain ?�체 ?�기�??�성?�었?��?�?
		// �?ViewportClient??TopLeftX/Y�?그�?�??�용?�여 ?�당 ?�역???�더�?

		// IMPORTANT: �?viewport마다 Scene RT�??�시 바인??
		// (?�전 viewport??post-processing??BackBuffer�?바인?�을 변경했?��?�?
		ID3D11RenderTargetView* SceneRtvs[] = { SceneColorRTV };
		GetDeviceContext()->OMSetRenderTargets(1, SceneRtvs, SceneDepthDSV);
		GetDeviceContext()->RSSetViewports(1, &ClientViewport);

		{
			TIME_PROFILE(RenderLevel)
			RenderLevel(CurrentCamera);
		}

		// === PointLight 렌더링 (w6_team6 방식 - Level 기반) ===
		// === UberLight 렌더링 (PointLight + SpotLight 통합) ===
		{
			TIME_PROFILE(RenderUberLights)
			RenderUberLights(CurrentCamera, ClientViewport);
		}

		// === ?�버�??�리미티�??�더�? Scene RT???�더�?(FXAA ?�용) ===
		{
			TIME_PROFILE(RenderDebugPrimitives)
			GEditor->GetEditorModule()->RenderDebugPrimitives(CurrentCamera);
		}

		// === Post-Processing: Scene RT -> 백버??===
		// ?�합 ?�스???�로?�싱 ?�스: Fog + Anti-Aliasing (FXAA)
		// RenderLevel + RenderDebugPrimitives 결과??모두 FXAA ?�용
		GetDeviceContext()->RSSetViewports(1, &ClientViewport);

		{
			TIME_PROFILE(ExecutePostProcess)
			ExecutePostProcess(CurrentCamera, ClientViewport); // Fog + FXAA ?�합
		}

		// === 기즈�??�더�? BackBuffer??직접 ?�더�?(FXAA 미적?? ??�� ?�명) ===
		{
			TIME_PROFILE(RenderGizmo)
			GEditor->GetEditorModule()->RenderGizmo(CurrentCamera);
		}
	}

	{
		TIME_PROFILE(UUIManager)
		UUIManager::GetInstance().Render();
	}
	{
		TIME_PROFILE(UStatOverlay)
		UStatOverlay::GetInstance().Render();
	}

	RenderEnd();
}

void URenderer::RenderBegin() const
{
	// BackBuffer ?�리??(post-processing 결과�?받을 �?
	auto* BackBufferRTV = DeviceResources->GetRenderTargetView();
	auto* BackBufferDSV = DeviceResources->GetDepthStencilView();
	GetDeviceContext()->ClearRenderTargetView(BackBufferRTV, ClearColor);
	GetDeviceContext()->ClearDepthStencilView(BackBufferDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Scene RT ?�리??�?바인??(Scene Color + Scene Depth)
	// ?�후 �?ViewportClient가 Scene RT???�당 ?�역???�더링함
	GetDeviceContext()->ClearRenderTargetView(SceneColorRTV, ClearColor);
	GetDeviceContext()->ClearDepthStencilView(SceneDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	ID3D11RenderTargetView* SceneRtvs[] = { SceneColorRTV };
	GetDeviceContext()->OMSetRenderTargets(1, SceneRtvs, SceneDepthDSV);

	DeviceResources->UpdateViewport();
}

void URenderer::RenderLevel(UCamera* InCurrentCamera)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel) { return; }
	
	const FViewProjConstants& ViewProj = InCurrentCamera->GetFViewProjConstants();
	TArray<UPrimitiveComponent*> FinalVisiblePrims = InCurrentCamera->GetViewVolumeCuller().GetRenderableObjects();

	// // ?�클루전 컬링 ?�행
	// TIME_PROFILE(Occlusion)
	// static COcclusionCuller Culler;
	// Culler.InitializeCuller(ViewProj.View, ViewProj.Projection);
	// FinalVisiblePrims = Culler.PerformCulling(
	// 	InCurrentCamera->GetViewVolumeCuller().GetRenderableObjects(),
	// 	InCurrentCamera->GetLocation()
	// );
	// TIME_PROFILE_END(Occlusion) 


	FRenderingContext RenderingContext(&ViewProj, InCurrentCamera, GEditor->GetEditorModule()->GetViewMode(), CurrentLevel->GetShowFlags());
	RenderingContext.AllPrimitives = FinalVisiblePrims;
	for (auto& Prim : FinalVisiblePrims)
	{
		if (auto StaticMesh = Cast<UStaticMeshComponent>(Prim))
		{
			RenderingContext.StaticMeshes.push_back(StaticMesh);
		}
		else if (auto BillBoard = Cast<UBillBoardComponent>(Prim))
		{
			RenderingContext.BillBoards.push_back(BillBoard);
		}
		else if (auto FireBall = Cast<UFireBallComponent>(Prim))
		{
			RenderingContext.FireBalls.push_back(FireBall);
		}
		// PointLight는 이제 Level::GetAllPointLights()로 직접 가져옴 (w6_team6 방식)
		else if (auto Text = Cast<UTextComponent>(Prim); Text && !Text->IsExactly(UUUIDTextComponent::StaticClass()))
		{
			RenderingContext.Texts.push_back(Text);
		}
		else if (!Prim->IsA(UUUIDTextComponent::StaticClass()))
		{
			RenderingContext.DefaultPrimitives.push_back(Prim);
		}
	}
	// ?�집 ?�에 ?�래�??�인
	const bool bWantsDecal = (CurrentLevel->GetShowFlags() & EEngineShowFlags::SF_Decal) != 0;
	if (bWantsDecal)
	{
		UStatOverlay::GetInstance().ResetDecalFrame();
		for (auto Decal : CurrentLevel->GetVisibleDecals())
		{
			if (Cast<USemiLightComponent>(Decal->GetParentAttachment()))
			{
				RenderingContext.AdditiveDecals.push_back(Decal);
			}
			else
			{
				RenderingContext.AlphaDecals.push_back(Decal);
			}
		}
		
		UStatOverlay::GetInstance().RecordDecalCollection(
			static_cast<uint32>(CurrentLevel->GetAllDecals().size()),
			static_cast<uint32>(CurrentLevel->GetVisibleDecals().size())
			);
	}

	for (auto RenderPass: RenderPasses)
	{
		RenderPass->Execute(RenderingContext);
	}
}

void URenderer::RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride, uint32 InIndexBufferStride)
{
    // Use the global stride if InStride is 0
    const uint32 FinalStride = (InStride == 0) ? Stride : InStride;

    // Allow for custom shaders, fallback to default
    FPipelineInfo PipelineInfo = {
        InPrimitive.InputLayout ? InPrimitive.InputLayout : DefaultInputLayout,
        InPrimitive.VertexShader ? InPrimitive.VertexShader : DefaultVertexShader,
		FRenderResourceFactory::GetRasterizerState(InRenderState),
        InPrimitive.bShouldAlwaysVisible ? DisabledDepthStencilState : DefaultDepthStencilState,
        InPrimitive.PixelShader ? InPrimitive.PixelShader : DefaultPixelShader,
        nullptr,
        InPrimitive.Topology
    };
    Pipeline->UpdatePipeline(PipelineInfo);

    // Update constant buffers
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels, FMatrix::GetModelMatrix(InPrimitive.Location, FVector::GetDegreeToRadian(InPrimitive.Rotation), InPrimitive.Scale));
	Pipeline->SetConstantBuffer(0, true, ConstantBufferModels);
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
	
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferColor, InPrimitive.Color);
	Pipeline->SetConstantBuffer(2, false, ConstantBufferColor);
	Pipeline->SetConstantBuffer(2, true, ConstantBufferColor);
	
    Pipeline->SetVertexBuffer(InPrimitive.Vertexbuffer, FinalStride);

    // The core logic: check for an index buffer
    if (InPrimitive.IndexBuffer && InPrimitive.NumIndices > 0)
    {
        Pipeline->SetIndexBuffer(InPrimitive.IndexBuffer, InIndexBufferStride);
        Pipeline->DrawIndexed(InPrimitive.NumIndices, 0, 0);
    }
    else
    {
        Pipeline->Draw(InPrimitive.NumVertices, 0);
    }
}

void URenderer::RenderEnd() const
{
	TIME_PROFILE(DrawCall)
	GetSwapChain()->Present(0, 0);
	TIME_PROFILE_END(DrawCall)
}

void URenderer::OnResize(uint32 InWidth, uint32 InHeight)
{
	if (!DeviceResources || !GetDeviceContext() || !GetSwapChain()) return;

	this->ReleaseSceneRenderTargets();
	DeviceResources->ReleaseFrameBuffer();
	DeviceResources->ReleaseDepthBuffer();
	GetDeviceContext()->OMSetRenderTargets(0, nullptr, nullptr);

	if (FAILED(GetSwapChain()->ResizeBuffers(2, InWidth, InHeight, DXGI_FORMAT_UNKNOWN, 0)))
	{
		UE_LOG("OnResize Failed");
		return;
	}

	DeviceResources->UpdateViewport();
	DeviceResources->CreateFrameBuffer();
	DeviceResources->CreateDepthBuffer();

	// Recreate Scene Render Targets with new size
	this->CreateSceneRenderTargets();

	auto* RenderTargetView = DeviceResources->GetRenderTargetView();
	ID3D11RenderTargetView* RenderTargetViews[] = { RenderTargetView };
	GetDeviceContext()->OMSetRenderTargets(1, RenderTargetViews, DeviceResources->GetDepthStencilView());
}

void URenderer::CreatePostProcessResources()
{
	// PostProcess ?�이??로드 (?�합 ?�스???�로?�싱: Fog + FXAA)
	TArray<D3D11_INPUT_ELEMENT_DESC> PostProcessLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/PostProcess.hlsl",
		PostProcessLayout,
		&PostProcessVertexShader,
		&PostProcessInputLayout
	);

	FRenderResourceFactory::CreatePixelShader(
		L"Asset/Shader/PostProcess.hlsl",
		&PostProcessPixelShader
	);

	// ?�형 ?�램???�플??
	PostProcessSamplerState = FRenderResourceFactory::CreateSamplerState(
		D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP
	);
	ConstantBufferPostProcessParameters = FRenderResourceFactory::CreateConstantBuffer<FPostProcessParameters>();
	PostProcessUserParameters = {}; // 기본�???구조�??�폴??
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPostProcessParameters, PostProcessUserParameters);
}

void URenderer::ReleasePostProcessResources()
{
	SafeRelease(PostProcessVertexShader);
	SafeRelease(PostProcessInputLayout);
	SafeRelease(PostProcessPixelShader);
	SafeRelease(PostProcessSamplerState);
	SafeRelease(ConstantBufferPostProcessParameters);
}

void URenderer::ExecutePostProcess(UCamera* InCurrentCamera, const D3D11_VIEWPORT& InViewport)
{
	auto* Context = GetDeviceContext();
	const ULevel* CurrentLevel = GWorld->GetLevel();

	// 출력: 백버??RTV�?
	auto* BackBufferRTV = DeviceResources->GetRenderTargetView();
	auto* BackBufferDSV = DeviceResources->GetDepthStencilView();
	Context->OMSetRenderTargets(1, &BackBufferRTV, BackBufferDSV);

	// Viewport ?�정 (�?ViewportClient ?�역?�만 ?�용)
	Context->RSSetViewports(1, &InViewport);

	// PostProcess ?�수 버퍼 ?�데?�트 (viewport + fog + FXAA ?�보)
	FPostProcessParameters postProcessParams = PostProcessUserParameters; // 기존 ?�용???�라미터 복사

	// FXAA ?�성???�래�??�정
	postProcessParams.EnableFXAA = bIsFXAAEnabled ? 1.0f : 0.0f;

	// Viewport ?�보 ?�정
	postProcessParams.ViewportTopLeft = FVector2(InViewport.TopLeftX, InViewport.TopLeftY);
	postProcessParams.ViewportSize = FVector2(InViewport.Width, InViewport.Height);

	// Scene RT ?�기 가?�오�?
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	GetSwapChain()->GetDesc(&swapChainDesc);
	postProcessParams.SceneRTSize = FVector2(
		static_cast<float>(swapChainDesc.BufferDesc.Width),
		static_cast<float>(swapChainDesc.BufferDesc.Height)
	);

	// Fog ?�라미터 ?�정
	const bool bShowFog = CurrentLevel && (CurrentLevel->GetShowFlags() & EEngineShowFlags::SF_Fog) != 0;

	// Find first enabled HeightFogComponent
	UHeightFogComponent* FogComponent = nullptr;
	if (CurrentLevel && bShowFog)
	{
		for (AActor* Actor : CurrentLevel->GetActors())
		{
			if (!Actor) continue;
			for (UActorComponent* Comp : Actor->GetOwnedComponents())
			{
				if (auto* Fog = Cast<UHeightFogComponent>(Comp))
				{
					if (Fog->IsEnabled())
					{
						FogComponent = Fog;
						break;
					}
				}
			}
			if (FogComponent) break;
		}
	}

	// Fog ?�라미터 채우�?
	if (FogComponent && bShowFog)
	{
		postProcessParams.FogDensity = FogComponent->GetFogDensity();
		postProcessParams.FogHeightFalloff = FogComponent->GetFogHeightFalloff();
		postProcessParams.StartDistance = FogComponent->GetStartDistance();
		postProcessParams.FogCutoffDistance = FogComponent->GetFogCutoffDistance();
		postProcessParams.FogMaxOpacity = FogComponent->GetFogMaxOpacity();

		FVector4 ColorRGBA = FogComponent->GetFogInscatteringColor();
		postProcessParams.FogInscatteringColor = FVector(ColorRGBA.X, ColorRGBA.Y, ColorRGBA.Z);

		postProcessParams.CameraPosition = InCurrentCamera->GetLocation();
		postProcessParams.FogHeight = FogComponent->GetWorldLocation().Z;
	}
	else
	{
		// Fog 비활?�화
		postProcessParams.FogDensity = 0.0f;
		postProcessParams.FogMaxOpacity = 0.0f;
	}

	// Inverse View-Projection Matrix
	const FViewProjConstants& ViewProj = InCurrentCamera->GetFViewProjConstants();
	FMatrix ViewProjMatrix = ViewProj.View * ViewProj.Projection;
	postProcessParams.InvViewProj = ViewProjMatrix.Inverse();

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPostProcessParameters, postProcessParams);

	// ?�이?�라???�업
	FPipelineInfo PipelineInfo = {
		PostProcessInputLayout,                     // PostProcess fullscreen quad layout
		PostProcessVertexShader,                    // PostProcess VS (fullscreen quad)
		FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid }),
		NoTestButWriteDepthState,                   // Depth test X, Depth write O
		PostProcessPixelShader,                     // PostProcess PS (Fog + FXAA ?�합)
		nullptr,                                    // Blend
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	Pipeline->SetConstantBuffer(0, false, ConstantBufferPostProcessParameters);

	// ?�스 ?�스�??�플??(Scene Color + Scene Depth)
	ID3D11ShaderResourceView* srvs[2] = { SceneColorSRV, SceneDepthSRV };
	Context->PSSetShaderResources(0, 2, srvs);
	Pipeline->SetSamplerState(0, false, PostProcessSamplerState);

	// Fullscreen Quad 그리�?(RenderFog?� ?�일??방식)
	uint32 stride = sizeof(float) * 5;  // Position(3) + TexCoord(2)
	uint32 offset = 0;
	Pipeline->SetVertexBuffer(FullscreenQuadVB, stride);
	Pipeline->SetIndexBuffer(FullscreenQuadIB, sizeof(uint32));
	Pipeline->DrawIndexed(6, 0, 0);

	// SRV ?�바?�드(경고 방�?)
	ID3D11ShaderResourceView* NullSrvs[2] = { nullptr, nullptr };
	Context->PSSetShaderResources(0, 2, NullSrvs);
}
void URenderer::UpdatePostProcessConstantBuffer()
{
	if (ConstantBufferPostProcessParameters)
	{
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferPostProcessParameters, PostProcessUserParameters);
	}
}

void URenderer::SetFXAASubpixelBlend(float InValue)
{
	float Clamped = std::clamp(InValue, 0.0f, 1.0f);
	if (PostProcessUserParameters.SubpixelBlend != Clamped)
	{
		PostProcessUserParameters.SubpixelBlend = Clamped;
		UpdatePostProcessConstantBuffer();
	}
}

void URenderer::SetFXAAEdgeThreshold(float InValue)
{
	// ?�반?�으�?0.05 ~ 0.25 권장
	float Clamped = std::clamp(InValue, 0.01f, 0.5f);
	if (PostProcessUserParameters.EdgeThreshold != Clamped)
	{
		PostProcessUserParameters.EdgeThreshold = Clamped;
		UpdatePostProcessConstantBuffer();
	}
}

void URenderer::SetFXAAEdgeThresholdMin(float InValue)
{
	// ?�반?�으�?0.002 ~ 0.05 권장
	float Clamped = std::clamp(InValue, 0.001f, 0.1f);
	if (PostProcessUserParameters.EdgeThresholdMin != Clamped)
	{
		PostProcessUserParameters.EdgeThresholdMin = Clamped;
		UpdatePostProcessConstantBuffer();
	}
}

void URenderer::CreateFireBallShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> FireBallLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
	};

    // Use default factory entries (mainVS/mainPS)
    // Viewport-corrected version of FireBall shader
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/FireBallShaderFixed.hlsl", FireBallLayout, &FireBallVertexShader, &FireBallInputLayout);
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/FireBallShaderFixed.hlsl", &FireBallPixelShader);

	CBPerObject = FRenderResourceFactory::CreateConstantBuffer<FPerObjectCB>();
	CBFireBall = FRenderResourceFactory::CreateConstantBuffer<FFireBallCB>();
}


void URenderer::ReleaseFireBallShader()
{

	SafeRelease(FireBallVertexShader);
	SafeRelease(FireBallPixelShader);
	SafeRelease(FireBallInputLayout);
}

void URenderer::CreateFullscreenQuad()
{
	struct
	{
		FVector Position;
		float U, V;
	} vertices[] =
	{
		{ FVector(-1.0f,  1.0f, 0.0f), 0.0f, 0.0f },  // Top-left
		{ FVector( 1.0f,  1.0f, 0.0f), 1.0f, 0.0f },  // Top-right
		{ FVector(-1.0f, -1.0f, 0.0f), 0.0f, 1.0f },  // Bottom-left
		{ FVector( 1.0f, -1.0f, 0.0f), 1.0f, 1.0f }   // Bottom-right
	};

	uint32 indices[] = { 0, 1, 2, 2, 1, 3 };

	D3D11_BUFFER_DESC vbd = {};
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = sizeof(vertices);
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA vInitData = {};
	vInitData.pSysMem = vertices;
	GetDevice()->CreateBuffer(&vbd, &vInitData, &FullscreenQuadVB);

	D3D11_BUFFER_DESC ibd = {};
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(indices);
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA iInitData = {};
	iInitData.pSysMem = indices;
	GetDevice()->CreateBuffer(&ibd, &iInitData, &FullscreenQuadIB);
}

void URenderer::ReleaseFullscreenQuad()
{
	SafeRelease(FullscreenQuadVB);
	SafeRelease(FullscreenQuadIB);
}

void URenderer::CreateSceneRenderTargets()
{
	// Scene RT??SwapChain ?�체 ?�기�??�성 (4분할 viewport 지??
	// �?ViewportClient??Scene RT???�당 ?�역???�더링됨
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	GetSwapChain()->GetDesc(&swapChainDesc);
	uint32 Width = swapChainDesc.BufferDesc.Width;
	uint32 Height = swapChainDesc.BufferDesc.Height;

	if (Width == 0 || Height == 0)
	{
		UE_LOG("CreateSceneRenderTargets: Invalid SwapChain size %ux%u", Width, Height);
		return;
	}

	UE_LOG("CreateSceneRenderTargets: Creating %ux%u textures (SwapChain full size)", Width, Height);

	// Scene Color Render Target
	D3D11_TEXTURE2D_DESC ColorDescription = {};
	ColorDescription.Width = Width;
	ColorDescription.Height = Height;
	ColorDescription.MipLevels = 1;
	ColorDescription.ArraySize = 1;
	ColorDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	ColorDescription.SampleDesc.Count = 1;
	ColorDescription.SampleDesc.Quality = 0;
	ColorDescription.Usage = D3D11_USAGE_DEFAULT;
	ColorDescription.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	ColorDescription.CPUAccessFlags = 0;
	ColorDescription.MiscFlags = 0;

	GetDevice()->CreateTexture2D(&ColorDescription, nullptr, &SceneColorTexture);

	// RTV ?�성 (???�더링용)
	D3D11_RENDER_TARGET_VIEW_DESC RTVDescription = {};
	RTVDescription.Format = ColorDescription.Format;
	RTVDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDescription.Texture2D.MipSlice = 0;
	GetDevice()->CreateRenderTargetView(SceneColorTexture, &RTVDescription, &SceneColorRTV);

	// SRV ?�성 (?�스???�로?�스?�서 ?�기??
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDescription = {};
	SRVDescription.Format = ColorDescription.Format;
	SRVDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDescription.Texture2D.MostDetailedMip = 0;
	SRVDescription.Texture2D.MipLevels = 1;
	GetDevice()->CreateShaderResourceView(SceneColorTexture, &SRVDescription, &SceneColorSRV);

	// Scene Depth Texture (SRV 지??
	D3D11_TEXTURE2D_DESC DepthDescription = {};
	DepthDescription.Width = Width;
	DepthDescription.Height = Height;
	DepthDescription.MipLevels = 1;
	DepthDescription.ArraySize = 1;
	DepthDescription.Format = DXGI_FORMAT_R24G8_TYPELESS; // Typeless�??�성 (DSV?� SRV 모두 지??
	DepthDescription.SampleDesc.Count = 1;
	DepthDescription.SampleDesc.Quality = 0;
	DepthDescription.Usage = D3D11_USAGE_DEFAULT;
	DepthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	DepthDescription.CPUAccessFlags = 0;
	DepthDescription.MiscFlags = 0;

	GetDevice()->CreateTexture2D(&DepthDescription, nullptr, &SceneDepthTexture);

	// DSV ?�성 (Depth ?�기??
	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDescription = {};
	DSVDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;  // Depth 24비트 + Stencil 8비트
	DSVDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDescription.Texture2D.MipSlice = 0;
	GetDevice()->CreateDepthStencilView(SceneDepthTexture, &DSVDescription, &SceneDepthDSV);

	// SRV ?�성 (Depth ?�기??- Stencil?� 무시)
	D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc = {};
	depthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;  // Depth�??�기, Stencil 무시
	depthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	depthSRVDesc.Texture2D.MostDetailedMip = 0;
	depthSRVDesc.Texture2D.MipLevels = 1;
	GetDevice()->CreateShaderResourceView(SceneDepthTexture, &depthSRVDesc, &SceneDepthSRV);

	// Read Only DSV
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvRO = {};
	dsvRO.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvRO.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvRO.Texture2D.MipSlice = 0;
	dsvRO.Flags = D3D11_DSV_READ_ONLY_DEPTH; // ?�요 ??| D3D11_DSV_READ_ONLY_STENCIL
	GetDevice()->CreateDepthStencilView(SceneDepthTexture, &dsvRO, &SceneDepthDSV_ReadOnly);

	UE_LOG("Scene Render Targets Created: %ux%u", Width, Height);
}

void URenderer::ReleaseSceneRenderTargets()
{
	SafeRelease(SceneColorSRV);
	SafeRelease(SceneColorRTV);
	SafeRelease(SceneColorTexture);

	SafeRelease(SceneDepthSRV);
	SafeRelease(SceneDepthDSV);
	SafeRelease(SceneDepthTexture);
}

void URenderer::CreateFireBallForwardShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> layout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord),   D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/FireBallForward.hlsl", layout, &FireBallFwdVertexShader, &FireBallFwdInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/FireBallForward.hlsl", &FireBallFwdPixelShader);
}

void URenderer::ReleaseFireBallForwardShader()
{
	SafeRelease(FireBallFwdVertexShader);
	SafeRelease(FireBallFwdPixelShader);
	SafeRelease(FireBallFwdInputLayout);
}

// ========================================
// UberLight Rendering (PointLight + SpotLight 통합)
// ========================================

void URenderer::CreateUberLightShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> layout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord),   D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/UberLightShader.hlsl", layout, &UberLightVertexShader, &UberLightInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLightShader.hlsl", &UberLightPixelShader);

	ConstantBufferLightProperties = FRenderResourceFactory::CreateConstantBuffer<FLightProperties>();
	UberLightSamplerState = FRenderResourceFactory::CreateSamplerState(D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);

	// Depth State: LESS_EQUAL, Write OFF (for camera outside light volume)
	D3D11_DEPTH_STENCIL_DESC depthDesc = {};
	depthDesc.DepthEnable = TRUE;
	depthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthDesc.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&depthDesc, &UberLightDepthState);
}

void URenderer::ReleaseUberLightShader()
{
	SafeRelease(UberLightVertexShader);
	SafeRelease(UberLightPixelShader);
	SafeRelease(UberLightInputLayout);
	SafeRelease(ConstantBufferLightProperties);
	SafeRelease(UberLightSamplerState);
	SafeRelease(UberLightDepthState);
}

void URenderer::RenderUberLights(UCamera* InCurrentCamera, const D3D11_VIEWPORT& InViewport)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel) return;

	const auto& PointLights = CurrentLevel->GetAllPointLights();
	const auto& SpotLights = CurrentLevel->GetAllSpotLights();

	// 렌더링할 라이트가 없으면 리턴
	if (PointLights.empty() && SpotLights.empty())
	{
		return;
	}

	auto* Context = GetDeviceContext();

	// IMPORTANT: Scene RT 바인딩, 하지만 DSV는 nullptr!
	// Depth texture를 SRV로 읽어야 하므로 DSV로 바인딩하면 안됨
	// (DirectX 11: 같은 리소스를 DSV와 SRV로 동시에 사용 불가)
	ID3D11RenderTargetView* SceneRtvs[] = { SceneColorRTV };
	Context->OMSetRenderTargets(1, SceneRtvs, nullptr);  // DSV = nullptr
	Context->RSSetViewports(1, &InViewport);

	// Additive Blend + NO Depth Test (DSV가 nullptr이므로 depth test 불가능)
	Context->OMSetBlendState(AdditiveBlendState, nullptr, 0xFFFFFFFF);
	Context->OMSetDepthStencilState(DisabledDepthStencilState, 0);  // Depth 완전 비활성화

	// Pipeline 설정 (UberLight 셰이더 사용)
	FPipelineInfo PipelineInfo = {
		UberLightInputLayout,
		UberLightVertexShader,
		FRenderResourceFactory::GetRasterizerState({ ECullMode::None, EFillMode::Solid }),  // Culling 비활성화
		DisabledDepthStencilState,  // Depth 완전 비활성화 (DSV가 nullptr)
		UberLightPixelShader,
		AdditiveBlendState,
		D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	// Constant Buffer 유효성 확인
	if (!ConstantBufferLightProperties)
	{
		UE_LOG_ERROR("ConstantBufferLightProperties is nullptr! Skipping light rendering.");
		return;
	}

	// Constant Buffers 설정
	Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);
	Pipeline->SetConstantBuffer(2, false, ConstantBufferLightProperties);

	// Depth Texture 바인딩
	Context->PSSetShaderResources(0, 1, &SceneDepthSRV);
	Pipeline->SetSamplerState(0, false, UberLightSamplerState);

	// Inverse ViewProj Matrix 계산
	const FViewProjConstants& ViewProj = InCurrentCamera->GetFViewProjConstants();
	FMatrix ViewProjMatrix = ViewProj.View * ViewProj.Projection;
	FMatrix InvViewProj = ViewProjMatrix.Inverse();

	// Scene RT 크기 가져오기 (PostProcess와 동일)
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	GetSwapChain()->GetDesc(&swapChainDesc);

	// AssetManager에서 Sphere Mesh 가져오기 (PointLight와 SpotLight 공통 사용)
	UAssetManager& AssetMgr = UAssetManager::GetInstance();
	ID3D11Buffer* SphereVB = AssetMgr.GetVertexbuffer(EPrimitiveType::Sphere);
	ID3D11Buffer* SphereIB = AssetMgr.GetIndexbuffer(EPrimitiveType::Sphere);
	uint32 SphereNumIndices = AssetMgr.GetNumIndices(EPrimitiveType::Sphere);
	uint32 SphereNumVertices = AssetMgr.GetNumVertices(EPrimitiveType::Sphere);

	// ========== PointLight 렌더링 ==========
	for (auto* PointLight : PointLights)
	{
		if (!PointLight || !PointLight->GetOwner())
		{
			continue;
		}

		// Light Properties 설정
		FLightProperties lightProps = {};
		lightProps.LightPosition = PointLight->GetWorldLocation();
		lightProps.Intensity = PointLight->GetIntensity();
		lightProps.LightColor = PointLight->GetLightColor();
		lightProps.Radius = PointLight->GetRadius();
		lightProps.LightDirection = FVector::ZeroVector();  // PointLight는 방향 없음
		lightProps.RadiusFalloff = PointLight->GetRadiusFalloff();

		// Viewport 정보 (PostProcess와 동일한 방식)
		lightProps.ViewportTopLeft = FVector2(InViewport.TopLeftX, InViewport.TopLeftY);
		lightProps.ViewportSize = FVector2(InViewport.Width, InViewport.Height);
		lightProps.SceneRTSize = FVector2(
			static_cast<float>(swapChainDesc.BufferDesc.Width),
			static_cast<float>(swapChainDesc.BufferDesc.Height)
		);

		lightProps.InnerConeAngle = 0.0f;  // PointLight는 cone 없음
		lightProps.OuterConeAngle = 0.0f;
		lightProps.LightType = 0;  // 0 = PointLight
		lightProps.Padding3 = FVector::ZeroVector();

		lightProps.InvViewProj = InvViewProj;

		// Constant Buffer 업데이트
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLightProperties, lightProps);
		Pipeline->SetConstantBuffer(2, false, ConstantBufferLightProperties);

		// World Transform (Sphere를 라이트 위치/반경으로 스케일)
		FVector LightPos = PointLight->GetWorldLocation();
		FVector LightScale = FVector(PointLight->GetRadius(), PointLight->GetRadius(), PointLight->GetRadius());
		FMatrix WorldMatrix = FMatrix::GetModelMatrix(LightPos, FVector::ZeroVector(), LightScale);

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels, WorldMatrix);
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModels);

		// Sphere Mesh 렌더링
		Pipeline->SetVertexBuffer(SphereVB, sizeof(FNormalVertex));
		if (SphereIB)
		{
			Pipeline->SetIndexBuffer(SphereIB, 0);
			Pipeline->DrawIndexed(SphereNumIndices, 0, 0);
		}
		else
		{
			Pipeline->Draw(SphereNumVertices, 0);
		}
	}

	// ========== SpotLight 렌더링 ==========
	static int SpotLightDebugCount = 0;
	for (auto* SpotLight : SpotLights)
	{
		if (!SpotLight || !SpotLight->GetOwner())
		{
			if (SpotLightDebugCount < 3)
			{
				UE_LOG("RenderUberLights: SpotLight skipped (nullptr or no owner)");
				SpotLightDebugCount++;
			}
			continue;
		}

		if (SpotLightDebugCount < 3)
		{
			UE_LOG("RenderUberLights: Rendering SpotLight - Pos=(%.1f, %.1f, %.1f), Intensity=%.1f, Radius=%.1f",
				SpotLight->GetWorldLocation().X, SpotLight->GetWorldLocation().Y, SpotLight->GetWorldLocation().Z,
				SpotLight->GetIntensity(), SpotLight->GetRadius());
			SpotLightDebugCount++;
		}

		// Light Properties 설정
		FLightProperties lightProps = {};
		lightProps.LightPosition = SpotLight->GetWorldLocation();
		lightProps.Intensity = SpotLight->GetIntensity();
		lightProps.LightColor = SpotLight->GetLightColor();
		lightProps.Radius = SpotLight->GetRadius();

		// SpotLight 방향 (Rotation으로부터 Forward 벡터 계산)
		FVector Rotation = SpotLight->GetWorldRotation();

		// Euler Angles (Pitch, Yaw, Roll)를 Forward Vector로 변환
		// Forward = (cos(Yaw)*cos(Pitch), sin(Yaw)*cos(Pitch), sin(Pitch))
		float Pitch = FVector::GetDegreeToRadian(Rotation.X);
		float Yaw = FVector::GetDegreeToRadian(Rotation.Y);

		FVector LightDirection;
		LightDirection.X = cosf(Yaw) * cosf(Pitch);
		LightDirection.Y = sinf(Yaw) * cosf(Pitch);
		LightDirection.Z = sinf(Pitch);
		LightDirection.Normalize();

		lightProps.LightDirection = LightDirection;

		lightProps.RadiusFalloff = SpotLight->GetRadiusFalloff();

		// Viewport 정보
		lightProps.ViewportTopLeft = FVector2(InViewport.TopLeftX, InViewport.TopLeftY);
		lightProps.ViewportSize = FVector2(InViewport.Width, InViewport.Height);
		lightProps.SceneRTSize = FVector2(
			static_cast<float>(swapChainDesc.BufferDesc.Width),
			static_cast<float>(swapChainDesc.BufferDesc.Height)
		);

		// Cone Angles (도 단위 -> 라디안 변환)
		lightProps.InnerConeAngle = FVector::GetDegreeToRadian(SpotLight->GetInnerConeAngle());
		lightProps.OuterConeAngle = FVector::GetDegreeToRadian(SpotLight->GetOuterConeAngle());
		lightProps.LightType = 1;  // 1 = SpotLight
		lightProps.Padding3 = FVector::ZeroVector();

		lightProps.InvViewProj = InvViewProj;

		// Constant Buffer 업데이트
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLightProperties, lightProps);
		Pipeline->SetConstantBuffer(2, false, ConstantBufferLightProperties);

		// World Transform (Sphere를 라이트 위치/반경으로 스케일)
		FVector LightPos = SpotLight->GetWorldLocation();
		FVector LightScale = FVector(SpotLight->GetRadius(), SpotLight->GetRadius(), SpotLight->GetRadius());
		FMatrix WorldMatrix = FMatrix::GetModelMatrix(LightPos, FVector::ZeroVector(), LightScale);

		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels, WorldMatrix);
		Pipeline->SetConstantBuffer(0, true, ConstantBufferModels);

		// Sphere Mesh 렌더링
		Pipeline->SetVertexBuffer(SphereVB, sizeof(FNormalVertex));
		if (SphereIB)
		{
			Pipeline->SetIndexBuffer(SphereIB, 0);
			Pipeline->DrawIndexed(SphereNumIndices, 0, 0);
		}
		else
		{
			Pipeline->Draw(SphereNumVertices, 0);
		}
	}

	// SRV 언바인드
	ID3D11ShaderResourceView* NullSRV = nullptr;
	Context->PSSetShaderResources(0, 1, &NullSRV);

	// Scene RT 다시 바인딩 (RenderDebugPrimitives를 위해)
	Context->OMSetRenderTargets(1, SceneRtvs, SceneDepthDSV);

	// Blend/Depth State 복원
	Context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
	Context->OMSetDepthStencilState(DefaultDepthStencilState, 0);
}
