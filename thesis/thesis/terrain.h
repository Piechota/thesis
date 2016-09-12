#pragma once
#include "pch.h"
#include "constantBuffers.h"
 
#define TOP_LOD 0x0001
#define BOTTOM_LOD 0x0002
#define RIGHT_LOD 0x0004
#define LEFT_LOD 0x0008
#define TOP_LEFT_LOD 0x0010
#define TOP_RIGHT_LOD 0x0020
#define BOTTOM_LEFT_LOD 0x0040
#define BOTTOM_RIGHT_LOD 0x0080

#define TOP_LOW_LOD 0x0100
#define BOTTOM_LOW_LOD 0x0200
#define RIGHT_LOW_LOD 0x0400
#define LEFT_LOW_LOD 0x0800
#define TOP_LEFT_LOW_LOD 0x1000
#define TOP_RIGHT_LOW_LOD 0x2000
#define BOTTOM_LEFT_LOW_LOD 0x4000
#define BOTTOM_RIGHT_LOW_LOD 0x8000

#define TOP_TILE 0x00010000
#define BOTTOM_TILE 0x00020000
#define RIGHT_TILE 0x00040000
#define LEFT_TILE 0x00080000
#define TOP_LEFT_TILE TOP_TILE | LEFT_TILE
#define TOP_RIGHT_TILE TOP_TILE | RIGHT_TILE
#define BOTTOM_LEFT_TILE BOTTOM_TILE | LEFT_TILE
#define BOTTOM_RIGHT_TILE BOTTOM_TILE | RIGHT_TILE

#define UPDATE_RIGHT_EDGE 0x00100000
#define UPDATE_BOTTOM_EDGE 0x00200000
#define UPDATE_LT_CORNER 0x01000000
#define UPDATE_LB_CORNER 0x02000000
#define UPDATE_RB_CORNER 0x04000000

#define EDGE_LOD_MASK 0x0000FFFF
#define EDGE_UPDATE_MASK 0xFFF00000

enum ELods
{
	LOD0,
	LOD1,
	LOD2,
	MAX
};

UINT const GTerrainLodInfo[3][2] =
{
	{ 1024, 32 },
	{ 256, 16 },
	{ 64, 8 }
	//{64, 8},
	//{16, 4},
	//{4, 2}
};

struct STileData
{
	Vec3 m_centerPosition;
	UINT m_verticesOffset;
	ELods m_lod;
	bool m_needUpdate;
};

class CTerrain
{
public:
	ID3D12Resource* m_indices[ELods::MAX];
	D3D12_INDEX_BUFFER_VIEW m_indicesView[ELods::MAX];
	UINT m_indicesNum[ELods::MAX];

	ID3D12Resource* m_vertices;
	D3D12_VERTEX_BUFFER_VIEW m_verticesView;

	ID3D12Resource* m_heightmap;

	ID3D12Resource*	m_gpuTileData;
	TileGenCB*	m_pGpuTileData;

	ID3D12Resource* m_diffuseTex;
	ID3D12Resource* m_normalTex;

	ID3D12RootSignature* m_terrainRS;
	ID3D12PipelineState* m_terrainPosPSO;
	ID3D12PipelineState* m_terrainLightPSOPass0;
	ID3D12PipelineState* m_terrainLightPSOPass1;

	std::vector< STileData > m_tilesData;

	Matrix4x4 m_objectToWorld;
};