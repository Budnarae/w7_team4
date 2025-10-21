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
#include "Component/Light/Public/AmbientLightComponent.h"
#include "Component/Light/Public/DirectionalLightComponent.h"
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
#include "Render/RenderPass/Public/LightPass.h"
#include "Render/RenderPass/Public/IconPass.h"
#include "Render/RenderPass/Public/DebugPass.h"
#include "Render/RenderPass/Public/FogPass.h"
#include "Render/RenderPass/Public/SceneDepthPass.h"
#include "Render/RenderPass/Public/NormalPass.h"
#include "Render/RenderPass/Public/LightCullingPass.h"
#include "Render/RenderPass/Public/DepthPrePass.h"
#include "Render/RenderPass/Public/LightComplexityPass.h"

IMPLEMENT_SINGLETON_CLASS_BASE(URenderer)

struct FFullscreenQuadVertex
{
	FVector Position;
	float U, V;
};

URenderer::URenderer() = default;

URenderer::~URenderer() = default;

void URenderer::Init(HWND InWindowHandle)
{
	DeviceResources = new UDeviceResources(InWindowHandle);
	Pipeline = new UPipeline(GetDeviceContext());
	ViewportClient = new FViewport();

	// 셰이더 및 상태 리소스 생성
	CreateDepthStencilState();
	CreateBlendState();
	CreateDefaultShader();
	CreateTextureShader();
	CreateDecalShader();
	CreateDepthShader();
	CreateFireBallShader();
	CreateFireBallForwardShader();
	CreateIconResources();
	CreateUberLightResources();
	CreateFullscreenQuad();
	CreateConstantBuffers();
	CreateFogResources();
	CreateFXAAResources();
	CreateSceneDepthResources();
	CreateNormalResources();
	// CreateLightComplexityResources();

	// FontRenderer 초기화
	FontRenderer = new UFontRenderer;
	if (!FontRenderer->Initialize())
	{
		UE_LOG("FontRenderer 초기화 실패");
		SafeDelete(FontRenderer);
	}

	ViewportClient->InitializeLayout(DeviceResources->GetViewportInfo());

	// Scene RT는 ViewportClient 초기화 후에 생성 (올바른 크기 사용)
	CreateSceneRenderTargets();

	// Depth Pre-pass
	// Depth만 렌더링 (Light Culling을 위한 Depth 정보), Default 셰이더 사용
	// Depth만 쓰고 Color는 출력 안 함 (RTV = nullptr이므로 PS 출력 무시됨)
	DepthPrePass = new FDepthPrePass(
		Pipeline,
		SceneDepthDSV,
		ConstantBufferViewProj,
		ConstantBufferModels,
		DefaultVertexShader,
		DefaultPixelShader,
		DefaultInputLayout,
		DefaultDepthStencilState
	);
	RenderPasses.push_back(DepthPrePass);

	// Light Culling Pass
	// Depth 기반으로 Light Usage Mask 생성
	// SceneDepthTexture의 크기 사용=
	D3D11_TEXTURE2D_DESC DepthDesc = {};
	SceneDepthTexture->GetDesc(&DepthDesc);
	LightCullingPass = new FLightCullingPass(
		Pipeline,
		ConstantBufferViewProj,
		ConstantBufferLighting
	);
	LightCullingPass->CreateResources(DepthDesc.Width, DepthDesc.Height);
	RenderPasses.push_back(LightCullingPass);

	FStaticMeshPass* StaticMeshPass =
		new FStaticMeshPass(
			Pipeline,
			SceneColorRTV,
			SceneNormalRTV,
			SceneDepthDSV,
			ConstantBufferViewProj,
			ConstantBufferModelForLight,
			ConstantBufferLighting,
			TextureVertexShader,
			TexturePixelShader,
			TextureInputLayout,
			DebugLineDepthState
			);
	RenderPasses.push_back(StaticMeshPass);

	FPrimitivePass* PrimitivePass =
		new FPrimitivePass(
			Pipeline,
			SceneColorRTV,
			SceneNormalRTV,
			SceneDepthDSV,
			ConstantBufferViewProj,
			ConstantBufferModels,
			DefaultVertexShader,
			DefaultPixelShader,
			DefaultInputLayout,
			DebugLineDepthState
			);
	RenderPasses.push_back(PrimitivePass);

	// 알파 블렌딩을 사용하는 일반 데칼 패스
	FDecalPass* AlphaDecalPass =
		new FDecalPass(Pipeline, ConstantBufferViewProj, DecalVertexShader, DecalPixelShader,
		               DecalInputLayout, DecalDepthStencilState, AlphaBlendState, false);
	RenderPasses.push_back(AlphaDecalPass);

	// 가산 혼합을 사용하는 SemiLight 데칼 패스
	FDecalPass* AdditiveDecalPass =
		new FDecalPass(Pipeline, ConstantBufferViewProj, DecalVertexShader,
		               DecalPixelShader, DecalInputLayout, DecalDepthStencilState,
		               AdditiveBlendState, true);
	RenderPasses.push_back(AdditiveDecalPass);

	FFireBallPass* FireBallPass =
		new FFireBallPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		                  FireBallVertexShader, FireBallPixelShader, FireBallInputLayout,
		                  DecalDepthStencilState, FireBallBlendState);
	RenderPasses.push_back(FireBallPass);

	// Forward Rendering 사용으로 LightPass 비활성화 (Light는 StaticMeshPass에서 직접 처리)
	// FLightPass* LightPass =
	// 	new FLightPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
	// 	               SceneDepthSRV, SceneColorRTV, LightVertexShader, LightInputLayout,
	// 	               LightPointLightPS, LightSpotLightPS,
	// 	               LightSamplerState, LightDepthLessEqualNoWrite,
	// 	               LightAdditiveBlend);
	// RenderPasses.push_back(LightPass);

	FDebugPass* DebugPass =
		new FDebugPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels);
	RenderPasses.push_back(DebugPass);

	FBillboardPass* BillboardPass =
		new FBillboardPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels,
		                   TextureVertexShader, TexturePixelShader, TextureInputLayout,
		                   NoTestButWriteDepthState);
	RenderPasses.push_back(BillboardPass);

	FIconPass* IconPass = new FIconPass(
		Pipeline, ConstantBufferViewProj, ConstantBufferModels, ConstantBufferIconProperties,
		IconVertexShader, IconPixelShader, IconInputLayout, NoTestButWriteDepthState, AlphaBlendState);
	RenderPasses.push_back(IconPass);

	FTextPass* TextPass = new FTextPass(Pipeline, ConstantBufferViewProj, ConstantBufferModels);
	RenderPasses.push_back(TextPass);
	auto* BackBufferRTV = DeviceResources->GetRenderTargetView();
	auto* BackBufferDSV = DeviceResources->GetDepthStencilView();

	FFogPass* FogPass =
		new FFogPass(
			Pipeline,
			SceneColorRTV,
			SceneDepthSRV,
			PostProcessSamplerState,
			ConstantBufferViewProj,
			ConstantBufferModels,
			ConstantBufferFogProperties,
			FogVertexShader,
			FogPixelShader,
			DepthTestAlwaysNoWriteState,
			AlphaBlendState
		);
	RenderPasses.push_back(FogPass); // Forward 방식으로 RenderPasses에 추가

	SceneDepthPass = new FSceneDepthPass(
		Pipeline,
		SceneColorRTV,
		SceneDepthSRV,
		PostProcessSamplerState,
		SceneDepthVertexShader,
		SceneDepthPixelShader,
		DepthTestAlwaysNoWriteState,
		ConstantBufferSceneDepthProperties
	);

	NormalPass = new FNormalPass(
		Pipeline,
		SceneColorRTV,
		SceneNormalSRV,
		PostProcessSamplerState,
		SceneNormalVertexShader,
		SceneNormalPixelShader,
		DepthTestAlwaysNoWriteState,
		ConstantBufferNormalProperties
		);

	// // LightComplexityPass 생성
	// FLightComplexityPass* InLightComplexityPass = new FLightComplexityPass(
	// 	Pipeline,
	// 	SceneColorRTV,
	// 	PostProcessSamplerState,
	// 	LightComplexityVertexShader,
	// 	LightComplexityPixelShader,
	// 	DepthTestAlwaysNoWriteState,
	// 	AlphaBlendState, // 반투명 블랜딩
	// 	ConstantBufferLightComplexity
	// );
	// LightComplexityPass = InLightComplexityPass;

	// FXAAPass 생성
	FFXAAPass* InFXAAPass = new FFXAAPass(
		Pipeline,
		BackBufferRTV,
		BackBufferDSV,
		SceneColorSRV,
		SceneDepthSRV,
		PostProcessSamplerState,
		ConstantBufferViewProj,
		ConstantBufferModels,
		ConstantBufferFXAAParameters,
		FXAAVertexShader,
		FXAAPixelShader,
		NoTestButWriteDepthState
	);

	FXAAPass = InFXAAPass;
}

