#pragma once
#include "Core/Public/Object.h"
#include <filesystem>
#include <chrono>

class URenderer;

UCLASS()
class UShaderHotReloader : public UObject
{
	GENERATED_BODY()
	DECLARE_SINGLETON_CLASS(UShaderHotReloader, UObject)

public:
	// 감시할 디렉터리 초기화 (기본: Asset/Shader)
	void Initialize(const std::wstring& InWatchDirectory = L"Asset/Shader");

	// 매 프레임 또는 일정 주기로 호출 (변경 감지 및 리빌드)
	void Tick(URenderer& InRenderer);

private:
	// UberLit 셰이더 재컴파일 및 적용
	void RebuildUberLitShaders(URenderer& InRenderer);

	// UberLit.hlsl 변경 감지
	bool HasUberShaderChanged();

private:
	// 감시할 폴더 및 대상 셰이더 경로
	std::wstring WatchDirectory;
	std::wstring TargetShaderPath;

	// 초기화 여부
	bool bInitialized{ false };

	// 마지막 수정 시각
	std::filesystem::file_time_type LastWriteTime{};

	// 마지막 스캔 시각 (Tick 주기 관리)
	std::chrono::steady_clock::time_point LastScanTime{};

	// 스캔 주기 (예: 0.35초마다 체크)
	std::chrono::milliseconds ScanInterval{ 350 };
};
