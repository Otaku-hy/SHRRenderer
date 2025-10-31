#pragma once

#include "SHRHeapSlotAllocator.h"

#define SHR_DEFAULT_CBV_SRV_UAV_DESCRIPTOR_COUNT 1024
#define SHR_DEFAULT_SAMPLER_DESCRIPTOR_COUNT 256

class SHRDescriptorCache
{
public:
	SHRDescriptorCache(ID3D12Device* pDevice, UINT cbvSrvUavDescriptorCount = SHR_DEFAULT_CBV_SRV_UAV_DESCRIPTOR_COUNT, UINT samplerDescriptorCount = SHR_DEFAULT_SAMPLER_DESCRIPTOR_COUNT);
	~SHRDescriptorCache();

	std::pair<UINT, D3D12_GPU_DESCRIPTOR_HANDLE> CacheCbvSrvUavDescriptor(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& descriptors);
	std::pair<UINT, D3D12_GPU_DESCRIPTOR_HANDLE> CacheSamplerDescriptor(const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& descriptors);

	void ClearCache();

	D3D12_GPU_DESCRIPTOR_HANDLE GetCbvSrvUavHeapBaseHandle();
	D3D12_GPU_DESCRIPTOR_HANDLE GetSamplerHeapBaseHandle();

private:
	void CreateCbvSrvUavHeap();
	void CreateSamplerHeap();

public:
	//only sampler & cbv_srv_uav need to be cached on GPU
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pCbvSrvUavHeap;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pSamplerHeap;


public:
	UINT m_cbvSrvUavIncrementSize;
	UINT m_samplerIncrementSize;

	UINT m_cbvSrvUavDescriptorCount;
	UINT m_samplerDescriptorCount;

	UINT m_cbvSrvUavOffset;
	UINT m_samplerOffset;

private:
	ID3D12Device* m_pDevice;

};

