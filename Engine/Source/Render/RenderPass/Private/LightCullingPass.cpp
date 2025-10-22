#include "pch.h"
#include "Render/RenderPass/Public/LightCullingPass.h"

#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Render/UI/Overlay/Public/StatOverlay.h"

FLightCullingPass::FLightCullingPass(
	UPipeline* InPipeline,
	ID3D11Buffer* InConstantBufferViewProj,
	ID3D11Buffer* InConstantBufferLighting
) :
	FRenderPass(InPipeline, InConstantBufferViewProj, nullptr, InConstantBufferLighting)
{
	// Compute Shader 컴파일
	ID3DBlob* ShaderBlob = nullptr;
	ID3DBlob* ErrorBlob = nullptr;

	HRESULT Result = D3DCompileFromFile(
		L"Asset/Shader/LightCulling.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"mainCS",
		"cs_5_0",
		0,
		0,
		&ShaderBlob,
		&ErrorBlob
	);

	if (FAILED(Result))
	{
		if (ErrorBlob)
		{
			OutputDebugStringA(static_cast<char*>(ErrorBlob->GetBufferPointer()));
			SafeRelease(ErrorBlob);
		}
		UE_LOG_ERROR("LightCullingPass: Compute Shader compilation failed");
		return;
	}

	URenderer::GetInstance().GetDevice()->CreateComputeShader(
		ShaderBlob->GetBufferPointer(),
		ShaderBlob->GetBufferSize(),
		nullptr,
		&ComputeShader
	);

	SafeRelease(ShaderBlob);

	// Constant Buffer 생성
	ConstantBufferLightCulling = FRenderResourceFactory::CreateConstantBuffer<FLightCullingConstants>();
}