void URenderer::Release()
{
	ReleaseSceneRenderTargets();
	ReleaseConstantBuffers();
	ReleaseDefaultShader();
	ReleaseDepthShader();
	ReleaseFireBallShader();
	ReleaseFireBallForwardShader();
	ReleaseUberLightResources();
	ReleaseIconResources();
	ReleaseFullscreenQuad();
	ReleaseDepthStencilState();
	ReleaseBlendState();
	ReleaseFogResources();
	ReleaseFXAAResources();
	ReleaseSceneDepthResources();
	ReleaseNormalResources();
	// ReleaseLightComplexityResources();

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

	// No Test But Write Depth (Depth Test X, Depth Write O): 포스트프로세스용
	D3D11_DEPTH_STENCIL_DESC NoTestWriteDescription = {};
	NoTestWriteDescription.DepthEnable = TRUE; // Depth 활성화 (write를 위해)
	NoTestWriteDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // Depth write 활성화
	NoTestWriteDescription.DepthFunc = D3D11_COMPARISON_ALWAYS; // 항상 통과 (test 비활성화)
	NoTestWriteDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&NoTestWriteDescription, &NoTestButWriteDepthState);

	// Test Always No Write Depth (Depth Test Always, Depth Write X): Fog용
	D3D11_DEPTH_STENCIL_DESC TestAlwaysNoWriteDescription = {};
	TestAlwaysNoWriteDescription.DepthEnable = TRUE; // Depth 활성화
	TestAlwaysNoWriteDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Depth write 비활성화
	TestAlwaysNoWriteDescription.DepthFunc = D3D11_COMPARISON_ALWAYS; // 항상 통과
	TestAlwaysNoWriteDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&TestAlwaysNoWriteDescription, &DepthTestAlwaysNoWriteState);

	// Debug Line Depth State (Depth Test ON, Write OFF): BatchLines용
	D3D11_DEPTH_STENCIL_DESC DebugLineDescription = {};
	DebugLineDescription.DepthEnable = TRUE;
	DebugLineDescription.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // Write OFF
	DebugLineDescription.DepthFunc = D3D11_COMPARISON_LESS_EQUAL; // Test ON
	DebugLineDescription.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&DebugLineDescription, &DebugLineDepthState);
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
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/SampleShader.hlsl",
		DefaultLayout,
		&DefaultVertexShader,
		&DefaultInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/SampleShader.hlsl",
	                                          &DefaultPixelShader);
	Stride = sizeof(FNormalVertex);
}

void URenderer::CreateDecalShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DecalLayout =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/DecalShader.hlsl", DecalLayout, &DecalVertexShader, &DecalInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/DecalShader.hlsl", &DecalPixelShader);
}

