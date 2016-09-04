#pragma once
#include "pch.h"

#define VECTOR_BY_QUATERNION(vec) Vec4 const qVec = Vec4( m_rotation.x, m_rotation.y, m_rotation.z, 0.f ); \
Vec4 t = 2.f * Vec4::Cross( qVec, vec ); \
Vec4 fp = vec + m_rotation.w * t + Vec4::Cross( qVec, t ); \
return Vec3( fp.x, fp.y, fp.z );

class CCamera
{
private:
	Matrix4x4 m_projMatrix;
	Quaternion m_rotation;
	Vec3 m_position;

public:
	CCamera();

	Matrix4x4 GetWorldToCamera() const
	{
		return Matrix4x4::Inverse(
			Matrix4x4::CreateFromQuaternion(m_rotation) *
			Matrix4x4::CreateTranslateMatrix(m_position)
		);
	}

	void SetPerspective(float const fovY, float const aspect, float const nearZ, float const farZ)
	{
		m_projMatrix = Matrix4x4::CreatePerspectiveFovLH(fovY, aspect, nearZ, farZ);
	}
	Matrix4x4 GetProjectionMatrix() const
	{
		return m_projMatrix;
	}

	void Update();

	Vec3 GetForward() const
	{
		static Vec4 const forward = Vec4(0.f, 0.f, 1.f, 0.f);
		VECTOR_BY_QUATERNION(forward);
	}
	Vec3 GetRight() const
	{
		static Vec4 const right = Vec4(1.f, 0.f, 0.f, 0.f);
		VECTOR_BY_QUATERNION(right);
	}
	Vec3 GetUp() const
	{
		static Vec4 const right = Vec4(0.f, 1.f, 0.f, 0.f);
		VECTOR_BY_QUATERNION(right);
	}
	Vec3 GetBackward() const
	{
		static Vec4 const forward = Vec4(0.f, 0.f, -1.f, 0.f);
		VECTOR_BY_QUATERNION(forward);
	}
	Vec3 GetLeft() const
	{
		static Vec4 const right = Vec4(-1.f, 0.f, 0.f, 0.f);
		VECTOR_BY_QUATERNION(right);
	}
	Vec3 GetDown() const
	{
		static Vec4 const right = Vec4(0.f, -1.f, 0.f, 0.f);
		VECTOR_BY_QUATERNION(right);
	}
};

extern CCamera GCamera;
