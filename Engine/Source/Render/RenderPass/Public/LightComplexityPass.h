#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

class UPipeline;

struct FLightComplexityConstants
{
	FVector2 ScreenDimensions;  // float2
	uint32 NumTilesX;           // uint
	uint32 NumTilesY;           // uint
	uint32 NumPointLights;      // uint
	uint32 NumSpotLights;       // uint
	uint32 _Padding[2];         // uint2
};

/**
 * @brief Light Complexity 시각화 Pass
 * 타일별 라이트 개수를 Heat Map으로 표시
 */
class FLightComplexityPass : public FRenderPass
{
public:
	FLightComplexityPass(
		UPipeline* InPipeline,
		ID3D11RenderTargetView* InSceneColorRTV,
		ID3D11SamplerState* InSamplerState,
		ID3D11VertexShader* InVS,
		ID3D11PixelShader* InPS,
		ID3D11DepthStencilState* InDepthState,
		ID3D11BlendState* InBlendState,
		ID3D11Buffer* InConstantBuffer
	);

	void Execute(FRenderingContext& Context) override;
	void Release() override;

private:
	ID3D11RenderTargetView* SceneColorRTV;
	ID3D11SamplerState* SamplerState;
	ID3D11VertexShader* VS;
	ID3D11PixelShader* PS;
	ID3D11DepthStencilState* DepthState;
	ID3D11BlendState* BlendState;
	ID3D11Buffer* ConstantBuffer;
};
