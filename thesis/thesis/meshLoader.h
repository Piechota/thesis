#pragma once
#include "pch.h"

struct SVertexData
{
	float m_position[3];
	float m_normal[3];
	float m_tangent[3];
	float m_uv[2];
};

class CMeshLoader
{
public:
	void LoadMesh(char const* path, UINT& verticesNum, SVertexData** outVertices, UINT& indicesNum, UINT32** outIndices);
};

extern CMeshLoader GMeshLoader;