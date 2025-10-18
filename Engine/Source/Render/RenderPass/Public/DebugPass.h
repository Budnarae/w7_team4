#pragma once
#include "RenderPass.h"

/**
 * @brief Debug primitives 렌더링 패스
 * RenderLevel의 Pass 처리 흐름에 통합되어 순서 제어 가능하도록 변경
 */
class FDebugPass :
	public FRenderPass
{
public:
	FDebugPass(UPipeline* InPipeline, ID3D11Buffer* InConstantBufferViewProj, ID3D11Buffer* InConstantBufferModel);
	~FDebugPass() override = default;

	void Execute(FRenderingContext& Context) override;
	void Release() override;
};
