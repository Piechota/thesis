#include "camera.h"
#include "render.h"
#include "constantBuffers.h"
#include "meshLoader.h"
#include "heapProperties.h"
#include "timer.h"

UINT const GTerrainEdgeTiles = 5;
UINT const GTerrainTilesNum = GTerrainEdgeTiles * GTerrainEdgeTiles;
float const GTerrainLoad0Sq = 200.f * 200.f;
float const GTerrainLoad1Sq = 400.f * 400.f;
Vec2 const GTerrainSize(400.f, 100.f);
bool GUpdateTerrain = true;
bool GWireframe = false;

ID3D12Resource* CRender::CopyTexture(STexutre const& texture, ID3D12Resource** resource)
{
	D3D12_RESOURCE_DESC descTexture = {};
	descTexture.DepthOrArraySize = 1;
	descTexture.SampleDesc.Count = 1;
	descTexture.Flags = D3D12_RESOURCE_FLAG_NONE;
	descTexture.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	descTexture.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	descTexture.MipLevels = texture.m_mipNum;
	descTexture.Width = texture.m_x[0];
	descTexture.Height = texture.m_y[0];
	descTexture.Format = texture.m_format;

	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &descTexture, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(resource)));

	UINT64 bufferSize;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pFootprints = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[texture.m_mipNum];
	UINT* pNumRows = new UINT[texture.m_mipNum];
	UINT64* pRowPitches = new UINT64[texture.m_mipNum];
	m_device->GetCopyableFootprints(&descTexture, 0, texture.m_mipNum, 0, pFootprints, pNumRows, pRowPitches, &bufferSize);

	descTexture.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	descTexture.Format = DXGI_FORMAT_UNKNOWN;
	descTexture.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	descTexture.MipLevels = 1;
	descTexture.Height = 1;
	descTexture.Width = bufferSize;

	ID3D12Resource* textureUploadRes;
	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &descTexture, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&textureUploadRes)));

	void* pGPU;
	textureUploadRes->Map(0, nullptr, &pGPU);
	for (UINT mipID = 0; mipID < texture.m_mipNum; ++mipID)
	{
		for (UINT rowID = 0; rowID < pNumRows[mipID]; ++rowID)
		{
			memcpy((BYTE*)pGPU + pFootprints[mipID].Offset + rowID * pFootprints[mipID].Footprint.RowPitch, texture.m_data[mipID] + rowID * pRowPitches[mipID], pRowPitches[mipID]);
		}

		D3D12_TEXTURE_COPY_LOCATION dst;
		dst.pResource = *resource;
		dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
		dst.SubresourceIndex = mipID;

		D3D12_TEXTURE_COPY_LOCATION src;
		src.pResource = textureUploadRes;
		src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
		src.PlacedFootprint = pFootprints[mipID];

		m_copyCL->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
	}
	textureUploadRes->Unmap(0, nullptr);

	delete pFootprints;
	delete pNumRows;
	delete pRowPitches;

	return textureUploadRes;
}

void CRender::LoadShader(LPCWSTR pFileName, D3D_SHADER_MACRO const* pDefines, ID3DInclude* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, ID3DBlob** ppCode)
{
	ID3DBlob* error;
	HRESULT result = D3DCompileFromFile(pFileName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, 0, ppCode, &error);
	if (FAILED(result))
	{
		if (error != nullptr)
			OutputDebugStringA((char*)error->GetBufferPointer());
		throw;
	}
}

void CRender::InitCommands()
{
	D3D12_COMMAND_QUEUE_DESC descCQ = {};
	descCQ.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	descCQ.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	CheckFailed(m_device->CreateCommandQueue(&descCQ, IID_PPV_ARGS(&m_mainCQ)));
	m_mainCQ->SetName(L"MainCommandQueue");

	descCQ.Type = D3D12_COMMAND_LIST_TYPE_COPY;
	CheckFailed(m_device->CreateCommandQueue(&descCQ, IID_PPV_ARGS(&m_copyCQ)));
	m_copyCQ->SetName(L"CopyCommandQueue");

	descCQ.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
	CheckFailed(m_device->CreateCommandQueue(&descCQ, IID_PPV_ARGS(&m_computeCQ)));
	m_copyCQ->SetName(L"ComputeCommandQueue");

	CheckFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_mainCA)));
	m_mainCA->SetName(L"MainCommandAllocator");
	CheckFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&m_copyCA)));
	m_copyCA->SetName(L"CopyCommandAllocator");
	CheckFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&m_computeCA)));
	m_computeCA->SetName(L"ComputeCommandAllocator");

	CheckFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_mainCA, nullptr, IID_PPV_ARGS(&m_mainCL)));
	m_mainCL->SetName(L"MainCommandList");
	m_mainCL->Close();
	CheckFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, m_copyCA, nullptr, IID_PPV_ARGS(&m_copyCL)));
	m_copyCL->SetName(L"CopyCommandList");
	m_copyCL->Close();
	CheckFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COMPUTE, m_computeCA, nullptr, IID_PPV_ARGS(&m_computeCL)));
	m_computeCL->SetName(L"ComputeCommandList");
	m_computeCL->Close();
}

void CRender::InitSwapChain()
{
	DXGI_SWAP_CHAIN_DESC descSwapChain = {};
	descSwapChain.BufferCount = FRAME_NUM;
	descSwapChain.BufferDesc.Width = GWidth;
	descSwapChain.BufferDesc.Height = GHeight;
	descSwapChain.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	descSwapChain.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	descSwapChain.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	descSwapChain.OutputWindow = GHWnd;
	descSwapChain.SampleDesc.Count = 1;
	descSwapChain.Windowed = TRUE;
	
	IDXGISwapChain* swapChain;
	CheckFailed(m_factor->CreateSwapChain(m_mainCQ, &descSwapChain, &swapChain));
	m_swapChain = (IDXGISwapChain3*)swapChain;
}

void CRender::InitRenderTargets()
{
	m_rtvDescriptorHandleIncrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	m_srvDescriptorHandleIncrementSize = m_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	D3D12_DESCRIPTOR_HEAP_DESC descDescHeap = {};
	descDescHeap.NumDescriptors = FRAME_NUM;
	descDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	descDescHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	CheckFailed(m_device->CreateDescriptorHeap(&descDescHeap, IID_PPV_ARGS(&m_renderTargetDH)));
	m_renderTargetDH->SetName(L"RenderTargetsDescriptorHeaps");

	descDescHeap.NumDescriptors = 1;
	descDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	CheckFailed(m_device->CreateDescriptorHeap(&descDescHeap, IID_PPV_ARGS(&m_depthDH)));
	m_depthDH->SetName(L"DepthDescriptorHeap");

	D3D12_CPU_DESCRIPTOR_HANDLE descHandle = m_renderTargetDH->GetCPUDescriptorHandleForHeapStart();
	for (UINT rtvID = 0; rtvID < FRAME_NUM; ++rtvID)
	{
		CheckFailed(m_swapChain->GetBuffer(rtvID, IID_PPV_ARGS(&m_rederTarget[rtvID])));
		m_device->CreateRenderTargetView(m_rederTarget[rtvID], nullptr, descHandle);
		std::wstring const rtvName = L"RenderTarget" + std::to_wstring(rtvID);
		m_rederTarget[rtvID]->SetName(rtvName.c_str());
		descHandle.ptr += m_rtvDescriptorHandleIncrementSize;
	}

	D3D12_CLEAR_VALUE depthOptimizedClearValue = {};
	depthOptimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
	depthOptimizedClearValue.DepthStencil.Depth = 1.f;
	depthOptimizedClearValue.DepthStencil.Stencil = 0;
	
	D3D12_RESOURCE_DESC descResource = {};
	descResource.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	descResource.Width = GWidth;
	descResource.Height = GHeight;
	descResource.DepthOrArraySize = 1;
	descResource.MipLevels = 1;
	descResource.Format = DXGI_FORMAT_D32_FLOAT;
	descResource.SampleDesc.Count = 1;
	descResource.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	descResource.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesGPUOnly, D3D12_HEAP_FLAG_NONE, &descResource, D3D12_RESOURCE_STATE_DEPTH_WRITE, &depthOptimizedClearValue, IID_PPV_ARGS(&m_depthTarget)));
	m_depthTarget->SetName(L"DepthTarget");

	D3D12_DEPTH_STENCIL_VIEW_DESC descDSV = {};
	descDSV.Format = DXGI_FORMAT_D32_FLOAT;
	descDSV.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	descDSV.Flags = D3D12_DSV_FLAG_NONE;
	m_device->CreateDepthStencilView(m_depthTarget, &descDSV, m_depthDH->GetCPUDescriptorHandleForHeapStart());

	m_renderTargetID = 0;
}

