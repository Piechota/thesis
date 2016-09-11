#pragma once
#include "pch.h"

#define PADDING_X( x ) float padding##x
#define PADDING_Y( y ) PADDING_X( y )
#define PADDING PADDING_Y( __COUNTER__ )

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
	UINT m_neighborsTilesLeft;
	UINT m_neighborsTilesRight;
	UINT m_neighborsTilesTop;
	UINT m_neighborsTilesBottom;
	UINT m_neighborsTilesTopLeft;
	UINT m_neighborsTilesTopRight;
	UINT m_neighborsTilesBottomLeft;
	UINT m_neighborsTilesBottomRight;
	UINT m_edgesData;
	UINT m_dataOffset;
};

__declspec(align(256))
struct ObjectBufferCB
{
	Matrix4x4 m_worldToProject;
	Matrix4x4 m_objectToWorld;
	Vec3 m_lightDir;
	PADDING;
	Vec3 m_lightColor;
	float m_lightIntensity;
	Vec3 m_ambientColor;
};
