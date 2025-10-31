#pragma once

#include "SHRD3D12Resource.h"

class SHRResourceAllocator;

class SHRResource
{
public:
	struct ResourceBlock
	{
		UINT64 layer;
		UINT64 offset;
		SHRResourceAllocator* pAllocator;
		SHRD3D12Resource* pResource;
	};
public:
	SHRResource() = default;
	SHRResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON);
	~SHRResource();
	SHRResource(const SHRResource&) = delete;
	SHRResource(SHRResource&& other) = default;
	SHRResource& operator=(const SHRResource&) = delete;
	SHRResource& operator=(SHRResource&& other) = default;

	void* Map(UINT subresource, D3D12_RANGE* pReadRange = nullptr) { m_pSHRD3dResource->Map(subresource, pReadRange); return m_pSHRD3dResource->m_mappedBaseAddress; };

public:
	std::unique_ptr<SHRD3D12Resource> m_pSHRD3dResource;
	ResourceBlock m_block;
};