void URenderer::CreateTextureShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> TextureLayout =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/TextureShader.hlsl", TextureLayout, &TextureVertexShader,
		&TextureInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/TextureShader.hlsl",
	                                          &TexturePixelShader);

	// Lit Shaders
	// Vertex Shader Layout (Normal Mapping 비사용 UberLit 모드에서 공통 사용)
	TArray<D3D11_INPUT_ELEMENT_DESC> UberLitLayout =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	// Vertex Shader Layout (Normal Mapping 사용 UberLit 모드에서 공통 사용)
	TArray<D3D11_INPUT_ELEMENT_DESC> UberLitNormalMappingLayout =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalMapping, NormalVertex.Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalMapping, NormalVertex.Normal), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalMapping, NormalVertex.Color), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalMapping, NormalVertex.TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0},
		// TBN 행렬의 세 행 (각각 float3)
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalMapping, Tangent), D3D11_INPUT_PER_VERTEX_DATA, 0},  // Tangent
		{"TANGENT", 1, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalMapping, BiTangent), D3D11_INPUT_PER_VERTEX_DATA, 0},  // Bitangent
		{"TANGENT", 2, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalMapping, TangentNormal), D3D11_INPUT_PER_VERTEX_DATA, 0}  // TBNNormal
	};

	/*ID3D11VertexShader* UberLitNormalMappingVertexShader = nullptr;
	ID3D11InputLayout* UberLitNormalMappingInputLayout = nullptr;
	ID3D11VertexShader* UberRambertNormalMappingPixelShader = nullptr;
	ID3D11VertexShader* UberPhongNormalMappingPixelShader = nullptr;*/

	// (VS에서 라이팅 계산. Normal_Mapping을 위해 TBN을 조합하여 PS로 넘겨줌)
	D3D_SHADER_MACRO DefinesNormalMappingVS[] = {
		{"NORMAL_MAPPING", "1"},
		{nullptr, nullptr}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
			L"Asset/Shader/UberLit.hlsl", UberLitNormalMappingLayout, DefinesNormalMappingVS,
			&UberLitNormalMappingVertexShader, &UberLitNormalMappingInputLayout);

	// Lambert Lit Normal Mapping Pixel Shader
	D3D_SHADER_MACRO DefinesLambertNormalMappingPS[] = {
		{"LIGHTING_MODEL_LAMBERT", "1"},
		{"NORMAL_MAPPING", "1"},
		{nullptr, nullptr}
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", DefinesLambertNormalMappingPS,
											  &UberLambertNormalMappingPixelShader);

	// Blinn-Phong Lit Normal Mapping Pixel Shader
	D3D_SHADER_MACRO DefinesPhongNormalMappingPS[] = {
		{"LIGHTING_MODEL_PHONG", "1"},
		{"NORMAL_MAPPING", "1"},
		{nullptr, nullptr}
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", DefinesPhongNormalMappingPS,
											  &UberPhongNormalMappingPixelShader);

	// Gouraud Vertex Shader (VS에서 라이팅 계산)
	D3D_SHADER_MACRO DefinesGouraudVS[] = {
		{"LIGHTING_MODEL_GOURAUD", "1"},
		{nullptr, nullptr}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/UberLit.hlsl", UberLitLayout, DefinesGouraudVS,
		&UberLitGouraudVertexShader, &UberLitGouraudInputLayout);

	// Lambert/Phong Vertex Shader (PS에서 라이팅 계산, VS는 변환만)
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/UberLit.hlsl", UberLitLayout, &UberLitVertexShader, &UberLitInputLayout);

	// Unlit Pixel Shader
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", &TextureUnlitPixelShader);

	// Lambert Lit Pixel Shader
	D3D_SHADER_MACRO DefinesLambert[] = {
		{"LIGHTING_MODEL_LAMBERT", "1"},
		{nullptr, nullptr}
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", DefinesLambert,
	                                          &TextureLambertPixelShader);

	// Gouraud Shading Pixel Shader
	D3D_SHADER_MACRO DefinesGouraud[] = {
		{"LIGHTING_MODEL_GOURAUD", "1"},
		{nullptr, nullptr}
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", DefinesGouraud,
	                                          &TextureGouraudPixelShader);

	// Blinn-Phong Shading Pixel Shader
	D3D_SHADER_MACRO DefinesPhong[] = {
		{"LIGHTING_MODEL_PHONG", "1"},
		{nullptr, nullptr}
	};
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", DefinesPhong,
	                                          &TexturePhongPixelShader);
}

void URenderer::CreateDepthShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> DepthLayout =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/DepthShader.hlsl", DepthLayout, &DepthVertexShader, &DepthInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/DepthShader.hlsl", &DepthPixelShader);
}

void URenderer::CreateConstantBuffers()
{
	ConstantBufferModels = FRenderResourceFactory::CreateConstantBuffer<FMatrix>();
	ConstantBufferViewProj = FRenderResourceFactory::CreateConstantBuffer<FViewProjConstants>();
	ConstantBufferColor = FRenderResourceFactory::CreateConstantBuffer<FVector4>();
	ConstantBufferLighting = FRenderResourceFactory::CreateConstantBuffer<FLightingConstants>();

	ConstantBufferModelForLight = FRenderResourceFactory::CreateConstantBuffer<FModelForLight>();
}

void URenderer::ReleaseConstantBuffers()
{
	SafeRelease(ConstantBufferModels);
	SafeRelease(ConstantBufferViewProj);
	SafeRelease(ConstantBufferColor);
	SafeRelease(ConstantBufferLighting);

	SafeRelease(ConstantBufferModelForLight);
}

void URenderer::ReleaseDefaultShader()
{
	SafeRelease(DefaultInputLayout);
	SafeRelease(DefaultPixelShader);
	SafeRelease(DefaultVertexShader);
	SafeRelease(TextureInputLayout);
	SafeRelease(TexturePixelShader);
	SafeRelease(TextureVertexShader);

	SafeRelease(UberLitInputLayout);
	SafeRelease(UberLitVertexShader);
	SafeRelease(UberLitGouraudInputLayout);
	SafeRelease(UberLitGouraudVertexShader);
	SafeRelease(TextureLambertPixelShader);
	SafeRelease(TextureGouraudPixelShader);
	SafeRelease(TexturePhongPixelShader);
	SafeRelease(TextureUnlitPixelShader);

	// Normal Mapping Shaders
	SafeRelease(UberLitNormalMappingVertexShader);
	SafeRelease(UberLitNormalMappingInputLayout);
	SafeRelease(UberLambertNormalMappingPixelShader);
	SafeRelease(UberPhongNormalMappingPixelShader);

	SafeRelease(DecalVertexShader);
	SafeRelease(DecalPixelShader);
}

void URenderer::ReleaseIconResources()
{
	SafeRelease(IconVertexShader);
	SafeRelease(IconPixelShader);
	SafeRelease(IconInputLayout);
	SafeRelease(ConstantBufferIconProperties);
}

void URenderer::ReleaseFogResources()
{
	SafeRelease(FogVertexShader);
	SafeRelease(FogPixelShader);
	SafeRelease(ConstantBufferFogProperties);
	SafeRelease(PostProcessSamplerState);
}