void CRender::InitRenderFrames()
{
	D3D12_RESOURCE_DESC descResource = {};
	descResource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	descResource.Alignment = 0;
	descResource.Height = 1;
	descResource.DepthOrArraySize = 1;
	descResource.MipLevels = 1;
	descResource.Format = DXGI_FORMAT_UNKNOWN;
	descResource.SampleDesc.Count = 1;
	descResource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	descResource.Flags = D3D12_RESOURCE_FLAG_NONE;
	descResource.Width = sizeof(ObjectBufferCB);

	Vec3 lightDir = Vec3(-1.f, 1.f, -1.f);
	lightDir.Normalize();

	for (UINT frameID = 0; frameID < FRAME_NUM; ++frameID)
	{
		SRenderFrame& renderFrame = m_renderFrames[frameID];
		
		CheckFailed(m_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&renderFrame.m_commandAllocator)));
		renderFrame.m_commandAllocator->SetName(L"FrameCommandAllocator");

		CheckFailed(m_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderFrame.m_commandAllocator, nullptr, IID_PPV_ARGS(&renderFrame.m_commandList)));
		renderFrame.m_commandList->SetName(L"FrameCommandList");
		CheckFailed(renderFrame.m_commandList->Close());

		CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &descResource, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&renderFrame.m_frameResource)));
		renderFrame.m_frameResource->SetName(L"FrameResource");

		CheckFailed(renderFrame.m_frameResource->Map(0, nullptr, &renderFrame.m_pResource));

		ObjectBufferCB* objectBuffer = (ObjectBufferCB*)(renderFrame.m_pResource);
		objectBuffer->m_lightDir = lightDir;
		objectBuffer->m_lightColor.Set(0.5f, 0.5f, 0.5f);
		objectBuffer->m_lightIntensity = 1.f;
		objectBuffer->m_ambientColor.Set(0.1f, 0.1f, 0.1f);
	}

	m_renderFrameID = 0;
	m_pRenderFrame = &m_renderFrames[m_renderFrameID];
}

void CRender::InitRootSignature()
{
	D3D12_DESCRIPTOR_RANGE descriptorRange[] = 
	{
		{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }
	};

	D3D12_ROOT_PARAMETER rootParameter[2];
	rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter[0].Descriptor = { 0, 0 };
	rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter[1].DescriptorTable = { 1, descriptorRange };
	rootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_STATIC_SAMPLER_DESC samplers[] =
	{
		{
			/*Filter*/				D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR
			/*AddressU*/			,D3D12_TEXTURE_ADDRESS_MODE_WRAP
		/*AddressV*/			,D3D12_TEXTURE_ADDRESS_MODE_WRAP
		/*AddressW*/			,D3D12_TEXTURE_ADDRESS_MODE_WRAP
		/*MipLODBias*/			,0
		/*MaxAnisotropy*/		,0
		/*ComparisonFunc*/		,D3D12_COMPARISON_FUNC_NEVER
		/*BorderColor*/			,D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK
		/*MinLOD*/				,0.f
		/*MaxLOD*/				,D3D12_FLOAT32_MAX
		/*ShaderRegister*/		,0
		/*RegisterSpace*/		,0
		/*ShaderVisibility*/	,D3D12_SHADER_VISIBILITY_PIXEL
		}
	};

	D3D12_ROOT_SIGNATURE_DESC descRootSignature;
	descRootSignature.NumParameters = 2;
	descRootSignature.pParameters = rootParameter;
	descRootSignature.NumStaticSamplers = 1;
	descRootSignature.pStaticSamplers = samplers;
	descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT | D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS | D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;

	ID3DBlob* signature;
	CheckFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));
	CheckFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_mainRS)));
	signature->Release();
}

void CRender::InitDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC descDescHeap = {};
	descDescHeap.NumDescriptors = 3;
	descDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descDescHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CheckFailed(m_device->CreateDescriptorHeap(&descDescHeap, IID_PPV_ARGS(&m_materialsDH)));
	m_materialsDH->SetName(L"MaterialsDescriptorHeaps");

	descDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descDescHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CheckFailed(m_device->CreateDescriptorHeap(&descDescHeap, IID_PPV_ARGS(&m_computeDH)));
	m_materialsDH->SetName(L"ComputeDescriptorHeaps");
}

