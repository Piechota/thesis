#include "camera.h"
#include "input.h"
#include "timer.h"

CCamera::CCamera()
{
	m_rotation = Quaternion::CreateQuaternionIdentity();
	m_position = Vec3(0.f, 0.f, 0.f);
}

void CCamera::Update()
{
	static Vec2i const midPos(GWidth / 2, GHeight / 2);
	Vec2i mouseDelta;
	GInputManager.GetMousePosition(mouseDelta);
	mouseDelta -= midPos;

	m_rotation *= Quaternion::CreateRotation(Vec4(0.f, 1.f, 0.f, 0.f), (float)(mouseDelta.x) *  2.f / (float)(GWidth));
	m_rotation = Quaternion::CreateRotation(Vec4(1.f, 0.f, 0.f, 0.f), (float)(mouseDelta.y) *  2.f / (float)(GHeight)) * m_rotation;

	GInputManager.SetMousePosition(midPos);

	Vec3 dir(0.f, 0.f, 0.f);

	if (GInputManager.IsKeyDown('W'))
	{
		dir += GetForward();
	}
	if (GInputManager.IsKeyDown('S'))
	{
		dir += GetBackward();
	}

	if (GInputManager.IsKeyDown('A'))
	{
		dir += GetLeft();
	}
	if (GInputManager.IsKeyDown('D'))
	{
		dir += GetRight();
	}

	if (dir.LengthSq() > 0.f)
	{
		dir.Normalize();
		float const speed = GInputManager.IsKeyDown(K_SPACE) ? 100.f : 2.f;
		m_position += dir * GTimer.Delta() * speed;
	}
}