void URenderer::ReleaseFXAAResources()
{
	SafeRelease(FXAAVertexShader);
	SafeRelease(FXAAPixelShader);
	SafeRelease(ConstantBufferFXAAParameters);
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
	SafeRelease(DepthTestAlwaysNoWriteState);
	SafeRelease(DebugLineDepthState);
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
		if (ViewportClient.GetViewportInfo().Width < 1.0f || ViewportClient.GetViewportInfo().Height
			< 1.0f) { continue; }

		UCamera* CurrentCamera = &ViewportClient.Camera;
		CurrentCamera->Update(ViewportClient.GetViewportInfo());
		FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferViewProj,
		                                                 CurrentCamera->GetFViewProjConstants());
		Pipeline->SetConstantBuffer(1, true, ConstantBufferViewProj);

		const D3D11_VIEWPORT& ClientViewport = ViewportClient.GetViewportInfo();

		// === Scene RT 렌더링: clientViewport과 동일한 viewport 사용 ===
		// Scene RT는 SwapChain 전체 크기로 생성되었으므로,
		// 각 ViewportClient의 TopLeftX/Y를 그대로 사용하여 해당 영역에 렌더링

		// IMPORTANT: 각 viewport마다 Scene RT를 다시 바인딩
		// (이전 viewport의 post-processing이 BackBuffer로 바인딩을 변경했으므로)
		ID3D11RenderTargetView* SceneRtvs[] = {SceneColorRTV, SceneNormalRTV};
		GetDeviceContext()->OMSetRenderTargets(2, SceneRtvs, SceneDepthDSV);
		GetDeviceContext()->RSSetViewports(1, &ClientViewport);

		{
			TIME_PROFILE(RenderLevel)
			RenderLevel(CurrentCamera, ClientViewport);
		}

		// === 오버레이 프리미티브 렌더링: 이제 DebugPass에서 처리됨 ===
		// RenderDebugPrimitives()는 DebugPass에서 RenderLevel 내부에서 호출되므로 여기서는 제거

		// === Post-Processing: Scene RT -> 백버퍼 ===
		// 통합 포스트프로세싱 패스: Fog + Anti-Aliasing (FXAA)
		// RenderLevel + RenderDebugPrimitives 결과에 모두 FXAA 적용
		GetDeviceContext()->RSSetViewports(1, &ClientViewport);
		// {
		// 	TIME_PROFILE(ExecutePostProcess)
		// 	ExecutePostProcess(CurrentCamera, ClientViewport); // Fog + FXAA 통합
		// }
		//FogPass->Execute(RenderingContext);

		// Post-Process 오버레이
		if (RenderingContext.ViewMode == EViewModeIndex::VMI_SceneDepth)
		{
			// SceneDepth 모드: Depth 시각화로 덮어씀
			SceneDepthPass->Execute(RenderingContext);
		}
		else if (RenderingContext.ViewMode == EViewModeIndex::VMI_Normal)
		{
			// Normal 모드: Normal 시각화
			NormalPass->Execute(RenderingContext);
		}
		// else if (RenderingContext.ViewMode == EViewModeIndex::VMI_LightComplexity)
		// {
		// 	// Light Complexity 모드: 메시 렌더링 위에 반투명 heat map 오버레이
		// 	LightComplexityPass->Execute(RenderingContext);
		// }

		// LightPass가 DSV를 nullptr로 설정했을 수 있으므로 Scene RT 재바인딩
		//GetDeviceContext()->OMSetRenderTargets(1, , );

		FXAAPass->Execute(RenderingContext);

		// === 기즈모 렌더링: BackBuffer에 직접 렌더링 (FXAA 미적용, 항상 선명) ===
		{
			TIME_PROFILE(RenderGizmo)
			GEditor->GetEditorModule()->RenderGizmo(CurrentCamera);
		}

		// === Axis Widget 렌더링: 기즈모 이후, 뷰포트 좌하단에 고정 ===
		{
			TIME_PROFILE(RenderAxisWidget)
			GEditor->GetEditorModule()->RenderAxis(CurrentCamera, ClientViewport);
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
	// BackBuffer 정리 (post-processing 결과를 받을 곳)
	auto* BackBufferRTV = DeviceResources->GetRenderTargetView();
	auto* BackBufferDSV = DeviceResources->GetDepthStencilView();
	GetDeviceContext()->ClearRenderTargetView(BackBufferRTV, ClearColor);
	GetDeviceContext()->ClearDepthStencilView(BackBufferDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	// Scene RT 정리 및 바인딩 (Scene Color + Scene Normal + Scene Depth)
	// 이후 각 ViewportClient가 Scene RT의 해당 영역에 렌더링함
	GetDeviceContext()->ClearRenderTargetView(SceneColorRTV, ClearColor);
	GetDeviceContext()->ClearRenderTargetView(SceneNormalRTV, ClearColor);
	GetDeviceContext()->ClearDepthStencilView(SceneDepthDSV, D3D11_CLEAR_DEPTH, 1.0f, 0);

	ID3D11RenderTargetView* SceneRtvs[] = {SceneColorRTV, SceneNormalRTV};
	GetDeviceContext()->OMSetRenderTargets(2, SceneRtvs, SceneDepthDSV);

	DeviceResources->UpdateViewport();
}

void URenderer::RenderLevel(UCamera* InCurrentCamera, const D3D11_VIEWPORT& InViewport)
{
	const ULevel* CurrentLevel = GWorld->GetLevel();
	if (!CurrentLevel) { return; }

	const FViewProjConstants& ViewProj = InCurrentCamera->GetFViewProjConstants();
	TArray<UPrimitiveComponent*> FinalVisiblePrims = InCurrentCamera->GetViewVolumeCuller().
	                                                                  GetRenderableObjects();

	// // 오클루전 컬링 수행
	// TIME_PROFILE(Occlusion)
	// static COcclusionCuller Culler;
	// Culler.InitializeCuller(ViewProj.View, ViewProj.Projection);
	// FinalVisiblePrims = Culler.PerformCulling(
	// 	InCurrentCamera->GetViewVolumeCuller().GetRenderableObjects(),
	// 	InCurrentCamera->GetLocation()
	// );
	// TIME_PROFILE_END(Occlusion)


	RenderingContext = FRenderingContext(&ViewProj, InCurrentCamera,
	                                     GEditor->GetEditorModule()->GetViewMode(),
	                                     CurrentLevel->GetShowFlags());
	RenderingContext.AllPrimitives = FinalVisiblePrims;

	// LightPass용 추가 정보 설정
	RenderingContext.Viewport = InViewport;
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	GetSwapChain()->GetDesc(&swapChainDesc);
	RenderingContext.SceneRTSize = FVector2(
		static_cast<float>(swapChainDesc.BufferDesc.Width),
		static_cast<float>(swapChainDesc.BufferDesc.Height)
	);

	for (auto& Prim : FinalVisiblePrims)
	{
		if (auto StaticMesh = Cast<UStaticMeshComponent>(Prim))
		{
			RenderingContext.StaticMeshes.push_back(StaticMesh);
		}
		else if (auto Icon = Cast<UIconComponent>(Prim))
		{
			// PIE가 아닐 때에만 아이콘 렌더링
			if (!(GEditor->GetPIEState() == EPIEState::Playing))
				RenderingContext.Icons.push_back(Icon);
			// IconComponent는 BillBoardComponent 상속이므로 여기서 continue
		}
		else if (auto BillBoard = Cast<UBillBoardComponent>(Prim))
		{
			// IconComponent가 아닌 순수 BillBoardComponent만 렌더링
			RenderingContext.BillBoards.push_back(BillBoard);
		}
		else if (auto FireBall = Cast<UFireBallComponent>(Prim))
		{
			RenderingContext.FireBalls.push_back(FireBall);
		}
		// PointLight는 이제 Level::GetAllPointLights()로 직접 가져옴 (w6_team6 방식)
		else if (auto Text = Cast<UTextComponent>(Prim); Text && !Text->IsExactly(
			UUUIDTextComponent::StaticClass()))
		{
			RenderingContext.Texts.push_back(Text);
		}
		else if (!Prim->IsA(UUUIDTextComponent::StaticClass()))
		{
			RenderingContext.DefaultPrimitives.push_back(Prim);
		}
	}

	// HeightFog는 USceneComponent이므로 Primitive 시스템에 등록되지 않음
	// Level의 모든 Actor를 순회하며 직접 수집 (Decal/PointLight 방식과 동일)
	const bool bShowFog = (CurrentLevel->GetShowFlags() & EEngineShowFlags::SF_Fog) != 0;
	if (bShowFog)
	{
		for (AActor* Actor : CurrentLevel->GetActors())
		{
			if (!Actor) continue;
			for (UActorComponent* Component : Actor->GetOwnedComponents())
			{
				if (auto Fog = Cast<UHeightFogComponent>(Component))
				{
					if (Fog->IsEnabled())
					{
						RenderingContext.Fogs.push_back(Fog);
					}
				}
			}
		}
	}
	// 편집 씬에 데칼 표시
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

	// Lighting 정보 수집 및 cbuffer 업데이트
	FLightingConstants LightingData = {};
	LightingData.PointLightUsageMask = 0xFFFFFFFF;
	LightingData.SpotLightUsageMask = 0xFFFFFFFF;

	// Ambient Light 수집
	const TArray<UAmbientLightComponent*>& AmbientLights = CurrentLevel->GetAllAmbientLights();
	if (!AmbientLights.empty())
	{
		// 첫 번째 활성화된 Ambient Light 사용
		for (auto* AmbientLight : AmbientLights)
		{
			if (AmbientLight && AmbientLight->IsVisible())
			{
				LightingData.Ambient.Color = AmbientLight->GetLightColor();
				LightingData.Ambient.Intensity = AmbientLight->GetIntensity();
				break;
			}
		}
	}

	// Directional Light 수집
	const TArray<UDirectionalLightComponent*>& DirectionalLights = CurrentLevel->GetAllDirectionalLights();
	if (!DirectionalLights.empty())
	{
		// 첫 번째 활성화된 Directional Light 사용
		for (auto* DirectionalLight : DirectionalLights)
		{
			if (DirectionalLight && DirectionalLight->IsVisible())
			{
				// Directional Light 방향 계산 (Rotation → Forward Vector)
				const FVector Rotation = DirectionalLight->GetWorldRotation();
				const FMatrix RotationMatrix = FMatrix::RotationMatrix(FVector::GetDegreeToRadian(Rotation));
				const FVector4 Forward4 = FVector4::ForwardVector() * RotationMatrix;
				LightingData.Directional.Direction = FVector(Forward4.X, Forward4.Y, Forward4.Z);
				LightingData.Directional.Direction.Normalize();

				LightingData.Directional.Color = DirectionalLight->GetLightColor();
				LightingData.Directional.Intensity = DirectionalLight->GetIntensity();
				break;
			}
		}
	}

	// Point Light 데이터 채우기
	const TArray<UPointLightComponent*>& PointLights = CurrentLevel->GetAllPointLights();
	uint32 PointLightIndex = 0;
	for (auto* PointLight : PointLights)
	{
		if (PointLightIndex >= MAX_POINT_LIGHTS)
		{
			break;
		}

		if (PointLight && PointLight->IsVisible())
		{
			FPointLightInfo& LightInfo = LightingData.PointLights[PointLightIndex];
			LightInfo.Position = PointLight->GetWorldLocation();
			LightInfo.Color = PointLight->GetLightColor();
			LightInfo.Intensity = PointLight->GetIntensity();
			LightInfo.Radius = PointLight->GetAttenuationRadius();
			LightInfo.Falloff = PointLight->GetLightFalloffExponent();
			++PointLightIndex;
		}
	}
	LightingData.NumActivePointLights = PointLightIndex;

	// Spot Light 데이터 채우기
	const TArray<USpotLightComponent*>& SpotLights = CurrentLevel->GetAllSpotLights();
	uint32 SpotLightIndex = 0;
	for (auto* SpotLight : SpotLights)
	{
		if (SpotLightIndex >= MAX_SPOT_LIGHTS)
		{
			break;
		}

		if (SpotLight && SpotLight->IsVisible())
		{
			FSpotLightInfo& LightInfo = LightingData.SpotLights[SpotLightIndex];
			LightInfo.Position = SpotLight->GetWorldLocation();
			LightInfo.Color = SpotLight->GetLightColor();
			LightInfo.Intensity = SpotLight->GetIntensity();
			LightInfo.Radius = SpotLight->GetAttenuationRadius();
			LightInfo.Falloff = SpotLight->GetLightFalloffExponent();

			// SpotLight 방향 계산 (Rotation → Forward Vector)
			const FVector Rotation = SpotLight->GetWorldRotation();
			const FMatrix RotationMatrix = FMatrix::RotationMatrix(FVector::GetDegreeToRadian(Rotation));
			const FVector4 Forward4 = FVector4::ForwardVector() * RotationMatrix;
			LightInfo.Direction = FVector(Forward4.X, Forward4.Y, Forward4.Z);
			LightInfo.Direction.Normalize();

			// Cone 각도를 Radian으로 변환하여 전달 (HLSL cos() 함수는 Radian 요구)
			LightInfo.InnerConeAngle = FVector::GetDegreeToRadian(SpotLight->GetInnerConeAngle());
			LightInfo.OuterConeAngle = FVector::GetDegreeToRadian(SpotLight->GetOuterConeAngle());
			++SpotLightIndex;
		}
	}
	LightingData.NumActiveSpotLights = SpotLightIndex;

	// RenderingContext에 LightingData 설정
	RenderingContext.LightingData = &LightingData;

	// Lighting Constant Buffer 초기 업데이트 (모든 Light 활성화 상태)
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLighting, LightingData);

	// ViewMode에 따라 실행할 Pass 선택
	const EViewModeIndex ViewMode = RenderingContext.ViewMode;

	// Scene Depth 모드: DepthPrePass만 실행
	if (ViewMode == EViewModeIndex::VMI_SceneDepth)
	{
		for (auto RenderPass : RenderPasses)
		{
			if (RenderPass == DepthPrePass)
			{
				RenderPass->Execute(RenderingContext);
			}
		}
	}
	// 일반 렌더링 모드: 모든 Pass 실행
	else
	{
		for (auto RenderPass : RenderPasses)
		{
			RenderPass->Execute(RenderingContext);
		}
	}

	// Light Culling 통계를 StatOverlay에 전달 (RenderPasses 실행 후)
	uint32 ActivePointCount = 0;
	uint32 ActiveSpotCount = 0;

	for (uint32 i = 0; i < LightingData.NumActivePointLights; ++i)
	{
		if (LightingData.PointLightUsageMask & (1u << i))
		{
			++ActivePointCount;
		}
	}

	for (uint32 i = 0; i < LightingData.NumActiveSpotLights; ++i)
	{
		if (LightingData.SpotLightUsageMask & (1u << i))
		{
			++ActiveSpotCount;
		}
	}

	UStatOverlay::GetInstance().RecordLightCullingStats(
		LightingData.NumActivePointLights,
		LightingData.NumActiveSpotLights,
		ActivePointCount,
		ActiveSpotCount,
		LightingData.PointLightUsageMask,
		LightingData.SpotLightUsageMask
	);
}

