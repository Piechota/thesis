#pragma once
#include "pch.h"

static D3D12_HEAP_PROPERTIES const GHeapPropertiesGPUOnly =
{
	/*Type*/					D3D12_HEAP_TYPE_CUSTOM
	/*CPUPageProperty*/			,D3D12_CPU_PAGE_PROPERTY_NOT_AVAILABLE
	/*MemoryPoolPreference*/	,D3D12_MEMORY_POOL_L0
	/*CreationNodeMask*/		,1
	/*VisibleNodeMask*/			,1
};

static D3D12_HEAP_PROPERTIES const GHeapPropertiesDefault =
{
	/*Type*/					D3D12_HEAP_TYPE_DEFAULT
	/*CPUPageProperty*/			,D3D12_CPU_PAGE_PROPERTY_UNKNOWN
	/*MemoryPoolPreference*/	,D3D12_MEMORY_POOL_UNKNOWN
	/*CreationNodeMask*/		,1
	/*VisibleNodeMask*/			,1
};
static D3D12_HEAP_PROPERTIES const GHeapPropertiesUpload =
{
	/*Type*/					D3D12_HEAP_TYPE_UPLOAD
	/*CPUPageProperty*/			,D3D12_CPU_PAGE_PROPERTY_UNKNOWN
	/*MemoryPoolPreference*/	,D3D12_MEMORY_POOL_UNKNOWN
	/*CreationNodeMask*/		,1
	/*VisibleNodeMask*/			,1
};