void CRender::InitMainPSO()
{
	D3D12_INPUT_ELEMENT_DESC const vertDesc[4] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_RASTERIZER_DESC rasterizerState =
	{
		/*FillMode*/					D3D12_FILL_MODE_SOLID
		/*CullMode*/					,D3D12_CULL_MODE_BACK
		/*FrontCounterClockwise*/		,FALSE
		/*DepthBias*/					,D3D12_DEFAULT_DEPTH_BIAS
		/*DepthBiasClamp*/				,D3D12_DEFAULT_DEPTH_BIAS_CLAMP
		/*SlopeScaledDepthBias*/		,D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS
		/*DepthClipEnable*/				,TRUE
		/*MultisampleEnable*/			,FALSE
		/*AntialiasedLineEnable*/		,FALSE
		/*ForcedSampleCount*/			,0
		/*ConservativeRaster*/			,D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	};
	static D3D12_BLEND_DESC const blendState =
	{
		/*AlphaToCoverageEnable*/					FALSE
		/*IndependentBlendEnable*/					,FALSE
		/*RenderTarget[0].BlendEnable*/				,FALSE
		/*RenderTarget[0].LogicOpEnable*/			,FALSE
		/*RenderTarget[0].SrcBlend*/				,D3D12_BLEND_ONE
		/*RenderTarget[0].DestBlend*/				,D3D12_BLEND_ZERO
		/*RenderTarget[0].BlendOp*/					,D3D12_BLEND_OP_ADD
		/*RenderTarget[0].SrcBlendAlpha*/			,D3D12_BLEND_ONE
		/*RenderTarget[0].DestBlendAlpha*/			,D3D12_BLEND_ZERO
		/*RenderTarget[0].BlendOpAlpha*/			,D3D12_BLEND_OP_ADD
		/*RenderTarget[0].LogicOp*/					,D3D12_LOGIC_OP_NOOP
		/*RenderTarget[0].RenderTargetWriteMask*/	,D3D12_COLOR_WRITE_ENABLE_ALL
		/*RenderTarget[1].BlendEnable*/				,FALSE
		/*RenderTarget[1].LogicOpEnable*/			,FALSE
		/*RenderTarget[1].SrcBlend*/				,D3D12_BLEND_ONE
		/*RenderTarget[1].DestBlend*/				,D3D12_BLEND_ZERO
		/*RenderTarget[1].BlendOp*/					,D3D12_BLEND_OP_ADD
		/*RenderTarget[1].SrcBlendAlpha*/			,D3D12_BLEND_ONE
		/*RenderTarget[1].DestBlendAlpha*/			,D3D12_BLEND_ZERO
		/*RenderTarget[1].BlendOpAlpha*/			,D3D12_BLEND_OP_ADD
		/*RenderTarget[1].LogicOp*/					,D3D12_LOGIC_OP_NOOP
		/*RenderTarget[1].RenderTargetWriteMask*/	,D3D12_COLOR_WRITE_ENABLE_ALL
	};
	static D3D12_DEPTH_STENCIL_DESC const depthStencilState =
	{
		/*DepthEnable*/							TRUE
		/*DepthWriteMask*/						,D3D12_DEPTH_WRITE_MASK_ALL
		/*DepthFunc*/							,D3D12_COMPARISON_FUNC_LESS
		/*StencilEnable*/						,FALSE
		/*StencilReadMask*/						,D3D12_DEFAULT_STENCIL_READ_MASK
		/*StencilWriteMask*/					,D3D12_DEFAULT_STENCIL_WRITE_MASK
		/*FrontFace.StencilFailOp*/				,{ D3D12_STENCIL_OP_REPLACE
		/*FrontFace.StencilDepthFailOp*/		,D3D12_STENCIL_OP_REPLACE
		/*FrontFace.StencilPassOp*/				,D3D12_STENCIL_OP_REPLACE
		/*FrontFace.StencilFunc*/				,D3D12_COMPARISON_FUNC_ALWAYS }
		/*BackFace.StencilFailOp*/				,{ D3D12_STENCIL_OP_REPLACE
		/*BackFace.StencilDepthFailOp*/			,D3D12_STENCIL_OP_REPLACE
		/*BackFace.StencilPassOp*/				,D3D12_STENCIL_OP_REPLACE
		/*BackFace.StencilFunc*/				,D3D12_COMPARISON_FUNC_ALWAYS }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPSO = {};
	descPSO.BlendState = blendState;
	descPSO.DepthStencilState = depthStencilState;
	descPSO.InputLayout = { vertDesc, 4 };
	descPSO.RasterizerState = rasterizerState;
	descPSO.SampleDesc.Count = 1;
	descPSO.SampleMask = UINT_MAX;
	descPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPSO.NumRenderTargets = 1;
	descPSO.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;
	descPSO.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	ID3DBlob* vsShader;
	LoadShader(L"../shaders/meshDraw.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vsMain", "vs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL2, &vsShader);
	descPSO.VS = { vsShader->GetBufferPointer(), vsShader->GetBufferSize() };

	ID3DBlob* psShader;
	LoadShader(L"../shaders/meshDraw.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "psMain", "ps_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL2, &psShader);
	descPSO.PS = { psShader->GetBufferPointer(), psShader->GetBufferSize() };

	descPSO.pRootSignature = m_mainRS;

	CheckFailed(m_device->CreateGraphicsPipelineState(&descPSO, IID_PPV_ARGS(&m_mainPSO)));

	rasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	descPSO.RasterizerState = rasterizerState;
	CheckFailed(m_device->CreateGraphicsPipelineState(&descPSO, IID_PPV_ARGS(&m_mainWireframePSO)));

	vsShader->Release();
	psShader->Release();
}

void CRender::InitSkybox()
{
	D3D12_INPUT_ELEMENT_DESC const vertDesc[4] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	static D3D12_RASTERIZER_DESC const rasterizerState =
	{
		/*FillMode*/					D3D12_FILL_MODE_SOLID
		/*CullMode*/					,D3D12_CULL_MODE_NONE
		/*FrontCounterClockwise*/		,FALSE
		/*DepthBias*/					,D3D12_DEFAULT_DEPTH_BIAS
		/*DepthBiasClamp*/				,D3D12_DEFAULT_DEPTH_BIAS_CLAMP
		/*SlopeScaledDepthBias*/		,D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS
		/*DepthClipEnable*/				,FALSE
		/*MultisampleEnable*/			,FALSE
		/*AntialiasedLineEnable*/		,FALSE
		/*ForcedSampleCount*/			,0
		/*ConservativeRaster*/			,D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
	};
	static D3D12_BLEND_DESC const blendState =
	{
		/*AlphaToCoverageEnable*/					FALSE
		/*IndependentBlendEnable*/					,FALSE
		/*RenderTarget[0].BlendEnable*/				,FALSE
		/*RenderTarget[0].LogicOpEnable*/			,FALSE
		/*RenderTarget[0].SrcBlend*/				,D3D12_BLEND_ONE
		/*RenderTarget[0].DestBlend*/				,D3D12_BLEND_ZERO
		/*RenderTarget[0].BlendOp*/					,D3D12_BLEND_OP_ADD
		/*RenderTarget[0].SrcBlendAlpha*/			,D3D12_BLEND_ONE
		/*RenderTarget[0].DestBlendAlpha*/			,D3D12_BLEND_ZERO
		/*RenderTarget[0].BlendOpAlpha*/			,D3D12_BLEND_OP_ADD
		/*RenderTarget[0].LogicOp*/					,D3D12_LOGIC_OP_NOOP
		/*RenderTarget[0].RenderTargetWriteMask*/	,D3D12_COLOR_WRITE_ENABLE_ALL
		/*RenderTarget[1].BlendEnable*/				,FALSE
		/*RenderTarget[1].LogicOpEnable*/			,FALSE
		/*RenderTarget[1].SrcBlend*/				,D3D12_BLEND_ONE
		/*RenderTarget[1].DestBlend*/				,D3D12_BLEND_ZERO
		/*RenderTarget[1].BlendOp*/					,D3D12_BLEND_OP_ADD
		/*RenderTarget[1].SrcBlendAlpha*/			,D3D12_BLEND_ONE
		/*RenderTarget[1].DestBlendAlpha*/			,D3D12_BLEND_ZERO
		/*RenderTarget[1].BlendOpAlpha*/			,D3D12_BLEND_OP_ADD
		/*RenderTarget[1].LogicOp*/					,D3D12_LOGIC_OP_NOOP
		/*RenderTarget[1].RenderTargetWriteMask*/	,D3D12_COLOR_WRITE_ENABLE_ALL
	};
	static D3D12_DEPTH_STENCIL_DESC const depthStencilState =
	{
		/*DepthEnable*/							FALSE
		/*DepthWriteMask*/						,D3D12_DEPTH_WRITE_MASK_ALL
		/*DepthFunc*/							,D3D12_COMPARISON_FUNC_LESS
		/*StencilEnable*/						,FALSE
		/*StencilReadMask*/						,D3D12_DEFAULT_STENCIL_READ_MASK
		/*StencilWriteMask*/					,D3D12_DEFAULT_STENCIL_WRITE_MASK
		/*FrontFace.StencilFailOp*/				,{ D3D12_STENCIL_OP_REPLACE
		/*FrontFace.StencilDepthFailOp*/		,D3D12_STENCIL_OP_REPLACE
		/*FrontFace.StencilPassOp*/				,D3D12_STENCIL_OP_REPLACE
		/*FrontFace.StencilFunc*/				,D3D12_COMPARISON_FUNC_ALWAYS }
		/*BackFace.StencilFailOp*/				,{ D3D12_STENCIL_OP_REPLACE
		/*BackFace.StencilDepthFailOp*/			,D3D12_STENCIL_OP_REPLACE
		/*BackFace.StencilPassOp*/				,D3D12_STENCIL_OP_REPLACE
		/*BackFace.StencilFunc*/				,D3D12_COMPARISON_FUNC_ALWAYS }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC descPSO = {};
	descPSO.BlendState = blendState;
	descPSO.DepthStencilState = depthStencilState;
	descPSO.InputLayout = { vertDesc, 4 };
	descPSO.RasterizerState = rasterizerState;
	descPSO.SampleDesc.Count = 1;
	descPSO.SampleMask = UINT_MAX;
	descPSO.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	descPSO.NumRenderTargets = 1;
	descPSO.RTVFormats[0] = DXGI_FORMAT_B8G8R8A8_UNORM;

	D3D_SHADER_MACRO const vsMacros[] = { "VERTEX_S", NULL, NULL };
	ID3DBlob* vsShader;
	LoadShader(L"../shaders/skybox.hlsl", vsMacros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "vsMain", "vs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL2, &vsShader);
	descPSO.VS = { vsShader->GetBufferPointer(), vsShader->GetBufferSize() };

	D3D_SHADER_MACRO const psMacros[] = { "PIXEL_S", NULL, NULL };
	ID3DBlob* psShader;
	LoadShader(L"../shaders/skybox.hlsl", psMacros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "psMain", "ps_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL2, &psShader);
	descPSO.PS = { psShader->GetBufferPointer(), psShader->GetBufferSize() };

	descPSO.pRootSignature = m_mainRS;

	CheckFailed(m_device->CreateGraphicsPipelineState(&descPSO, IID_PPV_ARGS(&m_skybox.m_skyboxPSO)));

	vsShader->Release();
	psShader->Release();

	SVertexData* vertices = nullptr;
	UINT32* indices = nullptr;
	UINT verticesNum = 0;
	m_skybox.m_indicesNum = 0;
	
	GMeshLoader.LoadMesh("../content/skydome.fbx", verticesNum, &vertices, m_skybox.m_indicesNum, &indices);

	D3D12_RESOURCE_DESC descResource = {};
	descResource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	descResource.Width = verticesNum * sizeof(SVertexData);
	descResource.Height = 1;
	descResource.DepthOrArraySize = 1;
	descResource.MipLevels = 1;
	descResource.Format = DXGI_FORMAT_UNKNOWN;
	descResource.SampleDesc.Count = 1;
	descResource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	descResource.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* verticesUploadRes;
	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &descResource, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&verticesUploadRes)));
	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &descResource, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_skybox.m_meshVertices)));

	D3D12_SUBRESOURCE_DATA subData = {};
	subData.pData = vertices;
	subData.RowPitch = descResource.Width;
	subData.SlicePitch = descResource.Width;

	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
	UINT numRows;
	UINT64 rowSizeInBytes, totalBytes;
	m_device->GetCopyableFootprints(&descResource, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

	void* pGPU;
	verticesUploadRes->Map(0, nullptr, &pGPU);
	D3D12_MEMCPY_DEST destData = { (BYTE*)pGPU + footprint.Offset, footprint.Footprint.RowPitch, footprint.Footprint.RowPitch * numRows };
	for ( UINT z = 0; z < footprint.Footprint.Depth; ++z)
	{
		SIZE_T const slice = destData.SlicePitch * z;
		BYTE* pDstSlice = (BYTE*)destData.pData + slice;
		BYTE const* pSrcSlice = (BYTE const*)subData.pData + slice;
		for (UINT y = 0; y < numRows; ++y)
		{
			memcpy(pDstSlice + destData.RowPitch * y, pSrcSlice + subData.RowPitch * y, rowSizeInBytes);
		}
	}
	verticesUploadRes->Unmap(0, nullptr);

	descResource.Width = m_skybox.m_indicesNum * sizeof(UINT32);

	ID3D12Resource* indicesUploadRes;
	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &descResource, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indicesUploadRes)));
	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &descResource, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_skybox.m_meshInidces)));

	subData.pData = indices;
	subData.RowPitch = descResource.Width;
	subData.SlicePitch = descResource.Width;

	m_device->GetCopyableFootprints(&descResource, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &totalBytes);

	indicesUploadRes->Map(0, nullptr, &pGPU);
	destData = { (BYTE*)pGPU + footprint.Offset, footprint.Footprint.RowPitch, footprint.Footprint.RowPitch * numRows };
	for (UINT z = 0; z < footprint.Footprint.Depth; ++z)
	{
		SIZE_T const slice = destData.SlicePitch * z;
		BYTE* pDstSlice = (BYTE*)destData.pData + slice;
		BYTE const* pSrcSlice = (BYTE const*)subData.pData + slice;
		for (UINT y = 0; y < numRows; ++y)
		{
			memcpy(pDstSlice + destData.RowPitch * y, pSrcSlice + subData.RowPitch * y, rowSizeInBytes);
		}
	}
	indicesUploadRes->Unmap(0, nullptr);

	STexutre skyboxTexture;
	void* cpuImg = GTextureLoader.LoadTexture("../content/skybox.dds", skyboxTexture);

	m_copyCA->Reset();
	m_copyCL->Reset(m_copyCA, nullptr);
	m_copyCL->CopyResource(m_skybox.m_meshVertices, verticesUploadRes);
	m_copyCL->CopyResource(m_skybox.m_meshInidces, indicesUploadRes);

	ID3D12Resource* textureUploadRes = CopyTexture( skyboxTexture, &m_skybox.m_texture );

	m_copyCL->Close();
	ID3D12CommandList* ppCopyCL[] = { m_copyCL };
	m_copyCQ->ExecuteCommandLists(1, ppCopyCL);

	D3D12_RESOURCE_BARRIER barrires[3];
	barrires[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrires[0].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrires[0].Transition.pResource = m_skybox.m_meshVertices;
	barrires[0].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrires[0].Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	barrires[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barrires[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrires[1].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrires[1].Transition.pResource = m_skybox.m_meshInidces;
	barrires[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrires[1].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
	barrires[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barrires[2].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrires[2].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrires[2].Transition.pResource = m_skybox.m_texture;
	barrires[2].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrires[2].Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrires[2].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	m_mainCA->Reset();
	m_mainCL->Reset(m_mainCA, nullptr);
	m_mainCL->ResourceBarrier(3, barrires);
	m_mainCL->Close();

	delete indices;
	delete vertices;
	delete cpuImg;

	CheckFailed(m_copyCQ->Signal(m_fence, m_fenceValue));
	CheckFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	++m_fenceValue;
	WaitForSingleObject(m_fenceEvent, INFINITE);

	ID3D12CommandList* ppMainCL[] = { m_mainCL };
	m_mainCQ->ExecuteCommandLists(1, ppMainCL);

	verticesUploadRes->Release();
	indicesUploadRes->Release();
	textureUploadRes->Release();

	CheckFailed(m_mainCQ->Signal(m_fence, m_fenceValue));
	CheckFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	++m_fenceValue;
	WaitForSingleObject(m_fenceEvent, INFINITE);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = skyboxTexture.m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = skyboxTexture.m_mipNum;
	srvDesc.Texture2D.MostDetailedMip = 0;

	m_device->CreateShaderResourceView(m_skybox.m_texture, &srvDesc, m_materialsDH->GetCPUDescriptorHandleForHeapStart());

	m_skybox.m_indicesBufferView.BufferLocation = m_skybox.m_meshInidces->GetGPUVirtualAddress();
	m_skybox.m_indicesBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_skybox.m_indicesBufferView.SizeInBytes = m_skybox.m_indicesNum * sizeof(UINT32);

	m_skybox.m_verticesBufferView.BufferLocation = m_skybox.m_meshVertices->GetGPUVirtualAddress();
	m_skybox.m_verticesBufferView.StrideInBytes = sizeof(SVertexData);
	m_skybox.m_verticesBufferView.SizeInBytes = verticesNum * sizeof(SVertexData);
}

void CRender::InitTerrain()
{
	ID3D12RootSignature* terrainIndicesRootSignature;
	D3D12_ROOT_PARAMETER parametersRS[3];
	parametersRS[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	parametersRS[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	parametersRS[0].Descriptor = { 0, 0 };

	parametersRS[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
	parametersRS[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	parametersRS[1].Descriptor = { 0, 0 };

	D3D12_DESCRIPTOR_RANGE descRange = {};
	descRange.BaseShaderRegister = 0;
	descRange.RegisterSpace = 0;
	descRange.NumDescriptors = 1;
	descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	parametersRS[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	parametersRS[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
	parametersRS[2].DescriptorTable.NumDescriptorRanges = 1;
	parametersRS[2].DescriptorTable.pDescriptorRanges = &descRange;

	D3D12_ROOT_SIGNATURE_DESC descRS = {};
	descRS.NumParameters = 2;
	descRS.pParameters = parametersRS;

	ID3DBlob* signature;
	CheckFailed(D3D12SerializeRootSignature(&descRS, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));
	CheckFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&terrainIndicesRootSignature)));
	signature->Release();

	ID3D12PipelineState* idPSO;
	D3D12_COMPUTE_PIPELINE_STATE_DESC descIdPSO = {};
	descIdPSO.pRootSignature = terrainIndicesRootSignature;

	D3D_SHADER_MACRO const idMacros[] = { "TERRAIN_INDICES", NULL, NULL };
	ID3DBlob* idShader;
	LoadShader(L"../shaders/terrainGeneration.hlsl", idMacros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "TerrainIndices", "cs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL2, &idShader);
	descIdPSO.CS = { idShader->GetBufferPointer(), idShader->GetBufferSize() };

	CheckFailed(m_device->CreateComputePipelineState(&descIdPSO, IID_PPV_ARGS(&idPSO)));
	idShader->Release();

	D3D12_RESOURCE_DESC indicesDesc = {};
	indicesDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	indicesDesc.Height = 1;
	indicesDesc.DepthOrArraySize = 1;
	indicesDesc.MipLevels = 1;
	indicesDesc.Format = DXGI_FORMAT_UNKNOWN;
	indicesDesc.SampleDesc.Count = 1;
	indicesDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	indicesDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	for (UINT lodID = 0; lodID < ELods::MAX; ++lodID)
	{
		indicesDesc.Width = 6 * GTerrainLodInfo[lodID][0] * sizeof(UINT32);

		CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesGPUOnly, D3D12_HEAP_FLAG_NONE, &indicesDesc, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_terrain.m_indices[lodID])));

		D3D12_INDEX_BUFFER_VIEW& bufferView = m_terrain.m_indicesView[lodID];
		bufferView.BufferLocation = m_terrain.m_indices[lodID]->GetGPUVirtualAddress();
		bufferView.Format = DXGI_FORMAT_R32_UINT;
		bufferView.SizeInBytes = indicesDesc.Width;

		m_terrain.m_indicesNum[lodID] = 6 * GTerrainLodInfo[lodID][0];
	}

	ID3D12Resource* indicesCB;
	D3D12_RESOURCE_DESC cbDesc = {};
	cbDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	cbDesc.Height = 1;
	cbDesc.Width = ELods::MAX * sizeof(TileGenCB);
	cbDesc.DepthOrArraySize = 1;
	cbDesc.MipLevels = 1;
	cbDesc.Format = DXGI_FORMAT_UNKNOWN;
	cbDesc.SampleDesc.Count = 1;
	cbDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	cbDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&indicesCB)));
	TileGenCB* cbData = nullptr;
	indicesCB->Map(0, nullptr, (void**)(&cbData));

	for (UINT lodID = 0; lodID < ELods::MAX; ++lodID)
	{
		cbData[lodID].m_verticesOnEdge = GTerrainLodInfo[lodID][1] + 1;
		cbData[lodID].m_quadsOnEdge = GTerrainLodInfo[lodID][1];
	}

	indicesCB->Unmap(0, nullptr);

	m_computeCA->Reset();
	m_computeCL->Reset(m_computeCA, idPSO);

	m_computeCL->SetComputeRootSignature(terrainIndicesRootSignature);

	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = indicesCB->GetGPUVirtualAddress();
	for (UINT lodID = 0; lodID < ELods::MAX; ++lodID)
	{
		UINT const indGroupNum = (UINT)ceil((float)GTerrainLodInfo[lodID][1] / 32.f);

		m_computeCL->SetComputeRootConstantBufferView(0, cbAddress);
		m_computeCL->SetComputeRootUnorderedAccessView(1, m_terrain.m_indices[lodID]->GetGPUVirtualAddress());
		m_computeCL->Dispatch(indGroupNum, indGroupNum, 1);
		cbAddress += sizeof(TileGenCB);
	}

	m_computeCL->Close();

	ID3D12CommandList* ppComputeCL[] = { m_computeCL };
	m_computeCQ->ExecuteCommandLists(1, ppComputeCL);

	STexutre heightmap;
	void* cpuImgHM = GTextureLoader.LoadTexture("../content/terrain_hm.dds", heightmap);
	STexutre diffuse;
	void* cpuImgDM = GTextureLoader.LoadTexture("../content/terrain_df.dds", diffuse);
	STexutre normal;
	void* cpuImgNM = GTextureLoader.LoadTexture("../content/terrain_nm.dds", normal);

	m_copyCA->Reset();
	m_copyCL->Reset(m_copyCA, nullptr);

	ID3D12Resource* textureUploadResHM = CopyTexture(heightmap, &m_terrain.m_heightmap);
	ID3D12Resource* textureUploadResDM = CopyTexture(diffuse, &m_terrain.m_diffuseTex);
	ID3D12Resource* textureUploadResNM = CopyTexture(normal, &m_terrain.m_normalTex);

	m_copyCL->Close();

	ID3D12CommandList* ppCopyCL[] = { m_copyCL };
	m_copyCQ->ExecuteCommandLists(1, ppCopyCL);

	D3D12_RESOURCE_BARRIER barriers[ELods::MAX + 1];
	for (UINT lodID = 0; lodID < ELods::MAX; ++lodID)
	{
		barriers[lodID].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barriers[lodID].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		barriers[lodID].Transition.pResource = m_terrain.m_indices[lodID];
		barriers[lodID].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
		barriers[lodID].Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
		barriers[lodID].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	}

	barriers[ELods::MAX].Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barriers[ELods::MAX].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barriers[ELods::MAX].Transition.pResource = m_terrain.m_heightmap;
	barriers[ELods::MAX].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	barriers[ELods::MAX].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barriers[ELods::MAX].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	m_mainCA->Reset();
	m_mainCL->Reset(m_mainCA, nullptr);

	m_mainCL->ResourceBarrier(ELods::MAX + 1, barriers);

	m_mainCL->Close();

	ID3D12CommandList* ppMainCL[] = { m_mainCL };

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = heightmap.m_format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = heightmap.m_mipNum;
	srvDesc.Texture2D.MostDetailedMip = 0;

	m_device->CreateShaderResourceView(m_terrain.m_heightmap, &srvDesc, m_computeDH->GetCPUDescriptorHandleForHeapStart());

	D3D12_CPU_DESCRIPTOR_HANDLE materialDH = m_materialsDH->GetCPUDescriptorHandleForHeapStart();

	materialDH.ptr += m_srvDescriptorHandleIncrementSize;
	srvDesc.Format = diffuse.m_format;
	srvDesc.Texture2D.MipLevels = diffuse.m_mipNum;
	m_device->CreateShaderResourceView(m_terrain.m_diffuseTex, &srvDesc, materialDH);

	materialDH.ptr += m_srvDescriptorHandleIncrementSize;
	srvDesc.Format = normal.m_format;
	srvDesc.Texture2D.MipLevels = normal.m_mipNum;
	m_device->CreateShaderResourceView(m_terrain.m_normalTex, &srvDesc, materialDH);

	descRS.NumParameters = 3;

	CheckFailed(D3D12SerializeRootSignature(&descRS, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));
	CheckFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_terrain.m_terrainRS)));
	signature->Release();

	ID3D12PipelineState* verPosPSO;
	D3D12_COMPUTE_PIPELINE_STATE_DESC descVerPSO = {};
	descVerPSO.pRootSignature = m_terrain.m_terrainRS;

	D3D_SHADER_MACRO const verPosMacros[] = { "TERRAIN_VERTICES", NULL, "TERRAIN_VERTICES_POS", NULL, NULL };
	D3D_SHADER_MACRO const verLigMacrosP0[] = { "TERRAIN_VERTICES", NULL, "TERRAIN_VERTICES_NOR_TAN", NULL, "TERRAIN_VERTICES_NOR_TAN_PASS0", NULL, NULL };
	D3D_SHADER_MACRO const verLigMacrosP1[] = { "TERRAIN_VERTICES", NULL, "TERRAIN_VERTICES_NOR_TAN", NULL, "TERRAIN_VERTICES_NOR_TAN_PASS1", NULL, NULL };
	ID3DBlob* verShader;
	LoadShader(L"../shaders/terrainGeneration.hlsl", verPosMacros, D3D_COMPILE_STANDARD_FILE_INCLUDE, "TerrainVertices", "cs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL2, &verShader);
	descVerPSO.CS = { verShader->GetBufferPointer(), verShader->GetBufferSize() };

	CheckFailed(m_device->CreateComputePipelineState(&descVerPSO, IID_PPV_ARGS(&m_terrain.m_terrainPosPSO)));
	verShader->Release();

	LoadShader(L"../shaders/terrainGeneration.hlsl", verLigMacrosP0, D3D_COMPILE_STANDARD_FILE_INCLUDE, "TerrainVertices", "cs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL2, &verShader);
	descVerPSO.CS = { verShader->GetBufferPointer(), verShader->GetBufferSize() };

	CheckFailed(m_device->CreateComputePipelineState(&descVerPSO, IID_PPV_ARGS(&m_terrain.m_terrainLightPSOPass0)));
	verShader->Release();

	LoadShader(L"../shaders/terrainGeneration.hlsl", verLigMacrosP1, D3D_COMPILE_STANDARD_FILE_INCLUDE, "TerrainVertices", "cs_5_1", D3DCOMPILE_OPTIMIZATION_LEVEL2, &verShader);
	descVerPSO.CS = { verShader->GetBufferPointer(), verShader->GetBufferSize() };

	CheckFailed(m_device->CreateComputePipelineState(&descVerPSO, IID_PPV_ARGS(&m_terrain.m_terrainLightPSOPass1)));
	verShader->Release();

	CheckFailed(m_copyCQ->Signal(m_fence, m_fenceValue));
	CheckFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	++m_fenceValue;
	WaitForSingleObject(m_fenceEvent, INFINITE);

	textureUploadResHM->Release();
	textureUploadResDM->Release();
	textureUploadResNM->Release();
	delete cpuImgHM;
	delete cpuImgDM;
	delete cpuImgNM;

	CheckFailed(m_computeCQ->Signal(m_fence, m_fenceValue));
	CheckFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	++m_fenceValue;
	WaitForSingleObject(m_fenceEvent, INFINITE);

	m_mainCQ->ExecuteCommandLists(1, ppMainCL);

	indicesCB->Release();
	idPSO->Release();
	terrainIndicesRootSignature->Release();

	CheckFailed(m_mainCQ->Signal(m_fence, m_fenceValue));
	CheckFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	++m_fenceValue;
	WaitForSingleObject(m_fenceEvent, INFINITE);

	m_terrain.m_tilesData.resize(GTerrainEdgeTiles * GTerrainEdgeTiles);

	D3D12_RESOURCE_DESC descTileData = {};
	descTileData.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	descTileData.Height = 1;
	descTileData.Width = GTerrainEdgeTiles * GTerrainEdgeTiles * sizeof(TileGenCB);
	descTileData.DepthOrArraySize = 1;
	descTileData.MipLevels = 1;
	descTileData.Format = DXGI_FORMAT_UNKNOWN;
	descTileData.SampleDesc.Count = 1;
	descTileData.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	descTileData.Flags = D3D12_RESOURCE_FLAG_NONE;
	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &descTileData, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_terrain.m_gpuTileData)));
	m_terrain.m_gpuTileData->Map(0, nullptr, (void**)(&m_terrain.m_pGpuTileData));

	float const invTilesEdgeNum = 1.f / (float)GTerrainEdgeTiles;

	for (UINT x = 0; x < GTerrainEdgeTiles; ++x)
	{
		for (UINT y = 0; y < GTerrainEdgeTiles; ++y)
		{
			UINT const flatID = x * GTerrainEdgeTiles + y;
			TileGenCB& tileCB = m_terrain.m_pGpuTileData[flatID];

			tileCB.m_heightmapRect.Set( invTilesEdgeNum, (float)x * invTilesEdgeNum, (float)y * invTilesEdgeNum);
			tileCB.m_worldTileSize = GTerrainSize.x * invTilesEdgeNum;
			tileCB.m_terrainHeight = GTerrainSize.y;
			tileCB.m_tilePosition.Set(x * tileCB.m_worldTileSize, y * tileCB.m_worldTileSize);
			tileCB.m_heightmapRes.Set(heightmap.m_x[0], heightmap.m_y[0]);
			tileCB.m_edgesData = 0;

			if (0 < x) tileCB.m_edgesData |= LEFT_TILE;
			if (0 < y) tileCB.m_edgesData |= BOTTOM_TILE;
			if (x < GTerrainEdgeTiles - 1) tileCB.m_edgesData |= RIGHT_TILE;
			if (y < GTerrainEdgeTiles - 1) tileCB.m_edgesData |= TOP_TILE;

			m_terrain.m_tilesData[flatID].m_centerPosition.Set(x * tileCB.m_worldTileSize + tileCB.m_worldTileSize * 0.5f, 0.f, y * tileCB.m_worldTileSize + tileCB.m_worldTileSize * 0.5f);
		}
	}
	m_terrain.m_vertices = nullptr;

	m_terrain.m_objectToWorld = Matrix4x4::Identity;

	UpdateTerrain();
}

