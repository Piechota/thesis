#include "textureLoader.h"
#include "../DirectXTex/DirectXTex.h"

CTextureLoader GTextureLoader;

void* CTextureLoader::LoadTexture(char const* path, STexutre& outTexture)
{
	wchar_t* wfile = Char2WChar(path);
	DirectX::TexMetadata meta;
	DirectX::ScratchImage* cpuImg = new DirectX::ScratchImage();
	CheckFailed(LoadFromDDSFile(wfile, DirectX::DDS_FLAGS_NONE, &meta, *cpuImg));
	delete wfile;

	outTexture.m_mipNum = (UINT16)meta.mipLevels;
	outTexture.m_format = meta.format;

	for (UINT mipID = 0; mipID < outTexture.m_mipNum; ++mipID)
	{
		DirectX::Image const* mip = cpuImg->GetImage(mipID, 0, 0);
		outTexture.m_data[mipID] = mip->pixels;
		outTexture.m_x[mipID] = mip->width;
		outTexture.m_y[mipID] = mip->height;
	}

	return cpuImg;
}
