#pragma once
#include "Core/Public/Object.h"
#include <unordered_map>
#include <filesystem>
#include <chrono>

class URenderer;

UCLASS()
class UShaderHotReloader : public UObject
{
    GENERATED_BODY()
    DECLARE_SINGLETON_CLASS(UShaderHotReloader, UObject)

public:
    //UShaderHotReloader() = default;
	//~UShaderHotReloader();
    void Initialize(const std::wstring& InWatchDir = L"Asset/Shader");

    void Tick(URenderer& InRenderer);

private:
    // Rebuild & swap shader resources triggered by file changes
    void RebuildUberLitShaders(URenderer& InRenderer);
    bool HasShaderDirectoryChanged();

private:
    std::wstring WatchDirectory;
    bool bInitialized{false};

    // File timestamp cache
    TMap<std::wstring, std::filesystem::file_time_type> FileTimeCache;

    // Throttle scanning
    std::chrono::steady_clock::time_point LastScanTime{};
    std::chrono::milliseconds ScanInterval{350};
};
