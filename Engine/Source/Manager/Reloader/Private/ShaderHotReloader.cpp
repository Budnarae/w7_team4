#include "pch.h"
#include "Manager/Reloader/Public/ShaderHotReloader.h"
#include "Render/Renderer/Public/Renderer.h"
#include "Render/Renderer/Public/RenderResourceFactory.h"
#include "Component/Mesh/Public/VertexDatas.h"

IMPLEMENT_SINGLETON_CLASS_BASE(UShaderHotReloader)

UShaderHotReloader::UShaderHotReloader() = default;
UShaderHotReloader::~UShaderHotReloader() = default;

void UShaderHotReloader::Initialize(const std::wstring& InWatchDirectory)
{
	if (bInitialized)
	{
		return;
	}

	WatchDirectory = InWatchDirectory;
	TargetShaderPath = WatchDirectory + L"\\UberLit.hlsl";

	std::error_code ErrorCode;
	if (!std::filesystem::exists(TargetShaderPath, ErrorCode))
	{
		UE_LOG("ShaderHotReloader: UberLit.hlsl not found in watch directory!");
		return;
	}

	// 파일의 초기 수정 시각 기록
	LastWriteTime = std::filesystem::last_write_time(TargetShaderPath, ErrorCode);
	if (ErrorCode)
	{
		ErrorCode.clear();
	}

	LastScanTime = std::chrono::steady_clock::now();
	bInitialized = true;

	UE_LOG("ShaderHotReloader: Initialized and monitoring UberLit.hlsl.");
}

void UShaderHotReloader::Tick(URenderer& InRenderer)
{
	if (!bInitialized)
	{
		return;
	}

	const auto CurrentTime = std::chrono::steady_clock::now();
	if (CurrentTime - LastScanTime < ScanInterval)
	{
		return;
	}
	LastScanTime = CurrentTime;

	if (HasUberShaderChanged())
	{
		RebuildUberLitShaders(InRenderer);
	}
}

bool UShaderHotReloader::HasUberShaderChanged()
{
	std::error_code ErrorCode;
	if (!std::filesystem::exists(TargetShaderPath, ErrorCode))
	{
		return false;
	}

	auto CurrentWriteTime = std::filesystem::last_write_time(TargetShaderPath, ErrorCode);
	if (ErrorCode)
	{
		ErrorCode.clear();
		return false;
	}

	if (CurrentWriteTime != LastWriteTime)
	{
		LastWriteTime = CurrentWriteTime;
		return true;
	}

	return false;
}

void UShaderHotReloader::RebuildUberLitShaders(URenderer& InRenderer)
{
	UE_LOG("ShaderHotReloader: Detected change in UberLit.hlsl — rebuilding shaders...");

	TArray<D3D11_INPUT_ELEMENT_DESC> UberLitLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(FNormalVertex, Position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, (UINT)offsetof(FNormalVertex, Normal),   D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, (UINT)offsetof(FNormalVertex, Color), D3D11_INPUT_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, (UINT)offsetof(FNormalVertex, TexCoord), D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};

	ID3D11VertexShader* NewUberVS = nullptr;
	ID3D11InputLayout* NewUberLayout = nullptr;
	ID3D11VertexShader* NewGouraudVS = nullptr;
	ID3D11InputLayout* NewGouraudLayout = nullptr;
	ID3D11PixelShader* NewUnlitPS = nullptr;
	ID3D11PixelShader* NewLambertPS = nullptr;
	ID3D11PixelShader* NewGouraudPS = nullptr;
	ID3D11PixelShader* NewPhongPS = nullptr;

	D3D_SHADER_MACRO DefinesGouraudVS[] = {
		{ "LIGHTING_MODEL_GOURAUD", "1" },
		{ nullptr, nullptr }
	};

	// 기본 Uber Vertex Shader
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		TargetShaderPath.c_str(), UberLitLayout, &NewUberVS, &NewUberLayout);

	// Gouraud 버전 Vertex Shader
	FRenderResourceFactory::CreateVertexShaderAndInputLayout(
		TargetShaderPath.c_str(), UberLitLayout, DefinesGouraudVS, &NewGouraudVS, &NewGouraudLayout);

	// Pixel Shader permutations
	FRenderResourceFactory::CreatePixelShader(TargetShaderPath.c_str(), &NewUnlitPS);

	D3D_SHADER_MACRO DefinesLambert[] = { { "LIGHTING_MODEL_LAMBERT", "1" }, { nullptr, nullptr } };
	FRenderResourceFactory::CreatePixelShader(TargetShaderPath.c_str(), DefinesLambert, &NewLambertPS);

	D3D_SHADER_MACRO DefinesGouraud[] = { { "LIGHTING_MODEL_GOURAUD", "1" }, { nullptr, nullptr } };
	FRenderResourceFactory::CreatePixelShader(TargetShaderPath.c_str(), DefinesGouraud, &NewGouraudPS);

	D3D_SHADER_MACRO DefinesPhong[] = { { "LIGHTING_MODEL_PHONG", "1" }, { nullptr, nullptr } };
	FRenderResourceFactory::CreatePixelShader(TargetShaderPath.c_str(), DefinesPhong, &NewPhongPS);

	// 컴파일 실패 시 리소스 정리
	if (!NewUberVS || !NewUberLayout || !NewGouraudVS || !NewGouraudLayout ||
		!NewUnlitPS || !NewLambertPS || !NewGouraudPS || !NewPhongPS)
	{
		SafeRelease(NewUberVS);
		SafeRelease(NewUberLayout);
		SafeRelease(NewGouraudVS);
		SafeRelease(NewGouraudLayout);
		SafeRelease(NewUnlitPS);
		SafeRelease(NewLambertPS);
		SafeRelease(NewGouraudPS);
		SafeRelease(NewPhongPS);
		return;
	}

	InRenderer.ApplyUberLitShaders(NewUberVS, NewUberLayout,
		NewGouraudVS, NewGouraudLayout,
		NewUnlitPS, NewLambertPS, NewGouraudPS, NewPhongPS);
}
