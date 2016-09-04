#pragma once
#include "pch.h"

struct SSkybox
{
	ID3D12PipelineState*		m_skyboxPSO;
	ID3D12Resource*				m_texture;
	ID3D12Resource*				m_meshVertices;
	ID3D12Resource*				m_meshInidces;

	D3D12_INDEX_BUFFER_VIEW		m_indicesBufferView;
	D3D12_VERTEX_BUFFER_VIEW	m_verticesBufferView;

	UINT						m_indicesNum;
};