void URenderer::RenderEditorPrimitive(const FEditorPrimitive& InPrimitive,
                                      const FRenderState& InRenderState, uint32 InStride,
                                      uint32 InIndexBufferStride)
{
	// Use the global stride if InStride is 0
	const uint32 FinalStride = (InStride == 0) ? Stride : InStride;

	// Depth Stencil State 선택
	ID3D11DepthStencilState* DepthState = DefaultDepthStencilState;
	if (InPrimitive.bShouldAlwaysVisible)
	{
		DepthState = DisabledDepthStencilState; // Depth Test OFF
	}
	else if (InPrimitive.bDepthWriteDisabled)
	{
		DepthState = DebugLineDepthState; // Depth Test ON (LESS_EQUAL), Write OFF
	}

	// Allow for custom shaders, fallback to default
	// BatchLines 등 에디터 프리미티브는 AlphaBlendState 사용 (Icon 이후 상태 복원)
	FPipelineInfo PipelineInfo = {
		InPrimitive.InputLayout ? InPrimitive.InputLayout : DefaultInputLayout,
		InPrimitive.VertexShader ? InPrimitive.VertexShader : DefaultVertexShader,
		FRenderResourceFactory::GetRasterizerState(InRenderState),
		DepthState,
		InPrimitive.PixelShader ? InPrimitive.PixelShader : DefaultPixelShader,
		AlphaBlendState,
		InPrimitive.Topology
	};
	Pipeline->UpdatePipeline(PipelineInfo);

	// Update constant buffers
	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferModels,
	                                                 FMatrix::GetModelMatrix(
		                                                 InPrimitive.Location,
		                                                 FVector::GetDegreeToRadian(
			                                                 InPrimitive.Rotation),
		                                                 InPrimitive.Scale));
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

	// Recreate LightCullingPass resources with new size
	if (LightCullingPass)
	{
		LightCullingPass->ReleaseResources();
		LightCullingPass->CreateResources(InWidth, InHeight);
	}

	auto* RenderTargetView = DeviceResources->GetRenderTargetView();
	ID3D11RenderTargetView* RenderTargetViews[] = {RenderTargetView};
	GetDeviceContext()->OMSetRenderTargets(1, RenderTargetViews,
	                                       DeviceResources->GetDepthStencilView());
}


