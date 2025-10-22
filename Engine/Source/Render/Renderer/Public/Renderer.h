#pragma once
#include "DeviceResources.h"
#include "Core/Public/Object.h"
#include "Component/Public/PrimitiveComponent.h"
#include "Editor/Public/EditorPrimitive.h"
#include "Render/Renderer/Public/Pipeline.h"
#include "Render/RenderPass/Public/FXAAPass.h"

class UDeviceResources;
class UPrimitiveComponent;
class UStaticMeshComponent;
class UUUIDTextComponent;
class AActor;
class AGizmo;
class UEditor;
class UFontRenderer;
class FViewport;
class UCamera;
class UPipeline;
class FSceneDepthPass;
class FNormalPass;
class FLightCullingPass;
class FDepthPrePass;

/**
 * @brief Rendering Pipeline 전반을 처리하는 클래스
 */
UCLASS()
class URenderer : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(URenderer, UObject)

public:
	void Init(HWND InWindowHandle);
	void Release();

	// Initialize
	void CreateDepthStencilState();
	void CreateBlendState();
	void CreateDefaultShader();
	void CreateDecalShader();
	void CreateTextureShader();
	void CreateDepthShader();
	void CreateFireBallShader();
	void CreateFireBallForwardShader();
	void CreateUberLightResources();
	void CreateIconResources();
	void CreateFogResources();
	void CreateFXAAResources();
	void CreateFullscreenQuad();
	void CreateConstantBuffers();
	void CreateSceneRenderTargets();
	void CreateSceneDepthResources();
	void CreateNormalResources();
	// void CreateLightComplexityResources();

	// Release
	void ReleaseConstantBuffers();
	void ReleaseDefaultShader();
	void ReleaseDepthStencilState();
	void ReleaseBlendState();
	void ReleaseDepthShader();
	void ReleaseFireBallShader();
	void ReleaseFireBallForwardShader();
	void ReleaseUberLightResources();
	void ReleaseIconResources();
	void ReleaseFogResources();
	void ReleaseFXAAResources();
	void ReleaseFullscreenQuad();
	void ReleaseSceneRenderTargets();
	void ReleaseSceneDepthResources();
	void ReleaseNormalResources();
	// void ReleaseLightComplexityResources();

	// Render
	void Update();
	void RenderBegin() const;
	void RenderLevel(UCamera* InCurrentCamera, const D3D11_VIEWPORT& InViewport);
	void RenderEnd() const;
    void RenderEditorPrimitive(const FEditorPrimitive& InPrimitive, const FRenderState& InRenderState, uint32 InStride = 0, uint32 InIndexBufferStride = 0);

    void OnResize(uint32 Inwidth = 0, uint32 InHeight = 0);

    // Hot-reload integration: swap in newly built UberLit shaders
    void ApplyUberLitShaders(
        ID3D11VertexShader* InUberVS,
        ID3D11InputLayout* InUberLayout,
        ID3D11VertexShader* InGouraudVS,
        ID3D11InputLayout* InGouraudLayout,
        ID3D11PixelShader* InUnlitPS,
        ID3D11PixelShader* InLambertPS,
        ID3D11PixelShader* InGouraudPS,
        ID3D11PixelShader* InPhongPS);

	// Getter & Setter
	ID3D11Device* GetDevice() const { return DeviceResources->GetDevice(); }
	ID3D11DeviceContext* GetDeviceContext() const { return DeviceResources->GetDeviceContext(); }
	IDXGISwapChain* GetSwapChain() const { return DeviceResources->GetSwapChain(); }
	ID3D11RenderTargetView* GetRenderTargetView() const { return DeviceResources->GetRenderTargetView(); }
	UDeviceResources* GetDeviceResources() const { return DeviceResources; }
	FViewport* GetViewportClient() const { return ViewportClient; }
	UPipeline* GetPipeline() const { return Pipeline; }
	bool GetIsResizing() const { return bIsResizing; }

	ID3D11DepthStencilState* GetDefaultDepthStencilState() const { return DefaultDepthStencilState; }
	ID3D11DepthStencilState* GetDisabledDepthStencilState() const { return DisabledDepthStencilState; }
	ID3D11DepthStencilState* GetDepthTestAlwaysNoWriteState() const { return DepthTestAlwaysNoWriteState; }
	ID3D11DepthStencilState* GetDebugLineDepthState() const { return DebugLineDepthState; }
	ID3D11BlendState* GetAlphaBlendState() const { return AlphaBlendState; }
	ID3D11BlendState* GetAdditiveBlendState() const { return AdditiveBlendState; }
	ID3D11BlendState* GetFireBallBlendState() const { return FireBallBlendState; }
	ID3D11Buffer* GetConstantBufferModels() const { return ConstantBufferModels; }
	ID3D11Buffer* GetConstantBufferViewProj() const { return ConstantBufferViewProj; }
	ID3D11RenderTargetView* GetSceneColorRTV() const{ return SceneColorRTV; }
	ID3D11ShaderResourceView* GetSceneDepthSRV() const { return SceneDepthSRV; }
	ID3D11DepthStencilView* GetSceneDepthDSV() const { return SceneDepthDSV; }
	ID3D11Texture2D* GetSceneDepthTexture() const { return SceneDepthTexture; }
	ID3D11DepthStencilView* GetReadOnlyDSV() const { return SceneDepthDSV_ReadOnly; }
	ID3D11ShaderResourceView* GetSceneColorSRV() const { return SceneColorSRV; }

	ID3D11VertexShader* GetDepthVertexShader() const { return DepthVertexShader; }
	ID3D11PixelShader* GetDepthPixelShader() const { return DepthPixelShader; }
	ID3D11InputLayout* GetDepthInputLayout() const { return DepthInputLayout; }

	ID3D11VertexShader* GetTextureVertexShader() const { return TextureVertexShader; }
	ID3D11PixelShader* GetTexturePixelShader() const { return TexturePixelShader; }
	ID3D11InputLayout* GetTextureInputLayout() const { return TextureInputLayout; }

	ID3D11VertexShader* GetUberLitNormalMappingVertexShader() const { return UberLitNormalMappingVertexShader; }
	ID3D11InputLayout* GetUberLitNormalMappingInputLayout() const { return UberLitNormalMappingInputLayout; }
	ID3D11PixelShader* GetUberLambertNormalMappingPixelShader() const { return UberLambertNormalMappingPixelShader; }
	ID3D11PixelShader* GetUberPhongNormalMappingPixelShader() const { return UberPhongNormalMappingPixelShader; }

	ID3D11VertexShader* GetUberLitVertexShader() const { return UberLitVertexShader; }
	ID3D11VertexShader* GetUberLitGouraudVertexShader() const { return UberLitGouraudVertexShader; }
	ID3D11PixelShader* GetTextureLambertPixelShader() const { return TextureLambertPixelShader; }
	ID3D11PixelShader* GetTextureGouraudPixelShader() const { return TextureGouraudPixelShader; }
	ID3D11PixelShader* GetTexturePhongPixelShader() const { return TexturePhongPixelShader; }
	ID3D11PixelShader* GetTextureUnlitPixelShader() const { return TextureUnlitPixelShader; }
	ID3D11InputLayout* GetUberLitInputLayout() const { return UberLitInputLayout; }
	ID3D11InputLayout* GetUberLitGouraudInputLayout() const { return UberLitGouraudInputLayout; }

	void SetIsResizing(bool isResizing) { bIsResizing = isResizing; }
	void SetNeedsResize(bool bNeedsResize) { bNeedsResizeNextFrame = bNeedsResize; }
