#pragma once
#include "Render/RenderPass/Public/RenderPass.h"

class UPipeline;

/**
 * @brief Tiled-Based Light Culling Pass
 * 화면을 32x32 타일로 분할하고 각 타일에 영향을 주는 Light만 선별하는 Compute Pass
 */
class FLightCullingPass : public FRenderPass
{
public:
	FLightCullingPass(
		UPipeline* InPipeline,
		ID3D11Buffer* InConstantBufferViewProj,
		ID3D11Buffer* InConstantBufferLighting
	);

	virtual void Execute(FRenderingContext& Context) override;
	virtual void Release() override;

	void CreateResources(uint32 InScreenWidth, uint32 InScreenHeight);
	void ReleaseResources();

	ID3D11ShaderResourceView* GetTileLightMaskSRV() const { return TileLightMaskSRV; }

private:
	static constexpr uint32 TILE_SIZE = 32;
	static constexpr uint32 MAX_LIGHTS_PER_TILE = 256;

	struct FLightCullingConstants
	{
		FMatrix ViewMatrix;
		FMatrix ProjectionMatrix;
		FMatrix InverseProjectionMatrix;
		FVector2 ScreenDimensions;
		uint32 NumTiles[2];  // NumTilesX, NumTilesY
		uint32 NumPointLights;
		uint32 NumSpotLights;
		float NearPlane;
		float FarPlane;
	};

	ID3D11ComputeShader* ComputeShader = nullptr;
	ID3D11Buffer* ConstantBufferLightCulling = nullptr;

	// Light 입력 버퍼 (Lighting CB에서 복사)
	ID3D11Buffer* PointLightBuffer = nullptr;
	ID3D11ShaderResourceView* PointLightSRV = nullptr;

	ID3D11Buffer* SpotLightBuffer = nullptr;
	ID3D11ShaderResourceView* SpotLightSRV = nullptr;

	// 출력 버퍼 (타일별 Light Mask)
	ID3D11Buffer* TileLightMaskBuffer = nullptr;
	ID3D11UnorderedAccessView* TileLightMaskUAV = nullptr;
	ID3D11ShaderResourceView* TileLightMaskSRV = nullptr;

	// Scene Depth (입력)
	ID3D11ShaderResourceView* SceneDepthSRV = nullptr;

	uint32 ScreenWidth = 0;
	uint32 ScreenHeight = 0;
	uint32 NumTilesX = 0;
	uint32 NumTilesY = 0;
};
