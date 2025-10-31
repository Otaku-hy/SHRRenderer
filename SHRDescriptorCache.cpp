#include "SHRDescriptorCache.h"

SHRDescriptorCache::SHRDescriptorCache(ID3D12Device* pDevice, UINT cbvSrvUavDescriptorCount, UINT samplerDescriptorCount)
{
	m_pDevice = pDevice;
	m_cbvSrvUavDescriptorCount = cbvSrvUavDescriptorCount;
	m_samplerDescriptorCount = samplerDescriptorCount;

	m_cbvSrvUavIncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	m_samplerIncrementSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	CreateCbvSrvUavHeap();
	CreateSamplerHeap();
}

SHRDescriptorCache::~SHRDescriptorCache()
{

}

std::pair<UINT, D3D12_GPU_DESCRIPTOR_HANDLE> SHRDescriptorCache::CacheCbvSrvUavDescriptor(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& descriptors)
{
	UINT requiredCacheSize = descriptors.size();

	CD3DX12_CPU_DESCRIPTOR_HANDLE cacheCPUHandle(m_pCbvSrvUavHeap->GetCPUDescriptorHandleForHeapStart(), m_cbvSrvUavOffset, m_cbvSrvUavIncrementSize);;
	m_pDevice->CopyDescriptors(1, &cacheCPUHandle, &requiredCacheSize, requiredCacheSize, descriptors.data(), nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	UINT beginOffset = m_cbvSrvUavOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE cacheGPUHandle(m_pCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart(), m_cbvSrvUavOffset, m_cbvSrvUavIncrementSize);
	m_cbvSrvUavOffset += requiredCacheSize;
	return { beginOffset, cacheGPUHandle };
}

std::pair<UINT, D3D12_GPU_DESCRIPTOR_HANDLE> SHRDescriptorCache::CacheSamplerDescriptor(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& descriptors)
{
	UINT requiredCacheSize = descriptors.size();

	CD3DX12_CPU_DESCRIPTOR_HANDLE cacheCPUHandle(m_pSamplerHeap->GetCPUDescriptorHandleForHeapStart(), m_samplerOffset, m_samplerIncrementSize);
	m_pDevice->CopyDescriptors(1, &cacheCPUHandle, &requiredCacheSize, requiredCacheSize, descriptors.data(), nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

	UINT beginOffset = m_samplerOffset;
	CD3DX12_GPU_DESCRIPTOR_HANDLE cacheGPUHandle(m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart(), m_samplerOffset, m_samplerIncrementSize);
	m_samplerOffset += requiredCacheSize;
	return { beginOffset, cacheGPUHandle };
}

void SHRDescriptorCache::CreateCbvSrvUavHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	desc.NodeMask = 0;
	desc.NumDescriptors = m_cbvSrvUavDescriptorCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pCbvSrvUavHeap)));
	m_cbvSrvUavOffset = 0;
}

void SHRDescriptorCache::CreateSamplerHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	desc.NodeMask = 0;
	desc.NumDescriptors = m_samplerDescriptorCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pSamplerHeap)));
	m_samplerOffset = 0;
}

void SHRDescriptorCache::ClearCache()
{
	m_samplerOffset = 0;
	m_cbvSrvUavOffset = 0;
}

D3D12_GPU_DESCRIPTOR_HANDLE SHRDescriptorCache::GetCbvSrvUavHeapBaseHandle()
{
	return m_pCbvSrvUavHeap->GetGPUDescriptorHandleForHeapStart();
}

D3D12_GPU_DESCRIPTOR_HANDLE SHRDescriptorCache::GetSamplerHeapBaseHandle()
{
	return m_pSamplerHeap->GetGPUDescriptorHandleForHeapStart();
}