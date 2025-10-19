#include "pch.h"
#include "Manager/PostProcess/Public/FXAAManager.h"

IMPLEMENT_SINGLETON_CLASS_BASE(UFXAAManager)

UFXAAManager::UFXAAManager()
{
	// 기본값으로 Medium 품질 프리셋 적용
	ApplyMediumQualityPreset();
}

UFXAAManager::~UFXAAManager()
{
}

void UFXAAManager::ApplyLowQualityPreset()
{
	SubpixelBlend = 0.75f;
	EdgeThreshold = 0.25f;
	EdgeThresholdMin = 0.0833f;
}

void UFXAAManager::ApplyMediumQualityPreset()
{
	SubpixelBlend = 0.5f;
	EdgeThreshold = 0.125f;
	EdgeThresholdMin = 0.0312f;
}

void UFXAAManager::ApplyHighQualityPreset()
{
	SubpixelBlend = 0.25f;
	EdgeThreshold = 0.0625f;
	EdgeThresholdMin = 0.0156f;
}