private:
	UPipeline* Pipeline = nullptr;
	UDeviceResources* DeviceResources = nullptr;
	UFontRenderer* FontRenderer = nullptr;
	TArray<UPrimitiveComponent*> PrimitiveComponents;

	// States
	ID3D11DepthStencilState* DefaultDepthStencilState = nullptr;
	ID3D11DepthStencilState* DecalDepthStencilState = nullptr;
	ID3D11DepthStencilState* DisabledDepthStencilState = nullptr;
	ID3D11DepthStencilState* NoTestButWriteDepthState = nullptr;  // Depth test 비활성화, depth write 활성화
	ID3D11DepthStencilState* DepthTestAlwaysNoWriteState = nullptr;  // Depth test ALWAYS, Write OFF (for Fog)
	ID3D11DepthStencilState* DebugLineDepthState = nullptr;  // Depth test ON (LESS_EQUAL), Write OFF (for BatchLines)
	ID3D11BlendState* AlphaBlendState = nullptr;
	ID3D11BlendState* AdditiveBlendState = nullptr;
	ID3D11BlendState* FireBallBlendState = nullptr;
	// FireBall Forward shader resources
	ID3D11VertexShader* FireBallFwdVertexShader = nullptr;
	ID3D11PixelShader* FireBallFwdPixelShader = nullptr;
	ID3D11InputLayout* FireBallFwdInputLayout = nullptr;

	// Constant Buffers
	ID3D11Buffer* ConstantBufferModels = nullptr;
	ID3D11Buffer* ConstantBufferViewProj = nullptr;
	ID3D11Buffer* ConstantBufferColor = nullptr;
	ID3D11Buffer* ConstantBufferBatchLine = nullptr;
	ID3D11Buffer* ConstantBufferLighting = nullptr;

	ID3D11Buffer* ConstantBufferModelForLight = nullptr;

	FLOAT ClearColor[4] = {0.025f, 0.025f, 0.025f, 1.0f};

	// Default Shaders
	ID3D11VertexShader* DefaultVertexShader = nullptr;
	ID3D11PixelShader* DefaultPixelShader = nullptr;
	ID3D11InputLayout* DefaultInputLayout = nullptr;

	// Texture Shaders
	ID3D11VertexShader* TextureVertexShader = nullptr;
	ID3D11PixelShader* TexturePixelShader = nullptr;
	ID3D11InputLayout* TextureInputLayout = nullptr;

	// UberLit Shaders

	/* Normal Mapping으로 연산하는 광원 셰이더 */
	ID3D11VertexShader* UberLitNormalMappingVertexShader = nullptr;	// Lambert/Phong VS with normal mapping
	ID3D11InputLayout* UberLitNormalMappingInputLayout = nullptr;	// Lambert/Phong Input Layout with normal mapping
	ID3D11PixelShader* UberLambertNormalMappingPixelShader = nullptr;	// Lambert shading PS with normal mapping
	ID3D11PixelShader* UberPhongNormalMappingPixelShader = nullptr;		// Blinn-Phong shading PS with normal mapping

	ID3D11VertexShader* UberLitVertexShader = nullptr;    // Lambert/Phong VS (PS에서 라이팅)/Unlit
	ID3D11InputLayout* UberLitInputLayout = nullptr;      // Lambert/Phong Input Layout
	ID3D11VertexShader* UberLitGouraudVertexShader = nullptr;  // Gouraud VS (VS에서 라이팅)
	ID3D11InputLayout* UberLitGouraudInputLayout = nullptr;    // Gouraud Input Layout
	ID3D11PixelShader* TextureLambertPixelShader = nullptr;   // Lambert shading
	ID3D11PixelShader* TextureGouraudPixelShader = nullptr; // Gouraud shading
	ID3D11PixelShader* TexturePhongPixelShader = nullptr;  // Blinn-Phong shading
	ID3D11PixelShader* TextureUnlitPixelShader = nullptr; // Unlit

	ID3D11VertexShader* DecalVertexShader = nullptr;
	ID3D11PixelShader* DecalPixelShader = nullptr;
	ID3D11InputLayout* DecalInputLayout = nullptr;

	// Depth Shaders (for Scene Depth ViewMode)
	ID3D11VertexShader* DepthVertexShader = nullptr;
	ID3D11PixelShader* DepthPixelShader = nullptr;
	ID3D11InputLayout* DepthInputLayout = nullptr;

	ID3D11VertexShader* FireBallVertexShader = nullptr;
	ID3D11PixelShader* FireBallPixelShader = nullptr;
	ID3D11InputLayout* FireBallInputLayout = nullptr;
	ID3D11Buffer* CBPerObject = nullptr;
	ID3D11Buffer* CBFireBall = nullptr;

	// Light Resources
	ID3D11VertexShader* LightVertexShader = nullptr;
	ID3D11InputLayout* LightInputLayout = nullptr;
	ID3D11PixelShader* LightPointLightPS = nullptr;
	ID3D11PixelShader* LightSpotLightPS = nullptr;
	ID3D11SamplerState* LightSamplerState = nullptr;
	ID3D11DepthStencilState* LightDepthLessEqualNoWrite = nullptr;
	ID3D11BlendState* LightAdditiveBlend = nullptr;

	// Icon Shaders
	ID3D11VertexShader* IconVertexShader = nullptr;
	ID3D11PixelShader* IconPixelShader = nullptr;
	ID3D11InputLayout* IconInputLayout = nullptr;
	ID3D11Buffer* ConstantBufferIconProperties = nullptr;

	// Fog Shaders
	ID3D11VertexShader* FogVertexShader = nullptr;
	ID3D11PixelShader* FogPixelShader = nullptr;
	ID3D11Buffer* ConstantBufferFogProperties = nullptr;

	// Scene Depth
	ID3D11VertexShader* SceneDepthVertexShader = nullptr;
	ID3D11PixelShader* SceneDepthPixelShader = nullptr;
	ID3D11Buffer* ConstantBufferSceneDepthProperties = nullptr;

	// Normal View Mode
	ID3D11VertexShader* SceneNormalVertexShader = nullptr;
	ID3D11PixelShader* SceneNormalPixelShader = nullptr;
	ID3D11Buffer* ConstantBufferNormalProperties = nullptr;

	// FXAA Shaders
	ID3D11VertexShader* FXAAVertexShader = nullptr;
	ID3D11PixelShader* FXAAPixelShader = nullptr;
	ID3D11Buffer* ConstantBufferFXAAParameters = nullptr;

	// Fullscreen Quad for Post-Processing
	ID3D11Buffer* FullscreenQuadVB = nullptr;
	ID3D11Buffer* FullscreenQuadIB = nullptr;

	// Scene Render Targets for Post-Processing
	ID3D11Texture2D* SceneColorTexture = nullptr;
	ID3D11RenderTargetView* SceneColorRTV = nullptr;
	ID3D11ShaderResourceView* SceneColorSRV = nullptr;

	ID3D11Texture2D* SceneDepthTexture = nullptr;
	ID3D11DepthStencilView* SceneDepthDSV = nullptr;
	ID3D11ShaderResourceView* SceneDepthSRV = nullptr;
	ID3D11DepthStencilView* SceneDepthDSV_ReadOnly = nullptr;

	// GBuffer for normal view mode
	ID3D11Texture2D* SceneNormalTexture = nullptr;
	ID3D11RenderTargetView* SceneNormalRTV = nullptr;
	ID3D11ShaderResourceView* SceneNormalSRV = nullptr;

	uint32 Stride = 0;

	FViewport* ViewportClient = nullptr;

	bool bIsResizing = false;
	bool bNeedsResizeNextFrame = false;

	ID3D11SamplerState* PostProcessSamplerState = nullptr;

	// Render Level에서 초기화 전 사용 불가
	FRenderingContext RenderingContext{
		nullptr,
		nullptr,
		EViewModeIndex::VMI_Unlit,
		0
	};

	TArray<class FRenderPass*> RenderPasses;
	FDepthPrePass* DepthPrePass = nullptr;
	FSceneDepthPass* SceneDepthPass = nullptr;
	FNormalPass* NormalPass = nullptr;
	FFXAAPass* FXAAPass = nullptr;
	FLightCullingPass* LightCullingPass = nullptr;
	class FLightComplexityPass* LightComplexityPass = nullptr;

	// Light Complexity Shader Resources
	ID3D11VertexShader* LightComplexityVertexShader = nullptr;
	ID3D11PixelShader* LightComplexityPixelShader = nullptr;
	ID3D11Buffer* ConstantBufferLightComplexity = nullptr;
};
