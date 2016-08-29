#pragma once
#include "pch.h"

struct SRenderFrame
{
	ID3D12CommandAllocator*		m_commandAllocator;
	ID3D12GraphicsCommandList*	m_commandList;
	ID3D12Resource*				m_frameResource;
	void*						m_pResource;
};