void URenderer::CreateFireBallShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> FireBallLayout =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
	};

	// Use default factory entries (mainVS/mainPS)
	// Viewport-corrected version of FireBall shader
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/FireBallShaderFixed.hlsl", FireBallLayout, &FireBallVertexShader,
		&FireBallInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/FireBallShaderFixed.hlsl",
	                                          &FireBallPixelShader);

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
	FFullscreenQuadVertex Vertices[] =
	{
		{FVector(-1.0f, 1.0f, 0.0f), 0.0f, 0.0f}, // Top-left
		{FVector(1.0f, 1.0f, 0.0f), 1.0f, 0.0f}, // Top-right
		{FVector(-1.0f, -1.0f, 0.0f), 0.0f, 1.0f}, // Bottom-left
		{FVector(1.0f, -1.0f, 0.0f), 1.0f, 1.0f} // Bottom-right
	};

	uint32 Indices[] = {0, 1, 2, 2, 1, 3};

	D3D11_BUFFER_DESC VertexBufferDescription = {};
	VertexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
	VertexBufferDescription.ByteWidth = sizeof(Vertices);
	VertexBufferDescription.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VertexInitialData = {};
	VertexInitialData.pSysMem = Vertices;
	GetDevice()->CreateBuffer(&VertexBufferDescription, &VertexInitialData, &FullscreenQuadVB);

	D3D11_BUFFER_DESC IndexBufferDescription = {};
	IndexBufferDescription.Usage = D3D11_USAGE_IMMUTABLE;
	IndexBufferDescription.ByteWidth = sizeof(Indices);
	IndexBufferDescription.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IndexInitialData = {};
	IndexInitialData.pSysMem = Indices;
	GetDevice()->CreateBuffer(&IndexBufferDescription, &IndexInitialData, &FullscreenQuadIB);
}

