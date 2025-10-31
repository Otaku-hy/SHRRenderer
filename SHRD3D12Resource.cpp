#include "SHRD3D12Resource.h"

SHRD3D12Resource::~SHRD3D12Resource()
{
	if (m_mappedBaseAddress)
	{
		m_pResource->Unmap(0, nullptr);
		m_mappedBaseAddress = nullptr;
	}
}

D3D12_RESOURCE_DESC SHRD3D12Resource::GetResourceDesc()
{
	if (m_pResource)
		return m_pResource->GetDesc();
	return {};
}

void SHRD3D12Resource::Map(UINT subresource, D3D12_RANGE* pReadRange)
{
	ThrowIfFailed(m_pResource->Map(subresource, pReadRange, &m_mappedBaseAddress));
}