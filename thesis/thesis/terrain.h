#pragma once
#include "pch.h"
#include "constantBuffers.h"
 
#define TOP_LOD 0x0001
#define BOTTOM_LOD 0x0002
#define RIGHT_LOD 0x0004
#define LEFT_LOD 0x0008

#define TOP_TILE 0x0100
#define BOTTOM_TILE 0x0200
#define RIGHT_TILE 0x0400
#define LEFT_TILE 0x0800

#define EDGE_LOD_MASK 0x00FF

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

	ID3D12RootSignature* m_terrainRS;
	ID3D12PipelineState* m_terrainPosPSO;
	ID3D12PipelineState* m_terrainLightPSO;

	std::vector< STileData > m_tilesData;
};