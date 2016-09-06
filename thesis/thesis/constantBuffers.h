#pragma once
#include "pch.h"

#define PADDING float padding##_COUNTER_;

__declspec(align(256))
struct FrameCB
{
	Matrix4x4 m_worldToProject;
	Matrix4x4 m_objectToProject;
};

__declspec(align(256))
struct TileGenCB
{
	Vec3 m_heightmapRect;
	float m_worldTileSize;
	float m_terrainHeight;
	float m_uvQuadSize;
	Vec2 m_tilePosition;
	Vec2u m_heightmapRes;
	UINT m_verticesOnEdge;
	UINT m_quadsOnEdge;
	UINT m_edgesData;
	UINT m_dataOffset;
};
