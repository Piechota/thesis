#pragma once
#include "pch.h"

__declspec(align(256))
struct FrameCB
{
	Matrix4x4 m_worldToProject;
};
