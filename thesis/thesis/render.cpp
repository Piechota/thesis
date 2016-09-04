#include "camera.h"
#include "render.h"
#include "constantBuffers.h"
#include "meshLoader.h"
#include "textureLoader.h"
#include "heapProperties.h"

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
	descResource.Width = sizeof(FrameCB);

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
	}

	m_renderFrameID = 0;
	m_pRenderFrame = &m_renderFrames[m_renderFrameID];
}

void CRender::InitRootSignature()
{
	D3D12_DESCRIPTOR_RANGE descriptorRange[] = 
	{
		{ D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND }
	};

	D3D12_ROOT_PARAMETER rootParameter[2];
	rootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameter[0].Descriptor = { 0, 0 };
	rootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

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
	descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* signature;
	CheckFailed(D3D12SerializeRootSignature(&descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr));
	CheckFailed(m_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_mainRS)));
	signature->Release();
}

void CRender::InitDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC descDescHeap = {};
	descDescHeap.NumDescriptors = 1;
	descDescHeap.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	descDescHeap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	CheckFailed(m_device->CreateDescriptorHeap(&descDescHeap, IID_PPV_ARGS(&m_materialsDH)));
	m_materialsDH->SetName(L"MaterialsDescriptorHeaps");
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

	descResource.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	descResource.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	descResource.MipLevels = skyboxTexture.m_mipNum;
	descResource.Width = skyboxTexture.m_x[0];
	descResource.Height = skyboxTexture.m_y[0];
	descResource.Format = skyboxTexture.m_format;

	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesDefault, D3D12_HEAP_FLAG_NONE, &descResource, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&m_skybox.m_texture)));

	UINT64 bufferSize;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pFootprints = new D3D12_PLACED_SUBRESOURCE_FOOTPRINT[skyboxTexture.m_mipNum];
	UINT* pNumRows = new UINT[skyboxTexture.m_mipNum];
	UINT64* pRowPitches = new UINT64[skyboxTexture.m_mipNum];
	m_device->GetCopyableFootprints(&descResource, 0, skyboxTexture.m_mipNum, 0, pFootprints, pNumRows, pRowPitches, &bufferSize);

	descResource.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	descResource.Format = DXGI_FORMAT_UNKNOWN;
	descResource.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	descResource.MipLevels = 1;
	descResource.Height = 1;
	descResource.Width = bufferSize;

	ID3D12Resource* textureUploadRes;
	CheckFailed(m_device->CreateCommittedResource(&GHeapPropertiesUpload, D3D12_HEAP_FLAG_NONE, &descResource, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&textureUploadRes)));

	m_copyCA->Reset();
	m_copyCL->Reset(m_copyCA, nullptr);
	m_copyCL->CopyResource(m_skybox.m_meshVertices, verticesUploadRes);
	m_copyCL->CopyResource(m_skybox.m_meshInidces, indicesUploadRes);

	textureUploadRes->Map(0, nullptr, &pGPU);
	for (UINT mipID = 0; mipID < skyboxTexture.m_mipNum; ++mipID)
	{
		for (UINT rowID = 0; rowID < pNumRows[mipID]; ++rowID)
		{
			memcpy((BYTE*)pGPU + pFootprints[mipID].Offset + rowID * pFootprints[mipID].Footprint.RowPitch, skyboxTexture.m_data[mipID] + rowID * pRowPitches[mipID], pRowPitches[mipID]);
		}

		D3D12_TEXTURE_COPY_LOCATION dst;
		dst.pResource = m_skybox.m_texture;
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

	m_copyCL->Close();
	ID3D12CommandList* ppCopyCL[] = { m_copyCL };
	m_copyCQ->ExecuteCommandLists(1, ppCopyCL);

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
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.MostDetailedMip = 0;

	m_device->CreateShaderResourceView(m_skybox.m_texture, &srvDesc, m_materialsDH->GetCPUDescriptorHandleForHeapStart());

	m_skybox.m_indicesBufferView.BufferLocation = m_skybox.m_meshInidces->GetGPUVirtualAddress();
	m_skybox.m_indicesBufferView.Format = DXGI_FORMAT_R32_UINT;
	m_skybox.m_indicesBufferView.SizeInBytes = m_skybox.m_indicesNum * sizeof(UINT32);

	m_skybox.m_verticesBufferView.BufferLocation = m_skybox.m_meshVertices->GetGPUVirtualAddress();
	m_skybox.m_verticesBufferView.StrideInBytes = sizeof(SVertexData);
	m_skybox.m_verticesBufferView.SizeInBytes = verticesNum * sizeof(SVertexData);
}

void CRender::DrawFrame()
{
	Matrix4x4 worldToProjection = GCamera.GetWorldToCamera() * GCamera.GetProjectionMatrix();
	memcpy(m_pRenderFrame->m_pResource, &worldToProjection, sizeof(Matrix4x4));

	ID3D12GraphicsCommandList* commandList = m_pRenderFrame->m_commandList;

	m_pRenderFrame->m_commandAllocator->Reset();
	commandList->Reset(m_pRenderFrame->m_commandAllocator, m_skybox.m_skyboxPSO);

	D3D12_CPU_DESCRIPTOR_HANDLE renderTargetDH = m_renderTargetDH->GetCPUDescriptorHandleForHeapStart();
	renderTargetDH.ptr += m_renderTargetID * m_rtvDescriptorHandleIncrementSize;
	D3D12_CPU_DESCRIPTOR_HANDLE depthDH = m_depthDH->GetCPUDescriptorHandleForHeapStart();

	D3D12_RESOURCE_BARRIER renderTargetBarrier = {};
	renderTargetBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	renderTargetBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	renderTargetBarrier.Transition.pResource = m_rederTarget[m_renderTargetID];
	renderTargetBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	renderTargetBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	renderTargetBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	commandList->ResourceBarrier(1, &renderTargetBarrier);

	commandList->OMSetRenderTargets(1, &renderTargetDH, true, &depthDH);
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
	InitSkybox();
}

void CRender::Release()
{
	CheckFailed(m_mainCQ->Signal(m_fence, m_fenceValue));
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
	//m_computeDH;

	//m_mainPSO->Release();

	//m_terrainGenRS;
	//m_terrainGetPosPSO;
	//m_terrainGetNorTanPSO;

	m_skybox.m_skyboxPSO->Release();
	m_skybox.m_meshVertices->Release();
	m_skybox.m_meshInidces->Release();
	m_skybox.m_texture->Release();

	m_swapChain->Release();
	m_fence->Release();
	m_factor->Release();
	m_device->Release();
}
