#include "pch.h"
#include "Global/Quaternion.h"

FQuaternion FQuaternion::FromAxisAngle(const FVector& Axis, float AngleRad)
{
	FVector N = Axis;
	N.Normalize();
	float s = sinf(AngleRad * 0.5f);
	return {
		N.X * s,
		N.Y * s,
		N.Z * s,
		cosf(AngleRad * 0.5f)
	};
}

FQuaternion FQuaternion::FromEuler(const FVector& EulerDeg)
{
	// EulerDeg: X=Roll, Y=Pitch, Z=Yaw (degrees)
	// Unreal Engine rotation order: Yaw → Pitch → Roll
	FVector Radians = FVector::GetDegreeToRadian(EulerDeg);

	float roll = Radians.X;
	float pitch = Radians.Y;
	float yaw = Radians.Z;

	float cr = cosf(roll * 0.5f);
	float sr = sinf(roll * 0.5f);
	float cp = cosf(pitch * 0.5f);
	float sp = sinf(pitch * 0.5f);
	float cy = cosf(yaw * 0.5f);
	float sy = sinf(yaw * 0.5f);

	// Rotation order: Yaw(Z) → Pitch(Y) → Roll(X)
	// Reference 참고: UE Rotator.h Quaternion() 변환
	return {
		cr * sp * sy - sr * cp * cy, // X
		-cr * sp * cy - sr * cp * sy, // Y
		cr * cp * sy - sr * sp * cy, // Z
		cr * cp * cy + sr * sp * sy  // W
	};
}

FVector FQuaternion::ToEuler() const
{
	// Convert Quaternion to Euler angles
	// Return: X=Roll, Y=Pitch, Z=Yaw (degrees)

	FVector Euler;

	// Singularity test
	const float SingularityTest = Z * X - W * Y;
	const float YawY = 2.0f * (W * Z + X * Y);
	const float YawX = 1.0f - 2.0f * (Y * Y + Z * Z);

	constexpr float SINGULARITY_THRESHOLD = 0.4999995f;
	constexpr float RAD_TO_DEG = 180.0f / PI;

	if (SingularityTest < -SINGULARITY_THRESHOLD)
	{
		// South pole singularity
		Euler.X = 0.0f; // Roll
		Euler.Y = -90.0f; // Pitch
		Euler.Z = -atan2f(YawY, YawX) * RAD_TO_DEG; // Yaw
	}
	else if (SingularityTest > SINGULARITY_THRESHOLD)
	{
		// North pole singularity
		Euler.X = 0.0f; // Roll
		Euler.Y = 90.0f; // Pitch
		Euler.Z = atan2f(YawY, YawX) * RAD_TO_DEG; // Yaw
	}
	else
	{
		// Standard conversion
		Euler.X = atan2f(-2.0f * (W * X + Y * Z), 1.0f - 2.0f * (X * X + Y * Y)) * RAD_TO_DEG; // Roll
		Euler.Y = asinf(2.0f * SingularityTest) * RAD_TO_DEG; // Pitch
		Euler.Z = atan2f(YawY, YawX) * RAD_TO_DEG; // Yaw
	}

	return Euler;
}

FQuaternion FQuaternion::operator*(const FQuaternion& Q) const
{
	return {
		W * Q.X + X * Q.W + Y * Q.Z - Z * Q.Y,
		W * Q.Y - X * Q.Z + Y * Q.W + Z * Q.X,
		W * Q.Z + X * Q.Y - Y * Q.X + Z * Q.W,
		W * Q.W - X * Q.X - Y * Q.Y - Z * Q.Z
	};
}

void FQuaternion::Normalize()
{
	float mag = sqrtf(X * X + Y * Y + Z * Z + W * W);
	if (mag > 0.0001f)
	{
		X /= mag;
		Y /= mag;
		Z /= mag;
		W /= mag;
	}
}

FVector FQuaternion::RotateVector(const FQuaternion& q, const FVector& v)
{
	FQuaternion p(v.X, v.Y, v.Z, 0.0f);
	FQuaternion r = q * p * q.Inverse();
	return {r.X, r.Y, r.Z};
}

FVector FQuaternion::RotateVector(const FVector& v) const
{
	FQuaternion p(v.X, v.Y, v.Z, 0.0f);
	FQuaternion r = (*this) * p * this->Inverse();
	return {r.X, r.Y, r.Z};
}
