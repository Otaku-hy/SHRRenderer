#include "SHRResource.h"
#include "SHRResourceAllocator.h"

SHRResource::SHRResource(ID3D12Resource* pResource, D3D12_RESOURCE_STATES initialState)
	: m_block{ 0, 0, nullptr, nullptr }
	, m_pSHRD3dResource(nullptr)
{
	m_pSHRD3dResource = std::make_unique<SHRD3D12Resource>(pResource, initialState);
}

SHRResource::~SHRResource()
{
	if (m_block.pAllocator)
	{
		m_block.pResource = m_pSHRD3dResource.release();
		m_block.pAllocator->DeallocateSHRResource(m_block);
	}
}