void URenderer::ReleaseFullscreenQuad()
{
	SafeRelease(FullscreenQuadVB);
	SafeRelease(FullscreenQuadIB);
}

void URenderer::CreateSceneRenderTargets()
{
	// Scene RT는 SwapChain 전체 크기로 생성 (4분할 viewport 지원)
	// 각 ViewportClient는 Scene RT의 해당 영역에 렌더링됨
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	GetSwapChain()->GetDesc(&swapChainDesc);
	uint32 Width = swapChainDesc.BufferDesc.Width;
	uint32 Height = swapChainDesc.BufferDesc.Height;

	if (Width == 0 || Height == 0)
	{
		UE_LOG("CreateSceneRenderTargets: Invalid SwapChain size %ux%u", Width, Height);
		return;
	}

	UE_LOG("CreateSceneRenderTargets: Creating %ux%u textures (SwapChain full size)", Width,
	       Height);

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
	GetDevice()->CreateTexture2D(&ColorDescription, nullptr, &SceneNormalTexture);

	// RTV 생성 (씬 렌더링용)
	D3D11_RENDER_TARGET_VIEW_DESC RTVDescription = {};
	RTVDescription.Format = ColorDescription.Format;
	RTVDescription.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	RTVDescription.Texture2D.MipSlice = 0;
	GetDevice()->CreateRenderTargetView(SceneColorTexture, &RTVDescription, &SceneColorRTV);
	GetDevice()->CreateRenderTargetView(SceneNormalTexture, &RTVDescription, &SceneNormalRTV);

	// SRV 생성 (포스트프로세스에서 읽기용)
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDescription = {};
	SRVDescription.Format = ColorDescription.Format;
	SRVDescription.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDescription.Texture2D.MostDetailedMip = 0;
	SRVDescription.Texture2D.MipLevels = 1;
	GetDevice()->CreateShaderResourceView(SceneColorTexture, &SRVDescription, &SceneColorSRV);
	GetDevice()->CreateShaderResourceView(SceneNormalTexture, &SRVDescription, &SceneNormalSRV);

	// Scene Depth Texture (SRV 지원)
	D3D11_TEXTURE2D_DESC DepthDescription = {};
	DepthDescription.Width = Width;
	DepthDescription.Height = Height;
	DepthDescription.MipLevels = 1;
	DepthDescription.ArraySize = 1;
	DepthDescription.Format = DXGI_FORMAT_R24G8_TYPELESS; // Typeless로 생성 (DSV와 SRV 모두 지원)
	DepthDescription.SampleDesc.Count = 1;
	DepthDescription.SampleDesc.Quality = 0;
	DepthDescription.Usage = D3D11_USAGE_DEFAULT;
	DepthDescription.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	DepthDescription.CPUAccessFlags = 0;
	DepthDescription.MiscFlags = 0;

	GetDevice()->CreateTexture2D(&DepthDescription, nullptr, &SceneDepthTexture);

	// DSV 생성 (Depth 쓰기용)
	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDescription = {};
	DSVDescription.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // Depth 24비트 + Stencil 8비트
	DSVDescription.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDescription.Texture2D.MipSlice = 0;
	GetDevice()->CreateDepthStencilView(SceneDepthTexture, &DSVDescription, &SceneDepthDSV);

	// SRV 생성 (Depth 읽기용 - Stencil은 무시)
	D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc = {};
	depthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS; // Depth만 읽기, Stencil 무시
	depthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	depthSRVDesc.Texture2D.MostDetailedMip = 0;
	depthSRVDesc.Texture2D.MipLevels = 1;
	GetDevice()->CreateShaderResourceView(SceneDepthTexture, &depthSRVDesc, &SceneDepthSRV);

	// Read Only DSV
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvRO = {};
	dsvRO.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvRO.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvRO.Texture2D.MipSlice = 0;
	dsvRO.Flags = D3D11_DSV_READ_ONLY_DEPTH; // 필요 시 | D3D11_DSV_READ_ONLY_STENCIL
	GetDevice()->CreateDepthStencilView(SceneDepthTexture, &dsvRO, &SceneDepthDSV_ReadOnly);

	UE_LOG("Scene Render Targets Created: %ux%u", Width, Height);
}

void URenderer::ReleaseSceneRenderTargets()
{
	SafeRelease(SceneColorSRV);
	SafeRelease(SceneColorRTV);
	SafeRelease(SceneColorTexture);

	SafeRelease(SceneNormalSRV);
	SafeRelease(SceneNormalRTV);
	SafeRelease(SceneNormalTexture);

	SafeRelease(SceneDepthSRV);
	SafeRelease(SceneDepthDSV);
	SafeRelease(SceneDepthTexture);
}

void URenderer::CreateSceneDepthResources()
{
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/SceneDepthShader.hlsl",
		TArray<D3D11_INPUT_ELEMENT_DESC>{},
		&SceneDepthVertexShader,
		nullptr
	);

	FRenderResourceFactory::CreatePixelShader(
		L"Asset/Shader/SceneDepthShader.hlsl",
		&SceneDepthPixelShader
	);

	ConstantBufferSceneDepthProperties =
		FRenderResourceFactory::CreateConstantBuffer<FSceneDepthParameters>();
}

void URenderer::ReleaseSceneDepthResources()
{
	SafeRelease(SceneDepthVertexShader);
	SafeRelease(SceneDepthPixelShader);
	SafeRelease(ConstantBufferSceneDepthProperties);
}

void URenderer::CreateNormalResources()
{
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/NormalShader.hlsl",
		TArray<D3D11_INPUT_ELEMENT_DESC>{},
		&SceneNormalVertexShader,
		nullptr
	);

	FRenderResourceFactory::CreatePixelShader(
		L"Asset/Shader/NormalShader.hlsl",
		&SceneNormalPixelShader
	);

	ConstantBufferNormalProperties =
		FRenderResourceFactory::CreateConstantBuffer<FNormalParameters>();
}