void CRender::UpdateTerrain()
{
	if (m_terrain.m_vertices)
	{
		m_terrain.m_vertices->Release();
		m_terrain.m_vertices = nullptr;
	}

	Vec3 const cameraPos = GCamera.GetPosition();
	UINT verticesNum = 0;
	for (UINT x = 0; x < GTerrainEdgeTiles; ++x)
	{
		for (UINT y = 0; y < GTerrainEdgeTiles; ++y)
		{
			UINT const flatID = x * GTerrainEdgeTiles + y;
			STileData& tileData = m_terrain.m_tilesData[flatID];

			tileData.m_verticesOffset = verticesNum;

			tileData.m_lod = LOD2;
			float const sqDistance = (cameraPos - tileData.m_centerPosition).LengthSq();
			if (sqDistance < GTerrainLoad1Sq)
			{
				tileData.m_lod = LOD1;
				if (sqDistance < GTerrainLoad0Sq)
				{
					tileData.m_lod = LOD0;
				}
			}

			verticesNum += (GTerrainLodInfo[tileData.m_lod][1] + 1) * (GTerrainLodInfo[tileData.m_lod][1] + 1);
		}
	}

	D3D12_RESOURCE_DESC descVertics = {};
	descVertics.DepthOrArraySize = 1;
	descVertics.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	descVertics.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	descVertics.Format = DXGI_FORMAT_UNKNOWN;
	descVertics.Height = 1;
	descVertics.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	descVertics.MipLevels = 1;
	descVertics.SampleDesc.Count = 1;
	descVertics.Width = verticesNum * sizeof(SVertexData);

	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesGPUOnly, D3D12_HEAP_FLAG_NONE, &descVertics, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr, IID_PPV_ARGS(&m_terrain.m_vertices)));
	m_terrain.m_vertices->SetName(L"TerrainVertices");
	m_terrain.m_verticesView.BufferLocation = m_terrain.m_vertices->GetGPUVirtualAddress();
	m_terrain.m_verticesView.SizeInBytes = descVertics.Width;
	m_terrain.m_verticesView.StrideInBytes = sizeof(SVertexData);

	m_computeCA->Reset();
	m_computeCL->Reset(m_computeCA, m_terrain.m_terrainPosPSO);
	m_computeCL->SetDescriptorHeaps(1, &m_computeDH);
	m_computeCL->SetComputeRootSignature(m_terrain.m_terrainRS);
	m_computeCL->SetComputeRootUnorderedAccessView(1, m_terrain.m_vertices->GetGPUVirtualAddress());
	m_computeCL->SetComputeRootDescriptorTable(2, m_computeDH->GetGPUDescriptorHandleForHeapStart());

	D3D12_GPU_VIRTUAL_ADDRESS vTileCB = m_terrain.m_gpuTileData->GetGPUVirtualAddress();
	for (UINT x = 0; x < GTerrainEdgeTiles; ++x)
	{
		for (UINT y = 0; y < GTerrainEdgeTiles; ++y)
		{
			UINT const flatID = x * GTerrainEdgeTiles + y;
			ELods const tileLod = m_terrain.m_tilesData[flatID].m_lod;
			TileGenCB& tileCB = m_terrain.m_pGpuTileData[flatID];
			tileCB.m_edgesData &= ~EDGE_LOD_MASK;
			tileCB.m_verticesOnEdge = GTerrainLodInfo[tileLod][1] + 1;
			tileCB.m_quadsOnEdge = GTerrainLodInfo[tileLod][1];
			tileCB.m_uvQuadSize = 1.f / (float)GTerrainLodInfo[tileLod][1];
			tileCB.m_dataOffset = m_terrain.m_tilesData[flatID].m_verticesOffset;

			if (0 < x)
			{
				STileData const& otherTileData = m_terrain.m_tilesData[(x - 1) * GTerrainEdgeTiles + y];
				tileCB.m_neighborsTilesLeft = otherTileData.m_verticesOffset;
				if (tileLod < otherTileData.m_lod) tileCB.m_edgesData |= LEFT_LOD;
				if (otherTileData.m_lod < tileLod) tileCB.m_edgesData |= LEFT_LOW_LOD;
			}
			if (0 < y)
			{
				STileData const& otherTileData = m_terrain.m_tilesData[x * GTerrainEdgeTiles + y - 1];
				tileCB.m_neighborsTilesBottom = otherTileData.m_verticesOffset;
				if (tileLod < otherTileData.m_lod) tileCB.m_edgesData |= BOTTOM_LOD;
				if (otherTileData.m_lod < tileLod) tileCB.m_edgesData |= BOTTOM_LOW_LOD;

				if (0 < x)
				{
					STileData const& crossTileData = m_terrain.m_tilesData[( x - 1) * GTerrainEdgeTiles + y - 1];
					tileCB.m_neighborsTilesBottomLeft = crossTileData.m_verticesOffset;
					if (tileLod < crossTileData.m_lod) tileCB.m_edgesData |= BOTTOM_LEFT_LOD;
					if (crossTileData.m_lod < tileLod) tileCB.m_edgesData |= BOTTOM_LEFT_LOW_LOD;
				}
				if (x < GTerrainEdgeTiles - 1)
				{
					STileData const& crossTileData = m_terrain.m_tilesData[(x + 1) * GTerrainEdgeTiles + y - 1];
					tileCB.m_neighborsTilesBottomRight = crossTileData.m_verticesOffset;
					if (tileLod < crossTileData.m_lod) tileCB.m_edgesData |= BOTTOM_RIGHT_LOD;
					if (crossTileData.m_lod < tileLod) tileCB.m_edgesData |= BOTTOM_RIGHT_LOW_LOD;
				}
			}
			if (x < GTerrainEdgeTiles - 1)
			{
				STileData const& otherTileData = m_terrain.m_tilesData[(x + 1) * GTerrainEdgeTiles + y];
				tileCB.m_neighborsTilesRight = otherTileData.m_verticesOffset;
				if (tileLod < otherTileData.m_lod) tileCB.m_edgesData |= RIGHT_LOD;
				if (otherTileData.m_lod < tileLod) tileCB.m_edgesData |= RIGHT_LOW_LOD;
			}
			if (y < GTerrainEdgeTiles - 1)
			{
				STileData const& otherTileData = m_terrain.m_tilesData[x * GTerrainEdgeTiles + y + 1];
				tileCB.m_neighborsTilesTop = otherTileData.m_verticesOffset;
				if (tileLod <  otherTileData.m_lod) tileCB.m_edgesData |= TOP_LOD;
				if (otherTileData.m_lod < tileLod) tileCB.m_edgesData |= TOP_LOW_LOD;

				if (0 < x)
				{
					STileData const& crossTileData = m_terrain.m_tilesData[(x - 1) * GTerrainEdgeTiles + y + 1];
					tileCB.m_neighborsTilesTopLeft = crossTileData.m_verticesOffset;
					if (tileLod < crossTileData.m_lod) tileCB.m_edgesData |= TOP_LEFT_LOD;
					if (crossTileData.m_lod < tileLod) tileCB.m_edgesData |= TOP_LEFT_LOW_LOD;
				}
				if (x < GTerrainEdgeTiles - 1)
				{
					STileData const& crossTileData = m_terrain.m_tilesData[(x + 1) * GTerrainEdgeTiles + y + 1];
					tileCB.m_neighborsTilesTopRight = crossTileData.m_verticesOffset;
					if (tileLod < crossTileData.m_lod) tileCB.m_edgesData |= TOP_RIGHT_LOD;
					if (crossTileData.m_lod < tileLod) tileCB.m_edgesData |= TOP_RIGHT_LOW_LOD;
				}
			}
			UINT const verGroupNum = (UINT)ceil((float)(GTerrainLodInfo[tileLod][1] + 1) / 22.f);
			m_computeCL->SetComputeRootConstantBufferView(0, vTileCB + flatID * sizeof(TileGenCB));
			m_computeCL->Dispatch(verGroupNum, verGroupNum, 1);
		}
	}

	D3D12_RESOURCE_BARRIER verticesBarrier = {};
	verticesBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	verticesBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	verticesBarrier.UAV.pResource = m_terrain.m_vertices;

	m_computeCL->ResourceBarrier(1, &verticesBarrier);
	m_computeCL->SetPipelineState(m_terrain.m_terrainLightPSOPass0);

	for (UINT tileID = 0; tileID < GTerrainTilesNum; ++tileID)
	{
		ELods const tileLod = m_terrain.m_tilesData[tileID].m_lod;
		UINT const verGroupNum = (UINT)ceil((float)(GTerrainLodInfo[tileLod][1] + 1) / 22.f);
		m_computeCL->SetComputeRootConstantBufferView(0, vTileCB + tileID * sizeof(TileGenCB));
		m_computeCL->Dispatch(verGroupNum, verGroupNum, 1);
	}

	m_computeCL->ResourceBarrier(1, &verticesBarrier);
	m_computeCL->SetPipelineState(m_terrain.m_terrainLightPSOPass1);

	for (UINT tileID = 0; tileID < GTerrainTilesNum; ++tileID)
	{
		ELods const tileLod = m_terrain.m_tilesData[tileID].m_lod;
		UINT const verGroupNum = (UINT)ceil((float)(GTerrainLodInfo[tileLod][1] + 1) / 22.f);
		m_computeCL->SetComputeRootConstantBufferView(0, vTileCB + tileID * sizeof(TileGenCB));
		m_computeCL->Dispatch(verGroupNum, verGroupNum, 1);
	}

	m_computeCL->Close();

	ID3D12CommandList* ppComputeCL[] = { m_computeCL };
	m_computeCQ->ExecuteCommandLists(1, ppComputeCL);

	verticesBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	verticesBarrier.Transition.pResource = m_terrain.m_vertices;
	verticesBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	verticesBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	verticesBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	m_mainCA->Reset();
	m_mainCL->Reset(m_mainCA, nullptr);

	m_mainCL->ResourceBarrier(1, &verticesBarrier);

	m_mainCL->Close();

	ID3D12CommandList* ppMainCL[] = { m_mainCL };

	CheckFailed(m_computeCQ->Signal(m_fence, m_fenceValue));
	CheckFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	++m_fenceValue;
	WaitForSingleObject(m_fenceEvent, INFINITE);

	m_mainCQ->ExecuteCommandLists(1, ppMainCL);

	CheckFailed(m_mainCQ->Signal(m_fence, m_fenceValue));
	CheckFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	++m_fenceValue;
	WaitForSingleObject(m_fenceEvent, INFINITE);
}

