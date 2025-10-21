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

	// Tile Light Mask Buffer 생성 (타일 개수 × 2: PointMask, SpotMask per tile)
	uint32 TotalTiles = NumTilesX * NumTilesY;
	uint32 MaskBufferSize = TotalTiles * 2;  // 각 타일당 2개 uint32

	D3D11_BUFFER_DESC BufferDesc = {};
	BufferDesc.ByteWidth = sizeof(uint32) * MaskBufferSize;
	BufferDesc.Usage = D3D11_USAGE_DEFAULT;
	BufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;  // UAV + SRV
	BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	BufferDesc.StructureByteStride = sizeof(uint32);

	Device->CreateBuffer(&BufferDesc, nullptr, &TileLightMaskBuffer);

	// UAV 생성 (Compute Shader 출력용)
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAVDesc.Buffer.FirstElement = 0;
	UAVDesc.Buffer.NumElements = MaskBufferSize;

	Device->CreateUnorderedAccessView(TileLightMaskBuffer, &UAVDesc, &TileLightMaskUAV);

	// SRV 생성 (Pixel Shader 입력용)
	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
	SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	SRVDesc.Buffer.FirstElement = 0;
	SRVDesc.Buffer.NumElements = MaskBufferSize;

	Device->CreateShaderResourceView(TileLightMaskBuffer, &SRVDesc, &TileLightMaskSRV);

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
	if (!ComputeShader || !TileLightMaskUAV || !ConstantBufferLightCulling)
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

	// CRITICAL: Compute Shader는 렌더 타겟/DSV 불필요, 명시적으로 언바인딩
	// DepthPrePass에서 이미 언바인딩했지만 안전을 위해 다시 확인
	DeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

	// CRITICAL: RAW 해저드 방지 - 그래픽스 파이프라인에서 SceneDepthSRV 언바인딩
	// DepthPrePass가 DSV로 썼던 리소스를 PS/VS SRV에서도 언바인딩하여 상태 전이 보장
	ID3D11ShaderResourceView* NullGraphicsSRVs[8] = { nullptr };
	DeviceContext->PSSetShaderResources(0, 8, NullGraphicsSRVs);
	DeviceContext->VSSetShaderResources(0, 8, NullGraphicsSRVs);

	// Tile Light Mask를 0으로 클리어
	UINT ClearValues[4] = {0, 0, 0, 0};
	DeviceContext->ClearUnorderedAccessViewUint(TileLightMaskUAV, ClearValues);

	// Scene Depth SRV 가져오기 (이제 DSV가 언바인딩되어 안전하게 읽기 가능)
	SceneDepthSRV = URenderer::GetInstance().GetSceneDepthSRV();
	if (!SceneDepthSRV)
	{
		UE_LOG_ERROR("LightCullingPass: SceneDepthSRV is null");
		return;
	}

	// Constant Buffer 업데이트
	FLightCullingConstants Constants = {};
	Constants.ViewMatrix = Context.ViewProjConstants->View;
	Constants.ProjectionMatrix = Context.ViewProjConstants->Projection;
	Constants.InverseProjectionMatrix = Constants.ProjectionMatrix.Inverse();
	Constants.ScreenDimensions = FVector2(static_cast<float>(ScreenWidth), static_cast<float>(ScreenHeight));
	Constants.NumTiles[0] = NumTilesX;
	Constants.NumTiles[1] = NumTilesY;

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
	UINT InitialCounts[] = { 0 };
	DeviceContext->CSSetUnorderedAccessViews(0, 1, &TileLightMaskUAV, InitialCounts);

	// Dispatch
	DeviceContext->Dispatch(NumTilesX, NumTilesY, 1);

	// 언바인딩
	ID3D11ShaderResourceView* NullSRVs[] = { nullptr, nullptr, nullptr };
	DeviceContext->CSSetShaderResources(0, 3, NullSRVs);

	ID3D11UnorderedAccessView* NullUAVs[] = { nullptr };
	DeviceContext->CSSetUnorderedAccessViews(0, 1, NullUAVs, InitialCounts);

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
	SafeRelease(TileLightMaskBuffer);
	SafeRelease(TileLightMaskUAV);
	SafeRelease(TileLightMaskSRV);

	SafeRelease(PointLightBuffer);
	SafeRelease(PointLightSRV);

	SafeRelease(SpotLightBuffer);
	SafeRelease(SpotLightSRV);
}
