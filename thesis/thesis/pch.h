#pragma once

#ifdef _DEBUG
#pragma comment(lib, "../DirectXTex/Bin/Desktop_2015/x64/Debug/DirectXTex.lib")
#else
#pragma comment(lib, "../DirectXTex/Bin/Desktop_2015/x64/Release/DirectXTex.lib")
#endif
#pragma comment( lib, "d3d12" )
#pragma comment( lib, "dxgi" )
#pragma comment( lib, "d3dcompiler" )
#pragma comment( lib, "d3dcompiler" )

#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>

#include <string>
#include <vector>
#include <comdef.h>
#include <intrin.h>