void CRender::DrawFrame()
{
	if (GUpdateTerrain)
	{
		UpdateTerrain();
	}

	Matrix4x4 worldToProjection = GCamera.GetWorldToCamera() * GCamera.GetProjectionMatrix();
	Matrix4x4 objectToProjection = worldToProjection;
	Matrix4x4 objectToWorld = m_terrain.m_objectToWorld;
	ObjectBufferCB* objectBuffer = (ObjectBufferCB*)(m_pRenderFrame->m_pResource);
	objectBuffer->m_objectToWorld = objectToWorld;
	objectBuffer->m_worldToProject = worldToProjection;

	ID3D12GraphicsCommandList* commandList = m_pRenderFrame->m_commandList;

	m_pRenderFrame->m_commandAllocator->Reset();
	commandList->Reset(m_pRenderFrame->m_commandAllocator, m_skybox.m_skyboxPSO);

	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetDH = m_renderTargetDH->GetCPUDescriptorHandleForHeapStart();
	renderTargetDH.ptr += m_renderTargetID * m_rtvDescriptorHandleIncrementSize;
	D3D12_CPU_DESCRIPTOR_HANDLE const depthDH = m_depthDH->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE const terrainMaterial = { m_materialsDH->GetGPUDescriptorHandleForHeapStart().ptr + m_srvDescriptorHandleIncrementSize };

	D3D12_RESOURCE_BARRIER renderTargetBarrier = {};
	renderTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	renderTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	renderTargetBarrier.Transition.pResource = m_rederTarget[m_renderTargetID];
	renderTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	renderTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	renderTargetBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(1, &renderTargetBarrier);

	commandList->OMSetRenderTargets(1, &renderTargetDH, true, &depthDH);
	commandList->ClearDepthStencilView(depthDH, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

	commandList->SetDescriptorHeaps(1, &m_materialsDH);
	
	commandList->RSSetScissorRects(1, &m_scissorRect);
	commandList->RSSetViewports(1, &m_viewport);
	
	commandList->SetGraphicsRootSignature(m_mainRS);
	
	commandList->SetGraphicsRootConstantBufferView(0, m_pRenderFrame->m_frameResource->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(1, m_materialsDH->GetGPUDescriptorHandleForHeapStart());
	
	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	commandList->IASetIndexBuffer(&m_skybox.m_indicesBufferView);
	commandList->IASetVertexBuffers(0, 1, &m_skybox.m_verticesBufferView);
	
	commandList->DrawIndexedInstanced(m_skybox.m_indicesNum, 1, 0, 0, 0);

	if (GWireframe)
	{
		commandList->SetPipelineState(m_mainWireframePSO);
	}
	else
	{
		commandList->SetPipelineState(m_mainPSO);
	}
	commandList->IASetVertexBuffers(0, 1, &m_terrain.m_verticesView);
	commandList->SetGraphicsRootDescriptorTable(1, terrainMaterial);

	for (STileData const& tileData : m_terrain.m_tilesData)
	{
		commandList->IASetIndexBuffer(&m_terrain.m_indicesView[tileData.m_lod]);
		commandList->DrawIndexedInstanced(m_terrain.m_indicesNum[tileData.m_lod], 1, 0, tileData.m_verticesOffset, 0);
	}

	renderTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	renderTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;

	commandList->ResourceBarrier(1, &renderTargetBarrier);

	CheckFailed(commandList->Close());

	ID3D12CommandList* ppMainCL[] = { commandList };
	m_mainCQ->ExecuteCommandLists(1, ppMainCL);

	CheckFailed(m_swapChain->Present(0, 0));

	m_renderTargetID = (m_renderTargetID + 1) % FRAME_NUM;
	m_renderFrameID = (m_renderFrameID + 1) % FRAME_NUM;
	m_pRenderFrame = &m_renderFrames[m_renderFrameID];
}

void CRender::Init()
{
	m_viewport.MaxDepth = 1.f;
	m_viewport.MinDepth = 0.f;
	m_viewport.Width = GWidth;
	m_viewport.Height = GHeight;
	m_viewport.TopLeftX = 0.f;
	m_viewport.TopLeftY = 0.f;

	m_scissorRect.right = (long)GWidth;
	m_scissorRect.bottom = (long)GHeight;
	m_scissorRect.left = 0;
	m_scissorRect.top = 0;

	m_fenceValue = 0;

#ifdef _DEBUG
	CheckFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&m_debugController)));
	m_debugController->EnableDebugLayer();
