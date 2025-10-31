#pragma once

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>

#include "d3dx12.h"
#include "SHRUtils.h"

class SHRD3D12Resource
{
public:
	SHRD3D12Resource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON) : m_pResource(pResource), m_currentState(initialState) {};
	~SHRD3D12Resource();

	void Map(UINT subresource, D3D12_RANGE* pReadRange = nullptr);

	D3D12_RESOURCE_DESC GetResourceDesc();

public:
	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource = nullptr;

	D3D12_GPU_VIRTUAL_ADDRESS m_resourceGPUAddress = 0;

	D3D12_RESOURCE_STATES m_currentState;

	void* m_mappedBaseAddress = nullptr;
};