void URenderer::ReleaseNormalResources()
{
	SafeRelease(SceneNormalVertexShader);
	SafeRelease(SceneNormalPixelShader);
	SafeRelease(ConstantBufferNormalProperties);
}

// void URenderer::ReleaseLightComplexityResources()
// {
// 	SafeRelease(LightComplexityVertexShader);
// 	SafeRelease(LightComplexityPixelShader);
// 	SafeRelease(ConstantBufferLightComplexity);
// }

void URenderer::CreateFireBallForwardShader()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> layout =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/FireBallForward.hlsl", layout, &FireBallFwdVertexShader,
		&FireBallFwdInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/FireBallForward.hlsl",
	                                          &FireBallFwdPixelShader);
}

void URenderer::ReleaseFireBallForwardShader()
{
	SafeRelease(FireBallFwdVertexShader);
	SafeRelease(FireBallFwdPixelShader);
	SafeRelease(FireBallFwdInputLayout);
}

// ========================================
// UberLight Resources (LightPass용)
// ========================================

void URenderer::CreateUberLightResources()
{
	// Vertex Shader & Input Layout
	TArray<D3D11_INPUT_ELEMENT_DESC> Layout =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/UberLit.hlsl",
		Layout,
		&LightVertexShader,
		&LightInputLayout
	);

	// PointLight Pixel Shader
	D3D_SHADER_MACRO DefinesPointLight[] = {
		{"LIGHT_TYPE_POINT", "1"},
		{nullptr, nullptr}
	};
	FRenderResourceFactory::CreatePixelShader(
		L"Asset/Shader/UberLit.hlsl",
		DefinesPointLight,
		&LightPointLightPS);

	if (!LightPointLightPS)
	{
		UE_LOG("Renderer: Failed to create UberLightPointLightPS");
	}

	// SpotLight Pixel Shader
	D3D_SHADER_MACRO DefinesSpotLight[] = {
		{"LIGHT_TYPE_SPOT", "1"},
		{nullptr, nullptr}
	};
	FRenderResourceFactory::CreatePixelShader(
		L"Asset/Shader/UberLit.hlsl",
		DefinesSpotLight,
		&LightSpotLightPS);

	if (!LightSpotLightPS)
	{
		UE_LOG("Renderer: Failed to create UberLightSpotLightPS");
	}

	// Sampler State
	LightSamplerState = FRenderResourceFactory::CreateSamplerState(
		D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP);

	// Depth State: LESS_EQUAL, Write OFF (for camera outside light volume)
	D3D11_DEPTH_STENCIL_DESC depthDescLessEqual = {};
	depthDescLessEqual.DepthEnable = TRUE;
	depthDescLessEqual.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	depthDescLessEqual.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthDescLessEqual.StencilEnable = FALSE;
	GetDevice()->CreateDepthStencilState(&depthDescLessEqual, &LightDepthLessEqualNoWrite);

	// Additive Blend State
	D3D11_BLEND_DESC blendDesc = {};
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	GetDevice()->CreateBlendState(&blendDesc, &LightAdditiveBlend);
}

void URenderer::CreateIconResources()
{
	TArray<D3D11_INPUT_ELEMENT_DESC> InputLayout =
	{
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Position),
			D3D11_INPUT_PER_VERTEX_DATA, 0
		},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(FNormalVertex, Normal), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(L"Asset/Shader/IconShader.hlsl", InputLayout, &IconVertexShader,
	                                                         &IconInputLayout);
	FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/IconShader.hlsl", &IconPixelShader);

	ConstantBufferIconProperties = FRenderResourceFactory::CreateConstantBuffer<FIconProperties>();
}


void URenderer::CreateFogResources()
{
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/FogShader.hlsl",
		TArray<D3D11_INPUT_ELEMENT_DESC>{},
		&FogVertexShader,
		nullptr
	);

	FRenderResourceFactory::CreatePixelShader(
		L"Asset/Shader/FogShader.hlsl",
		&FogPixelShader
	);

	ConstantBufferFogProperties =
		FRenderResourceFactory::CreateConstantBuffer<FHeightFogParameters>();

	// PostProcess용 선형 보간 샘플러 (FogPass와 FXAAPass에서 공유)
	PostProcessSamplerState = FRenderResourceFactory::CreateSamplerState(
		D3D11_FILTER_MIN_MAG_MIP_LINEAR, D3D11_TEXTURE_ADDRESS_CLAMP
	);
}

void URenderer::CreateFXAAResources()
{
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		L"Asset/Shader/FXAAShader.hlsl",
		TArray<D3D11_INPUT_ELEMENT_DESC>{},
		&FXAAVertexShader,
		nullptr
	);

	FRenderResourceFactory::CreatePixelShader(
		L"Asset/Shader/FXAAShader.hlsl",
		&FXAAPixelShader
	);

	ConstantBufferFXAAParameters = FRenderResourceFactory::CreateConstantBuffer<FFXAAParameters>();
}

// void URenderer::CreateLightComplexityResources()
// {
// 	// Vertex Shader (SV_VertexID 사용하므로 Input Layout 불필요)
// 	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
// 		L"Asset/Shader/LightComplexityShader.hlsl",
// 		TArray<D3D11_INPUT_ELEMENT_DESC>{},
// 		&LightComplexityVertexShader,
// 		nullptr
// 	);
//
// 	// Pixel Shader
// 	FRenderResourceFactory::CreatePixelShader(
// 		L"Asset/Shader/LightComplexityShader.hlsl",
// 		&LightComplexityPixelShader
// 	);
//
// 	// Constant Buffer
// 	ConstantBufferLightComplexity = FRenderResourceFactory::CreateConstantBuffer<FLightComplexityConstants>();
// }

void URenderer::ReleaseUberLightResources()
{
	SafeRelease(LightVertexShader);
	SafeRelease(LightInputLayout);
	SafeRelease(LightPointLightPS);
	SafeRelease(LightSpotLightPS);
	SafeRelease(LightSamplerState);
	SafeRelease(LightDepthLessEqualNoWrite);
	SafeRelease(LightAdditiveBlend);
}
