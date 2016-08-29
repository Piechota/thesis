#pragma once
#include "pch.h"
struct STexutre
{
	UINT64 m_x[D3D12_REQ_MIP_LEVELS];
	UINT m_y[D3D12_REQ_MIP_LEVELS];
	UINT16 m_mipNum;
	BYTE* m_data[D3D12_REQ_MIP_LEVELS];
	DXGI_FORMAT m_format;
};

class CTextureLoader
{
public:
	void* LoadTexture(char const* path, STexutre& outTexture);
};

extern CTextureLoader GTextureLoader;