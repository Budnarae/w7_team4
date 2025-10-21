#include "pch.h"
#include "Render/RenderPass/Public/LightCullingPass.h"

#include "Editor/Public/Camera.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"

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
		"CS_Main",
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

	// Usage Mask Buffer 생성 (2개 uint: PointMask, SpotMask)
	D3D11_BUFFER_DESC BufferDesc = {};
	BufferDesc.ByteWidth = sizeof(uint32) * 2;
	BufferDesc.Usage = D3D11_USAGE_DEFAULT;
	BufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	BufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	BufferDesc.StructureByteStride = sizeof(uint32);

	Device->CreateBuffer(&BufferDesc, nullptr, &UsageMaskBuffer);

	// UAV 생성
	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAVDesc.Buffer.FirstElement = 0;
	UAVDesc.Buffer.NumElements = 2;

	Device->CreateUnorderedAccessView(UsageMaskBuffer, &UAVDesc, &UsageMaskUAV);

	// Staging Buffer 생성 (CPU Readback용)
	D3D11_BUFFER_DESC StagingDesc = {};
	StagingDesc.ByteWidth = sizeof(uint32) * 2;
	StagingDesc.Usage = D3D11_USAGE_STAGING;
	StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

	Device->CreateBuffer(&StagingDesc, nullptr, &UsageMaskStagingBuffer);

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
	if (!ComputeShader || !UsageMaskUAV || !ConstantBufferLightCulling)
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

	// Usage Mask를 0으로 클리어
	UINT ClearValues[4] = {0, 0, 0, 0};
	DeviceContext->ClearUnorderedAccessViewUint(UsageMaskUAV, ClearValues);

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
	DeviceContext->CSSetUnorderedAccessViews(0, 1, &UsageMaskUAV, InitialCounts);

	// Dispatch
	DeviceContext->Dispatch(NumTilesX, NumTilesY, 1);

	// 언바인딩
	ID3D11ShaderResourceView* NullSRVs[] = { nullptr, nullptr, nullptr };
	DeviceContext->CSSetShaderResources(0, 3, NullSRVs);

	ID3D11UnorderedAccessView* NullUAVs[] = { nullptr };
	DeviceContext->CSSetUnorderedAccessViews(0, 1, NullUAVs, InitialCounts);

	DeviceContext->CSSetShader(nullptr, nullptr, 0);

	// ============================================================================
	// WARNING: 동기식 (Synchronous) CPU Readback - TDR 위험!
	// ============================================================================
	// Dispatch() 직후 Map(D3D11_MAP_READ)는 CPU 스톨을 유발하며,
	// 컴퓨트 셰이더가 2초 안에 끝나지 않으면 TDR (Timeout Detection and Recovery) 발생 가능.
	//
	// ** 권장 개선 방법 (향후 적용): **
	// 1. Staging Buffer를 2개(핑퐁)로 만들기
	// 2. 프레임 N에서 Dispatch → CopyResource(StagingBuffer[N % 2])
	// 3. 프레임 N에서 Map(StagingBuffer[(N+1) % 2]) → N-1 프레임 결과 읽기
	// 4. 이렇게 하면 CPU는 GPU를 기다리지 않고 1프레임 전 데이터 사용 (스톨 제거)
	//
	// 현재는 단순 구현으로 유지하되, 라이트가 많거나 화면이 크면 TDR 주의!
	// ============================================================================

	// 결과를 Staging Buffer로 복사하고 CPU에서 읽기 (동기식)
	DeviceContext->CopyResource(UsageMaskStagingBuffer, UsageMaskBuffer);

	D3D11_MAPPED_SUBRESOURCE MappedResource = {};
	if (SUCCEEDED(DeviceContext->Map(UsageMaskStagingBuffer, 0, D3D11_MAP_READ, 0, &MappedResource)))
	{
		uint32* Masks = static_cast<uint32*>(MappedResource.pData);
		if (Context.LightingData)
		{
			// 비상수 포인터로 캐스팅하여 마스크 설정
			FLightingConstants* LightingData = const_cast<FLightingConstants*>(Context.LightingData);
			LightingData->PointLightUsageMask = Masks[0];
			LightingData->SpotLightUsageMask = Masks[1];

			// 마스크 업데이트 후 Constant Buffer 재업데이트 (다음 Pass들이 사용)
			FRenderResourceFactory::UpdateConstantBufferData(ConstantBufferLighting, *LightingData);
		}
		DeviceContext->Unmap(UsageMaskStagingBuffer, 0);
	}
}

void FLightCullingPass::Release()
{
	ReleaseResources();

	SafeRelease(ComputeShader);
	SafeRelease(ConstantBufferLightCulling);
}

void FLightCullingPass::ReleaseResources()
{
	SafeRelease(UsageMaskBuffer);
	SafeRelease(UsageMaskUAV);
	SafeRelease(UsageMaskStagingBuffer);

	SafeRelease(PointLightBuffer);
	SafeRelease(PointLightSRV);

	SafeRelease(SpotLightBuffer);
	SafeRelease(SpotLightSRV);
}

void FLightCullingPass::ReadUsageMasks(uint32& OutPointMask, uint32& OutSpotMask)
{
	// Execute에서 이미 LightingData에 설정하므로 이 함수는 불필요
	OutPointMask = 0;
	OutSpotMask = 0;
}