void FLightCullingPass::CreateResources(uint32 InScreenWidth, uint32 InScreenHeight)
{
	ScreenWidth = InScreenWidth;
	ScreenHeight = InScreenHeight;
	NumTilesX = (ScreenWidth + TILE_SIZE - 1) / TILE_SIZE;
	NumTilesY = (ScreenHeight + TILE_SIZE - 1) / TILE_SIZE;

	auto* Device = URenderer::GetInstance().GetDevice();

	// 타일별 라이트 인덱스 리스트 버퍼 생성
	uint32 TotalTiles = NumTilesX * NumTilesY;
	uint32 MaxLightsTotal = MAX_LIGHTS_PER_TILE * TotalTiles;  // 최악의 경우: 모든 타일에 최대 라이트

	// TileLightOffsets: 각 타일의 인덱스 리스트 시작 오프셋
	D3D11_BUFFER_DESC OffsetsDesc = {};
	OffsetsDesc.ByteWidth = sizeof(uint32) * TotalTiles;
	OffsetsDesc.Usage = D3D11_USAGE_DEFAULT;
	OffsetsDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	OffsetsDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	OffsetsDesc.StructureByteStride = sizeof(uint32);
	Device->CreateBuffer(&OffsetsDesc, nullptr, &TileLightOffsetsBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC OffsetsUAVDesc = {};
	OffsetsUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	OffsetsUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	OffsetsUAVDesc.Buffer.NumElements = TotalTiles;
	Device->CreateUnorderedAccessView(TileLightOffsetsBuffer, &OffsetsUAVDesc, &TileLightOffsetsUAV);

	D3D11_SHADER_RESOURCE_VIEW_DESC OffsetsSRVDesc = {};
	OffsetsSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	OffsetsSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	OffsetsSRVDesc.Buffer.NumElements = TotalTiles;
	Device->CreateShaderResourceView(TileLightOffsetsBuffer, &OffsetsSRVDesc, &TileLightOffsetsSRV);

	// TileLightCounts: 각 타일의 라이트 개수
	D3D11_BUFFER_DESC CountsDesc = {};
	CountsDesc.ByteWidth = sizeof(uint32) * 2 * TotalTiles;  // uint2 per tile
	CountsDesc.Usage = D3D11_USAGE_DEFAULT;
	CountsDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	CountsDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	CountsDesc.StructureByteStride = sizeof(uint32) * 2;
	Device->CreateBuffer(&CountsDesc, nullptr, &TileLightCountsBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC CountsUAVDesc = {};
	CountsUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	CountsUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	CountsUAVDesc.Buffer.NumElements = TotalTiles;
	Device->CreateUnorderedAccessView(TileLightCountsBuffer, &CountsUAVDesc, &TileLightCountsUAV);

	D3D11_SHADER_RESOURCE_VIEW_DESC CountsSRVDesc = {};
	CountsSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	CountsSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	CountsSRVDesc.Buffer.NumElements = TotalTiles;
	Device->CreateShaderResourceView(TileLightCountsBuffer, &CountsSRVDesc, &TileLightCountsSRV);

	// TileLightIndices: 라이트 인덱스 배열
	D3D11_BUFFER_DESC IndicesDesc = {};
	IndicesDesc.ByteWidth = sizeof(uint32) * MaxLightsTotal;
	IndicesDesc.Usage = D3D11_USAGE_DEFAULT;
	IndicesDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	IndicesDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	IndicesDesc.StructureByteStride = sizeof(uint32);
	Device->CreateBuffer(&IndicesDesc, nullptr, &TileLightIndicesBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC IndicesUAVDesc = {};
	IndicesUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	IndicesUAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	IndicesUAVDesc.Buffer.NumElements = MaxLightsTotal;
	Device->CreateUnorderedAccessView(TileLightIndicesBuffer, &IndicesUAVDesc, &TileLightIndicesUAV);

	D3D11_SHADER_RESOURCE_VIEW_DESC IndicesSRVDesc = {};
	IndicesSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	IndicesSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	IndicesSRVDesc.Buffer.NumElements = MaxLightsTotal;
	Device->CreateShaderResourceView(TileLightIndicesBuffer, &IndicesSRVDesc, &TileLightIndicesSRV);

	// Point Light StructuredBuffer 생성
	// (매 프레임 업데이트되므로 DYNAMIC 사용)
	D3D11_BUFFER_DESC PointLightBufferDesc = {};
	PointLightBufferDesc.ByteWidth = sizeof(FPointLightInfo) * 16; // MAX 16 point lights
	PointLightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	PointLightBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	PointLightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	PointLightBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	PointLightBufferDesc.StructureByteStride = sizeof(FPointLightInfo);

	Device->CreateBuffer(&PointLightBufferDesc, nullptr, &PointLightBuffer);

	D3D11_SHADER_RESOURCE_VIEW_DESC PointLightSRVDesc = {};
	PointLightSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	PointLightSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	PointLightSRVDesc.Buffer.NumElements = 16;

	Device->CreateShaderResourceView(PointLightBuffer, &PointLightSRVDesc, &PointLightSRV);

	// Spot Light StructuredBuffer 생성
	D3D11_BUFFER_DESC SpotLightBufferDesc = {};
	SpotLightBufferDesc.ByteWidth = sizeof(FSpotLightInfo) * 16; // MAX 16 spot lights
	SpotLightBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	SpotLightBufferDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	SpotLightBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	SpotLightBufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	SpotLightBufferDesc.StructureByteStride = sizeof(FSpotLightInfo);

	Device->CreateBuffer(&SpotLightBufferDesc, nullptr, &SpotLightBuffer);

	D3D11_SHADER_RESOURCE_VIEW_DESC SpotLightSRVDesc = {};
	SpotLightSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SpotLightSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SpotLightSRVDesc.Buffer.NumElements = 16;

	Device->CreateShaderResourceView(SpotLightBuffer, &SpotLightSRVDesc, &SpotLightSRV);
}

void FLightCullingPass::Execute(FRenderingContext& Context)
{
	// 필수 리소스 유효성 검사
	if (!ComputeShader || !TileLightOffsetsUAV || !TileLightCountsUAV || !TileLightIndicesUAV || !ConstantBufferLightCulling)
	{
		return;
	}

	// 화면 크기 유효성 확인
	if (ScreenWidth == 0 || ScreenHeight == 0)
	{
		return;
	}

	auto* DeviceContext = URenderer::GetInstance().GetDeviceContext();
	if (!DeviceContext)
	{
		return;
	}

	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	ID3D11ShaderResourceView* NullGraphicsSRVs[8] = { nullptr };
	DeviceContext->PSSetShaderResources(0, 8, NullGraphicsSRVs);
	DeviceContext->VSSetShaderResources(0, 8, NullGraphicsSRVs);

	// 첫 번째 뷰포트에서만 전체 버퍼 클리어
	if (!bClearedThisFrame)
	{
		UINT ClearValues[4] = {0, 0, 0, 0};
		DeviceContext->ClearUnorderedAccessViewUint(TileLightOffsetsUAV, ClearValues);
		DeviceContext->ClearUnorderedAccessViewUint(TileLightCountsUAV, ClearValues);
		DeviceContext->ClearUnorderedAccessViewUint(TileLightIndicesUAV, ClearValues);
		bClearedThisFrame = true;
	}

	// Scene Depth SRV 가져오기 (이제 DSV가 언바인딩되어 안전하게 읽기 가능)
	SceneDepthSRV = URenderer::GetInstance().GetSceneDepthSRV();
	if (!SceneDepthSRV)
	{
		UE_LOG_ERROR("LightCullingPass: SceneDepthSRV is null");
		return;
	}

	// 현재 Viewport의 크기 (각 뷰포트마다 독립 연산)
	uint32 ViewportWidth = static_cast<uint32>(Context.Viewport.Width);
	uint32 ViewportHeight = static_cast<uint32>(Context.Viewport.Height);
	uint32 ViewportNumTilesX = (ViewportWidth + 31) / 32;
	uint32 ViewportNumTilesY = (ViewportHeight + 31) / 32;

	// Constant Buffer 업데이트 (뷰포트별 카메라 정보)
	FLightCullingConstants Constants = {};
	Constants.ViewMatrix = Context.ViewProjConstants->View;
	Constants.ProjectionMatrix = Context.ViewProjConstants->Projection;
	Constants.InverseProjectionMatrix = Constants.ProjectionMatrix.Inverse();

	// ScreenDimensions는 현재 뷰포트 크기
	Constants.ScreenDimensions = FVector2(static_cast<float>(ViewportWidth), static_cast<float>(ViewportHeight));
	Constants.NumTiles[0] = ViewportNumTilesX;
	Constants.NumTiles[1] = ViewportNumTilesY;

	// Viewport 오프셋 (SceneDepth 텍스처 내 뷰포트 시작 위치)
	Constants.ViewportOffset[0] = static_cast<uint32>(Context.Viewport.TopLeftX);
	Constants.ViewportOffset[1] = static_cast<uint32>(Context.Viewport.TopLeftY);

	// 전체 SceneDepth 텍스처 크기
	Constants.SceneRTSize[0] = static_cast<uint32>(Context.SceneRTSize.X);
	Constants.SceneRTSize[1] = static_cast<uint32>(Context.SceneRTSize.Y);

	// Camera Near/Far plane 설정
	if (Context.CurrentCamera)
	{
		Constants.NearPlane = Context.CurrentCamera->GetNearZ();
		Constants.FarPlane = Context.CurrentCamera->GetFarZ();
	}
	else
	{
		Constants.NearPlane = 0.1f;
		Constants.FarPlane = 1000.0f;
	}

	// Light 개수 설정
	if (Context.LightingData)
	{
		Constants.NumPointLights = Context.LightingData->NumActivePointLights;
		Constants.NumSpotLights = Context.LightingData->NumActiveSpotLights;
	}
	else
	{
		Constants.NumPointLights = 0;
		Constants.NumSpotLights = 0;
	}

	FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLightCulling, Constants);

	// Point Light Structured Buffer 업데이트
	if (Constants.NumPointLights > 0 && PointLightBuffer && Context.LightingData)
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		if (SUCCEEDED(DeviceContext->Map(PointLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
		{
			memcpy(MappedResource.pData, Context.LightingData->PointLights, sizeof(FPointLightInfo) * Constants.NumPointLights);
			DeviceContext->Unmap(PointLightBuffer, 0);
		}
	}

	// Spot Light Structured Buffer 업데이트
	if (Constants.NumSpotLights > 0 && SpotLightBuffer && Context.LightingData)
	{
		D3D11_MAPPED_SUBRESOURCE MappedResource = {};
		if (SUCCEEDED(DeviceContext->Map(SpotLightBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &MappedResource)))
		{
			memcpy(MappedResource.pData, Context.LightingData->SpotLights, sizeof(FSpotLightInfo) * Constants.NumSpotLights);
			DeviceContext->Unmap(SpotLightBuffer, 0);
		}
	}

	// Compute Shader 설정
	DeviceContext->CSSetShader(ComputeShader, nullptr, 0);
	DeviceContext->CSSetConstantBuffers(0, 1, &ConstantBufferLightCulling);

	// 입력 리소스 바인딩 (Light가 없으면 nullptr도 허용, shader에서 count로 제어)
	ID3D11ShaderResourceView* SRVs[] = {
		SceneDepthSRV,
		PointLightSRV ? PointLightSRV : nullptr,
		SpotLightSRV ? SpotLightSRV : nullptr
	};
	DeviceContext->CSSetShaderResources(0, 3, SRVs);

	// 출력 리소스 바인딩
	ID3D11UnorderedAccessView* UAVs[] = {
		TileLightOffsetsUAV,
		TileLightCountsUAV,
		TileLightIndicesUAV
	};
	UINT InitialCounts[] = { static_cast<UINT>(-1), static_cast<UINT>(-1), static_cast<UINT>(-1) };
	DeviceContext->CSSetUnorderedAccessViews(0, 3, UAVs, InitialCounts);

	// Dispatch (현재 뷰포트 크기만큼만)
	DeviceContext->Dispatch(ViewportNumTilesX, ViewportNumTilesY, 1);

	// 언바인딩
	ID3D11ShaderResourceView* NullSRVs[] = { nullptr, nullptr, nullptr };
	DeviceContext->CSSetShaderResources(0, 3, NullSRVs);

	ID3D11UnorderedAccessView* NullUAVs[] = { nullptr, nullptr, nullptr };
	DeviceContext->CSSetUnorderedAccessViews(0, 3, NullUAVs, InitialCounts);

	DeviceContext->CSSetShader(nullptr, nullptr, 0);

	// Light Culling 통계 기록
	UStatOverlay::GetInstance().RecordLightCullingStats(
		Constants.NumPointLights,
		Constants.NumSpotLights
	);
}

void FLightCullingPass::Release()
{
	ReleaseResources();

	SafeRelease(ComputeShader);
	SafeRelease(ConstantBufferLightCulling);
}

void FLightCullingPass::ReleaseResources()
{
	SafeRelease(TileLightOffsetsBuffer);
	SafeRelease(TileLightOffsetsUAV);
	SafeRelease(TileLightOffsetsSRV);

	SafeRelease(TileLightCountsBuffer);
	SafeRelease(TileLightCountsUAV);
	SafeRelease(TileLightCountsSRV);

	SafeRelease(TileLightIndicesBuffer);
	SafeRelease(TileLightIndicesUAV);
	SafeRelease(TileLightIndicesSRV);

	SafeRelease(PointLightBuffer);
	SafeRelease(PointLightSRV);

	SafeRelease(SpotLightBuffer);
	SafeRelease(SpotLightSRV);
}

void FLightCullingPass::ResetFrameFlag()
{
	bClearedThisFrame = false;
}
