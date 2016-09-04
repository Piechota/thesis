#pragma once
#include "pch.h"
#include "renderFrame.h"
#include "skybox.h"

class CRender
{
private:
	enum { FRAME_NUM = 3 };

#ifdef _DEBUG
	ID3D12Debug*				m_debugController;
#endif

	ID3D12Device*				m_device;
	IDXGIFactory4*				m_factor;

	ID3D12Fence*				m_fence;
	HANDLE						m_fenceEvent;
	UINT						m_fenceValue;

	D3D12_VIEWPORT				m_viewport;
	D3D12_RECT					m_scissorRect;

	ID3D12CommandQueue*			m_mainCQ;
	ID3D12CommandAllocator*		m_mainCA;
	ID3D12GraphicsCommandList*	m_mainCL;

	ID3D12CommandQueue*			m_copyCQ;
	ID3D12CommandAllocator*		m_copyCA;
	ID3D12GraphicsCommandList*	m_copyCL;
	
	ID3D12CommandQueue*			m_computeCQ;
	ID3D12CommandAllocator*		m_computeCA;
	ID3D12GraphicsCommandList*	m_computeCL;

	SRenderFrame				m_renderFrames[FRAME_NUM];
	SRenderFrame*				m_pRenderFrame;
	UINT						m_renderFrameID;

	ID3D12DescriptorHeap*		m_renderTargetDH;
	ID3D12DescriptorHeap*		m_depthDH;
	ID3D12Resource*				m_depthTarget;
	ID3D12Resource*				m_rederTarget[FRAME_NUM];
	UINT						m_renderTargetID;

	IDXGISwapChain3*			m_swapChain;

	ID3D12RootSignature*		m_mainRS;

	ID3D12DescriptorHeap*		m_materialsDH;
	ID3D12DescriptorHeap*		m_computeDH;

	ID3D12PipelineState*		m_mainPSO;

	ID3D12RootSignature*		m_terrainGenRS;
	ID3D12PipelineState*		m_terrainGetPosPSO;
	ID3D12PipelineState*		m_terrainGetNorTanPSO;

	UINT						m_rtvDescriptorHandleIncrementSize;

	SSkybox						m_skybox;

private:
	void LoadShader(LPCWSTR pFileName, D3D_SHADER_MACRO const* pDefines, ID3DInclude* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, ID3DBlob** ppCode);
	void InitCommands();
	void InitSwapChain();
	void InitRenderTargets();
	void InitRenderFrames();
	void InitRootSignature();
	void InitDescriptorHeaps();
	void InitSkybox();

public:
	void DrawFrame();
	void Init();
	void Release();
};