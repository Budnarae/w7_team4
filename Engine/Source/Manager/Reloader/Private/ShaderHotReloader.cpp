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
	std::error_code ErrorCode;

	// 디렉터리 존재 여부 확인
	if (!std::filesystem::exists(WatchDirectory, ErrorCode) ||
		!std::filesystem::is_directory(WatchDirectory, ErrorCode))
	{
		return;
	}

	// 재귀적으로 모든 파일 탐색
	for (auto DirectoryIterator = std::filesystem::recursive_directory_iterator(
		WatchDirectory,
		std::filesystem::directory_options::skip_permission_denied,
		ErrorCode);
		DirectoryIterator != std::filesystem::recursive_directory_iterator();
		DirectoryIterator.increment(ErrorCode))
	{
		if (ErrorCode)
		{
			ErrorCode.clear();
			continue;
		}

		const auto& DirectoryEntry = *DirectoryIterator;
		if (!DirectoryEntry.is_regular_file(ErrorCode))
		{
			ErrorCode.clear();
			continue;
		}

		const auto& FilePath = DirectoryEntry.path();
		const auto& FileExtension = FilePath.extension();

		//  확장자 필터
		if (FileExtension != L".hlsl" && FileExtension != L".hlsli")
		{
			continue;
		}

		// 마지막 수정 시간 기록
		auto LastWriteTime = std::filesystem::last_write_time(FilePath, ErrorCode);
		if (!ErrorCode)
		{
			FileTimeCache[FilePath.wstring()] = LastWriteTime;
		}

		ErrorCode.clear();
	}

	LastScanTime = std::chrono::steady_clock::now();
	bInitialized = true;
}


void UShaderHotReloader::Tick(URenderer& InRenderer)
{
    if (!bInitialized)
        return;

    auto now = std::chrono::steady_clock::now();
    if (now - LastScanTime < ScanInterval)
        return;
    LastScanTime = now;

    if (HasShaderDirectoryChanged())
    {
        RebuildUberLitShaders(InRenderer);
    }
}

void UShaderHotReloader::RebuildUberLitShaders(URenderer& InRenderer)
{
    UE_LOG("ShaderHotReloader: Rebuilding UberLit shaders...");

    // Input layout shared by UberLit VS variants
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

    // VS: Gouraud define
    D3D_SHADER_MACRO DefinesGouraudVS[] = {
        { "LIGHTING_MODEL_GOURAUD", "1" },
        { nullptr, nullptr }
    };

    // Compile/create
    FRenderResourceFactory::CreateVertexShaderAndInputLayout(
        L"Asset/Shader/UberLit.hlsl", UberLitLayout, &NewUberVS, &NewUberLayout);

    FRenderResourceFactory::CreateVertexShaderAndInputLayout(
        L"Asset/Shader/UberLit.hlsl", UberLitLayout, DefinesGouraudVS, &NewGouraudVS, &NewGouraudLayout);

    // PS permutations
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", &NewUnlitPS);

    D3D_SHADER_MACRO DefinesLambert[] = {
        { "LIGHTING_MODEL_LAMBERT", "1" },
        { nullptr, nullptr }
    };
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", DefinesLambert, &NewLambertPS);

    D3D_SHADER_MACRO DefinesGouraud[] = {
        { "LIGHTING_MODEL_GOURAUD", "1" },
        { nullptr, nullptr }
    };
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", DefinesGouraud, &NewGouraudPS);

    D3D_SHADER_MACRO DefinesPhong[] = {
        { "LIGHTING_MODEL_PHONG", "1" },
        { nullptr, nullptr }
    };
    FRenderResourceFactory::CreatePixelShader(L"Asset/Shader/UberLit.hlsl", DefinesPhong, &NewPhongPS);

    if (!NewUberVS || !NewUberLayout || !NewGouraudVS || !NewGouraudLayout || !NewUnlitPS || !NewLambertPS || !NewGouraudPS || !NewPhongPS)
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

    InRenderer.ApplyUberLitShaders( NewUberVS, NewUberLayout,NewGouraudVS, NewGouraudLayout, NewUnlitPS, NewLambertPS, NewGouraudPS, NewPhongPS);
}

bool UShaderHotReloader::HasShaderDirectoryChanged()
{
	bool bHasChanged = false;
	std::error_code ErrorCode;

	// 디렉터리 유효성 검사
	if (!std::filesystem::exists(WatchDirectory, ErrorCode) ||
		!std::filesystem::is_directory(WatchDirectory, ErrorCode))
	{
		return false;
	}

	// 디렉터리 순회 설정
	for (auto DirectoryIterator = std::filesystem::recursive_directory_iterator(
		WatchDirectory,
		std::filesystem::directory_options::skip_permission_denied,
		ErrorCode);
		DirectoryIterator != std::filesystem::recursive_directory_iterator();
		DirectoryIterator.increment(ErrorCode))
	{
		if (ErrorCode)
		{
			ErrorCode.clear();
			continue;
		}

		const auto& DirectoryEntry = *DirectoryIterator;
		if (!DirectoryEntry.is_regular_file(ErrorCode))
		{
			ErrorCode.clear();
			continue;
		}

		const auto& FilePath = DirectoryEntry.path();
		const auto& FileExtension = FilePath.extension();

		// 셰이더 파일 필터
		if (FileExtension != L".hlsl" && FileExtension != L".hlsli")
		{
			continue;
		}

		auto CurrentWriteTime = std::filesystem::last_write_time(FilePath, ErrorCode);
		if (ErrorCode)
		{
			ErrorCode.clear();
			continue;
		}

		const std::wstring FileKey = FilePath.wstring();
		auto CachedFileIterator = FileTimeCache.find(FileKey);

		// 신규 파일이거나 수정된 파일일 경우 캐시 갱신
		if (CachedFileIterator == FileTimeCache.end())
		{
			FileTimeCache[FileKey] = CurrentWriteTime;
			bHasChanged = true;
		}
		else if (CachedFileIterator->second != CurrentWriteTime)
		{
			CachedFileIterator->second = CurrentWriteTime;
			bHasChanged = true;
		}
	}

	return bHasChanged;
}