#endif

	CheckFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_factor)));
	CheckFailed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));

	CheckFailed(m_device->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	++m_fenceValue;
	m_fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
	if (!m_fenceEvent)
	{
		throw;
	}

	InitCommands();
	InitSwapChain();
	InitRenderTargets();
	InitRenderFrames();
	InitRootSignature();
	InitDescriptorHeaps();
	InitMainPSO();
	InitSkybox();
	InitTerrain();
}

void CRender::Release()
{
	CheckFailed(m_mainCQ->Signal(m_fence, m_fenceValue));
	CheckFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	++m_fenceValue;
	WaitForSingleObject(m_fenceEvent, INFINITE);

	CheckFailed(m_computeCQ->Signal(m_fence, m_fenceValue));
	CheckFailed(m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent));
	++m_fenceValue;
	WaitForSingleObject(m_fenceEvent, INFINITE);

	m_debugController->Release();
	
	m_mainCQ->Release();
	m_mainCA->Release();
	m_mainCL->Release();

	m_copyCQ->Release();
	m_copyCA->Release();
	m_copyCL->Release();

	m_computeCQ->Release();
	m_computeCA->Release();
	m_computeCL->Release();

	for (UINT frameID = 0; frameID < FRAME_NUM; ++frameID)
	{
		m_renderFrames[frameID].m_frameResource->Unmap(0, nullptr);
		m_renderFrames[frameID].m_frameResource->Release();
		m_renderFrames[frameID].m_commandList->Release();
		m_renderFrames[frameID].m_commandAllocator->Release();
	}

	m_renderTargetDH->Release();
	m_depthDH->Release();
	m_depthTarget->Release();
	for (UINT renderID = 0; renderID < FRAME_NUM; ++renderID)
	{
		m_rederTarget[renderID]->Release();
	}


	m_mainRS->Release();

	m_materialsDH->Release();
	m_computeDH->Release();

	m_mainPSO->Release();
	m_mainWireframePSO->Release();

	for (UINT lodID = 0; lodID < ELods::MAX; ++lodID)
	{
		m_terrain.m_indices[lodID]->Release();
	}
	m_terrain.m_heightmap->Release();
	m_terrain.m_gpuTileData->Unmap(0, nullptr);
	m_terrain.m_gpuTileData->Release();
	m_terrain.m_vertices->Release();
	m_terrain.m_terrainRS->Release();
	m_terrain.m_terrainPosPSO->Release();
	m_terrain.m_terrainLightPSOPass0->Release();
	m_terrain.m_terrainLightPSOPass1->Release();
	m_terrain.m_diffuseTex->Release();
	m_terrain.m_normalTex->Release();

	m_skybox.m_skyboxPSO->Release();
	m_skybox.m_meshVertices->Release();
	m_skybox.m_meshInidces->Release();
	m_skybox.m_texture->Release();

	m_swapChain->Release();
	m_fence->Release();
	m_factor->Release();
	m_device->Release();
}
