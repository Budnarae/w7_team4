#pragma once
#include "Core/Public/Object.h"
#include "Global/CoreTypes.h"
#include "Editor/Public/Lines.h"

/**
 * @brief Cone 와이어프레임을 위한 정점 관리 클래스
 * - SemiLightActor의 Decal 투사 범위를 시각화
 * - SpotAngle과 DecalBox 크기에 따라 가변적인 Cone 형태 생성
 */
class UConeLines : public ULines
{
public:
	UConeLines();
	~UConeLines() override;

	/**
	 * @brief Cone 와이어프레임 정점 업데이트
	 * @param Apex Cone의 꼭지점 (광원 위치)
	 * @param Direction Cone의 투사 방향 (정규화된 벡터)
	 * @param UpVector Cone의 Up 벡터 (회전 기준)
	 * @param Angle Cone의 각도 (degree)
	 * @param DecalBoxSize 데칼 박스 크기 (X: 깊이, Y/Z: 반지름)
	 */
	void UpdateVertices(const FVector& Apex, const FVector& Direction, const FVector& UpVector,
	                    float Angle, const FVector& DecalBoxSize);

	/**
	 * @brief Cone 와이어프레임의 세그먼트 수 반환
	 */
	uint32 GetNumSegments() const
	{
		return NumSegments;
	}

	/**
	 * @brief Cone 와이어프레임을 비활성화 (크기 0으로 설정)
	 */
	void Disable();

private:
	uint32 NumSegments = 32;  // 바닥면 원을 구성하는 세그먼트 수 (부드러운 원을 위해)
	bool bIsEnabled = false;
};
