#include "pch.h"
#include "Editor/Public/SphereLines.h"

USphereLines::USphereLines()
{
	NumVertices = Segments * 3; // XY, XZ, YZ ��� ������ ���� Segments ������ ����
	Vertices.reserve(NumVertices);
	Vertices.resize(NumVertices);
}

void USphereLines::UpdateVertices(const FVector& CenterPosition, float Radius)
{
	// 3���� ��(XY, XZ, YZ)�� ���� ���� �׷� ���� ǥ���մϴ�.
	
	for (int32 i = 0; i < Segments; ++i)
	{
		const float Angle = static_cast<float>(i) / Segments * 2.0f * PI;
		const float Sin = sinf(Angle);
		const float Cos = cosf(Angle);

		// XY ����� ��
		FVector Point_XY = CenterPosition + FVector(Radius * Cos, Radius * Sin, 0.f);
		Vertices[i * 3] = Point_XY;

		// XZ ����� ��
		FVector Point_XZ = CenterPosition + FVector(Radius * Cos, 0.f, Radius * Sin);
		Vertices[i * 3 + 1] = Point_XZ;

		// YZ ����� ��
		FVector Point_YZ = CenterPosition + FVector(0.f, Radius * Cos, Radius * Sin);
		Vertices[i * 3 + 2] = Point_YZ;
	}
}