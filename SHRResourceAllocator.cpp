#include "SHRResourceAllocator.h"

///////
// Texture -> SFL -> default heap -> heap_flag_deny_buffers & YES rtv/dsv
// Buffer -> Buddy -> default/upload heap -> heap_flag_allow_only_buffers & NO rtv/dsv
// heap tier 1
//////
static D3D12_HEAP_FLAGS GetAllocatorFlags(SHRAllocatorType type, const D3D12_RESOURCE_DESC& desc)
{
	D3D12_HEAP_FLAGS flag = D3D12_HEAP_FLAG_NONE;

	switch (type)
	{
	case SHRAllocatorType::Segregated:
	{
		flag |= D3D12_HEAP_FLAG_DENY_BUFFERS;					//normal
		if (desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			flag |= D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;      //only rt&ds
		}
		else
		{
			flag |= D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;  //normal texture
		
		}
	}
	break;
	case SHRAllocatorType::Buddy:
	{
		flag |= D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;				//normal
	}
	break;
	}

	return flag;
}

static BOOL ValidateAllocation(const SHRAllocatorDesc& allocDesc, const D3D12_RESOURCE_DESC& resDesc, D3D12_HEAP_TYPE resourceAllocType)
{
	BOOL isValid = allocDesc.properties.Type == resourceAllocType;
	isValid &= allocDesc.flags == GetAllocatorFlags(allocDesc.type, resDesc);
	return isValid;
}

SHRResourceAllocator::SHRResourceAllocator(ID3D12Device* pDevice, const SHRAllocatorDesc& initData)
{
	Initialize(pDevice, initData);
}

void SHRResourceAllocator::Initialize(ID3D12Device* pDevice, const SHRAllocatorDesc& initData)
{
	m_pDevice = pDevice;
	m_allocatorDesc = initData;

	//检查对齐方式是否合法
	m_allocatorDesc.heapBlockSize = m_allocatorDesc.heapBlockSize < m_allocatorDesc.alignment ? m_allocatorDesc.alignment : m_allocatorDesc.heapBlockSize;
}

SHRAllocatorDesc SHRResourceAllocator::GetDesc()
{
	return m_allocatorDesc;
}

SHRBuddyAllocator::SHRBuddyAllocator(ID3D12Device* pDevice, const SHRAllocatorDesc& initData) : SHRResourceAllocator(pDevice, initData)
{
	Initialize(pDevice, initData);
}

void SHRBuddyAllocator::Initialize(ID3D12Device* pDevice, const SHRAllocatorDesc& initData)
{
	m_allocatorDesc.buddyParams.heapMaxSize = m_allocatorDesc.buddyParams.heapMaxSize > m_allocatorDesc.heapBlockSize ? m_allocatorDesc.buddyParams.heapMaxSize : m_allocatorDesc.heapBlockSize * SHR_BUDDY_HEAP_DEFAULT_BLOCK_COUNT;

	UINT64 blockNum = m_allocatorDesc.buddyParams.heapMaxSize / m_allocatorDesc.heapBlockSize;
	m_blockStates.resize(2 * blockNum - 1);

	D3D12_HEAP_DESC heapDesc = {};
	heapDesc.Alignment = m_allocatorDesc.alignment;
	heapDesc.Properties;
	heapDesc.Flags = m_allocatorDesc.flags;
	heapDesc.Properties = initData.properties;
	heapDesc.SizeInBytes = m_allocatorDesc.buddyParams.heapMaxSize;

	ID3D12Heap* pHeap;
	ThrowIfFailed(m_pDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(&pHeap)));

	m_pHeap = pHeap;

	for (auto it = m_blockStates.begin(); it != m_blockStates.end(); it++)
	{
		it->valid = 1;
		it->pResource = nullptr;
	}
}


bool SHRBuddyAllocator::AllocateSHRResouce(const D3D12_RESOURCE_DESC& desc,
	const D3D12_RESOURCE_STATES& initState,
	UINT size, SHRResource& resource,
	const D3D12_CLEAR_VALUE* clrValue)
{
	if (desc.Alignment > m_allocatorDesc.alignment) return false; //只有对齐方式小于等于堆对齐方式才能分配

	UINT64 allocSize = GetAllocateSize(size, desc.Alignment);

	auto [layer, offset] = CanAllocate(allocSize);

	if (layer == -1)
	{
		return false;
	}

	AllocateBlock(layer, offset, resource);

	UINT64 addressOffset = GetRealAllocatedLocation(size, desc.Alignment, layer, offset);

	ID3D12Resource* pResource;
	ThrowIfFailed(m_pDevice->CreatePlacedResource(m_pHeap.Get(), addressOffset, &desc, initState, clrValue, IID_PPV_ARGS(&pResource)));
	resource.m_pSHRD3dResource = std::make_unique<SHRD3D12Resource>(pResource,initState);
	resource.m_block.layer = layer;
	resource.m_block.offset = offset;
	resource.m_block.pAllocator = this;
	resource.m_block.pResource = resource.m_pSHRD3dResource.get();
	resource.m_pSHRD3dResource->m_resourceGPUAddress = pResource->GetGPUVirtualAddress();

	return true;
}

void SHRBuddyAllocator::DeallocateSHRResource(SHRResource::ResourceBlock& block)
{
	m_defferedDeletionList.push_back(block);
}

void SHRBuddyAllocator::DeallocateImmediate(SHRResource::ResourceBlock& block)
{
	DeallocateBlock(block.layer, block.offset);

	if (block.pResource)
	{
		delete block.pResource;
	}
}

void SHRBuddyAllocator::DeallocateBlock(UINT64 layer, UINT64 offset)
{
	UINT baseOffset = (1 << layer) - 1;
	UINT realOffset = static_cast<UINT>(offset) + baseOffset;

	m_blockStates[realOffset].valid = 1;

	UINT parentOffset = (realOffset - 1) >> 1;
	while (parentOffset)
	{
		if (m_blockStates[parentOffset * 2 + 1].valid && m_blockStates[parentOffset * 2 + 2].valid)
		{
			m_blockStates[parentOffset].valid = 1;
		}
		parentOffset = (parentOffset - 1) >> 1;
	}

	UINT rangeBegin = realOffset * 2 + 1;
	UINT rangeEnd = realOffset * 2 + 2;
	while (rangeEnd < m_blockStates.size())
	{
		for (UINT it = 0; rangeBegin + it <= rangeEnd; it++)
		{
			m_blockStates[rangeBegin + it].valid = 1;
		}
		rangeBegin = rangeBegin * 2 + 1;
		rangeEnd = rangeEnd * 2 + 2;
	}
}

void SHRBuddyAllocator::CleanupHeap()
{
	for (size_t i = 0; i < m_defferedDeletionList.size(); i++)
	{
		SHRResource::ResourceBlock& block = m_defferedDeletionList[i];
		DeallocateImmediate(block);
	}

	m_defferedDeletionList.clear();
}

UINT64 SHRBuddyAllocator::GetRealAllocatedLocation(UINT64 size, UINT64 alignment, UINT64 layer, UINT64 offset)
{
	UINT64 layerBlockSize = m_allocatorDesc.buddyParams.heapMaxSize >> layer;
	UINT64 blockBaseAddressOffset = layerBlockSize * offset;
	return alignment == 0? blockBaseAddressOffset : UPPER_ALIGNMENT(blockBaseAddressOffset, alignment);
}

UINT64 SHRBuddyAllocator::GetAllocateSize(UINT64 size, UINT64 alignment)
{
	if (alignment != 0 && m_allocatorDesc.heapBlockSize % alignment != 0)
	{
		return size + alignment;
	}
	return size;
}

std::pair<INT, UINT> SHRBuddyAllocator::CanAllocate(UINT64 size)
{
	if (m_allocatorDesc.buddyParams.heapMaxSize < size) return { -1,0 };

	int layer = 0;
	while ((m_allocatorDesc.buddyParams.heapMaxSize >> layer) >= size && (m_allocatorDesc.buddyParams.heapMaxSize >> layer) >= m_allocatorDesc.heapBlockSize)
	{
		layer++;
	}
	layer -= 1;

	UINT baseOffset = (1 << layer) - 1;
	for (UINT offset = 0; offset < (1ULL << layer); offset++)
	{
		if (m_blockStates[offset + baseOffset].valid)
		{
			return { layer, offset };
		}
	}
	return { -1,0 };
}

void SHRBuddyAllocator::AllocateBlock(UINT layer, UINT offset, SHRResource& resource)
{
	UINT baseOffset = (1 << layer) - 1;
	UINT realOffset = offset + baseOffset;

	UINT parentOffset = realOffset;
	m_blockStates[realOffset].pResource = &resource;
	while (parentOffset > 0)
	{
		m_blockStates[parentOffset].valid = 0;
		parentOffset = (parentOffset - 1) >> 1;
	}
	m_blockStates[parentOffset].valid = 0;

	UINT rangeBegin = realOffset * 2 + 1;
	UINT rangeEnd = realOffset * 2 + 2;
	while (rangeEnd < m_blockStates.size())
	{
		for (UINT it = 0; rangeBegin + it <= rangeEnd; it++)
		{
			m_blockStates[rangeBegin + it].valid = 0;
		}
		rangeBegin = rangeBegin * 2 + 1;
		rangeEnd = rangeEnd * 2 + 2;
	}
}


SHRSegregatedAllocator::~SHRSegregatedAllocator()
{
	delete[] m_pBlockStates;
}


SHRSegregatedAllocator::SHRSegregatedAllocator(ID3D12Device* pDevice, const SHRAllocatorDesc& initData) : SHRResourceAllocator(pDevice, initData)
{
	Initialize(pDevice, initData);
}

///
void SHRSegregatedAllocator::Initialize(ID3D12Device* pDevice, const SHRAllocatorDesc& initData)
{
	m_allocatorDesc = initData;
	m_pHeaps.resize(m_allocatorDesc.sflParams.heapLayerNum, nullptr); ////heaps 过多会导致内存占用过大，惰性分配
	m_pBlockStates = new std::vector<BlockState>[m_allocatorDesc.sflParams.heapLayerNum];

	for (size_t layer = 0; layer < m_allocatorDesc.sflParams.heapLayerNum; layer++)
	{
		m_pBlockStates[layer].resize(SHR_SEGREGATED_HEAP_DEFAULT_BLOCK_COUNT, { 0,nullptr });
	}
}

bool SHRSegregatedAllocator::AllocateSHRResouce(const D3D12_RESOURCE_DESC& desc,
	const D3D12_RESOURCE_STATES& initState,
	UINT size, SHRResource& resource,
	const D3D12_CLEAR_VALUE* clrValue)
{
	if (desc.Alignment > m_allocatorDesc.alignment) return false; //只有对齐方式小于等于堆对齐方式才能分配

	UINT64 allocSize = GetAllocateSize(size, desc.Alignment);

	auto [layer, offset] = CanAllocate(allocSize);

	if (layer == -1)
	{
		return false;
	}

	AllocateBlock(layer, offset, resource);

	UINT64 addressOffset = GetRealAllocatedLocation(size, desc.Alignment, layer, offset);

	ID3D12Resource* pResource;
	ThrowIfFailed(m_pDevice->CreatePlacedResource(m_pHeaps[layer].Get(), addressOffset, &desc, initState, clrValue, IID_PPV_ARGS(&pResource)));

	resource.m_pSHRD3dResource = std::make_unique<SHRD3D12Resource>(pResource, initState);
	resource.m_block.layer = layer;
	resource.m_block.offset = offset;
	resource.m_block.offset = offset;
	resource.m_block.pAllocator = this;
	resource.m_block.pResource = resource.m_pSHRD3dResource.get();
	resource.m_pSHRD3dResource->m_resourceGPUAddress = pResource->GetGPUVirtualAddress();

	return true;
}

void SHRSegregatedAllocator::DeallocateSHRResource(SHRResource::ResourceBlock& block)
{
	m_defferedDeletionList.push_back(block);
}

void SHRSegregatedAllocator::DeallocateImmediate(SHRResource::ResourceBlock& block)
{
	DeallocateBlock(block.layer, block.offset);
	if (block.pResource)
	{
		delete block.pResource;
	}
}

void SHRSegregatedAllocator::CleanupHeap()
{
	for (size_t i = 0; i < m_defferedDeletionList.size(); i++)
	{
		SHRResource::ResourceBlock& block = m_defferedDeletionList[i];
		DeallocateImmediate(block);
	}

	m_defferedDeletionList.clear();
}

std::pair<INT, UINT> SHRSegregatedAllocator::CanAllocate(UINT64 size)
{
	INT layer = static_cast<INT>(m_allocatorDesc.sflParams.heapLayerNum) - 1;
	while (layer >= 0)
	{
		UINT64 blockSize = m_allocatorDesc.heapBlockSize << (SHR_SEGREGATED_HEAP_DEFAULT_BLOCK_COUNT - 1 - layer);
		if (blockSize >= size)
		{
			if (!m_pHeaps[layer]) CreateHeap(layer);

			for (UINT offset = 0; offset < SHR_SEGREGATED_HEAP_DEFAULT_BLOCK_COUNT; offset++)
			{
				if (m_pBlockStates[layer][offset].valid)
				{
					return { layer, offset };
				}
			}
		}
		layer--;
	}
	return { -1,0 };
}

void SHRSegregatedAllocator::AllocateBlock(UINT layer, UINT offset, SHRResource& resource)
{
	BlockState& block = m_pBlockStates[layer][offset];
	block.pResource = &resource;
	block.valid = 0;
}

void SHRSegregatedAllocator::DeallocateBlock(UINT64 layer, UINT64 offset)
{
	BlockState& block = m_pBlockStates[layer][offset];
	block.valid = 1;
	block.pResource = nullptr;
}

UINT64 SHRSegregatedAllocator::GetAllocateSize(UINT64 size, UINT64 alignment)
{
	if (alignment != 0 && m_allocatorDesc.heapBlockSize % alignment != 0)
	{
		return size + alignment;
	}
	return size;
}

UINT64 SHRSegregatedAllocator::GetRealAllocatedLocation(UINT64 size, UINT64 alignment, UINT64 layer, UINT64 offset)
{
	UINT64 layerBlockSize = m_allocatorDesc.heapBlockSize << (m_allocatorDesc.sflParams.heapLayerNum - 1 - layer);
	UINT64 blockBaseAddressOffset = layerBlockSize * offset;
	return alignment == 0 ? blockBaseAddressOffset : UPPER_ALIGNMENT(blockBaseAddressOffset, alignment);
}

void SHRSegregatedAllocator::CreateHeap(UINT layer)
{
	UINT64 size = (m_allocatorDesc.heapBlockSize << (SHR_SEGREGATED_HEAP_DEFAULT_BLOCK_COUNT - 1 - layer)) * SHR_SEGREGATED_HEAP_DEFAULT_BLOCK_COUNT;

	D3D12_HEAP_DESC heapDesc = {};
	heapDesc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	heapDesc.Properties;
	heapDesc.Flags = m_allocatorDesc.flags;
	heapDesc.Properties = m_allocatorDesc.properties;
	heapDesc.SizeInBytes = size;

	ID3D12Heap* pHeap;
	ThrowIfFailed(m_pDevice->CreateHeap(&heapDesc, IID_PPV_ARGS(&pHeap)));

	m_pHeaps[layer] = pHeap;

	for (auto it = m_pBlockStates[layer].begin(); it != m_pBlockStates[layer].end(); it++)
	{
		it->valid = 1;
		it->pResource = nullptr;
	}
}

SHRBuddySystem::SHRBuddySystem(ID3D12Device* pDevice)
{
	Initialize(pDevice);
}

void SHRBuddySystem::Initialize(ID3D12Device* pDevice)
{
	m_pDevice = pDevice;
}

bool SHRBuddySystem::AllocateSHRResouce(D3D12_HEAP_TYPE requiredType, const D3D12_RESOURCE_DESC& desc,
	const D3D12_RESOURCE_STATES& initState,
	UINT size, SHRResource& resource,
	const D3D12_CLEAR_VALUE* clrValue)
{
	bool result = false;

	for (size_t i = 0; i < m_allocators.size(); i++)
	{
		SHRBuddyAllocator& allocator = m_allocators[i];
		result = ValidateAllocation(allocator.GetDesc(), desc, requiredType);
		if (result) result = allocator.AllocateSHRResouce(desc, initState, size, resource, clrValue);
		if (result) break;
	}

	if (!result)
	{
		BOOL isDefault = desc.Alignment <= D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

		CD3DX12_HEAP_PROPERTIES properties(requiredType);

		SHRAllocatorDesc allocDesc = {};
		allocDesc.type = SHRAllocatorType::Buddy;
		allocDesc.properties = properties;
		allocDesc.flags = GetAllocatorFlags(SHRAllocatorType::Buddy, desc);
		allocDesc.heapBlockSize = max(SHR_BUDDY_HEAP_DEFAULT_BLOCK_SIZE, desc.Alignment);
		allocDesc.buddyParams.heapMaxSize = isDefault ? SHR_BUDDY_HEAP_DEFAULT_MAX_SIZE : allocDesc.heapBlockSize * SHR_BUDDY_HEAP_DEFAULT_BLOCK_COUNT;
		allocDesc.alignment = isDefault ? D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT : desc.Alignment;

		m_allocators.emplace_back(m_pDevice, allocDesc);
		m_allocators[m_allocators.size() - 1].AllocateSHRResouce(desc, initState, size, resource, clrValue);
	}

	return result;
}

void SHRBuddySystem::CleanupSystem()
{
	for (size_t i = 0; i < m_allocators.size(); i++)
	{
		SHRResourceAllocator& allocator = m_allocators[i];
		allocator.CleanupHeap();
	}
}


SHRSegregatedListSystem::SHRSegregatedListSystem(ID3D12Device* pDevice)
{
	Initialize(pDevice);
}

void SHRSegregatedListSystem::Initialize(ID3D12Device* pDevice)
{
	m_pDevice = pDevice;
}

bool SHRSegregatedListSystem::AllocateSHRResouce(D3D12_HEAP_TYPE requiredType, const D3D12_RESOURCE_DESC& desc,
	const D3D12_RESOURCE_STATES& initState,
	UINT size, SHRResource& resource,
	const D3D12_CLEAR_VALUE* clrValue)
{
	bool result = false;

	for (size_t i = 0; i < m_allocators.size(); i++)
	{
		SHRSegregatedAllocator& allocator = m_allocators[i];
		result = ValidateAllocation(allocator.GetDesc(), desc, requiredType);
		if (result) result = allocator.AllocateSHRResouce(desc, initState, size, resource, clrValue);
		if (result) break;
	}

	if (!result)
	{
		BOOL isDefault = desc.Alignment <= D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;

		CD3DX12_HEAP_PROPERTIES properties(requiredType);

		SHRAllocatorDesc allocDesc = {};
		allocDesc.type = SHRAllocatorType::Segregated;
		allocDesc.properties = properties;
		allocDesc.flags = GetAllocatorFlags(SHRAllocatorType::Segregated, desc);
		allocDesc.alignment = isDefault ? D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT : desc.Alignment;
		allocDesc.heapBlockSize = allocDesc.alignment;
		allocDesc.sflParams.heapLayerNum = SHR_SEGREGATED_HEAP_DEFAULT_LAYER_NUM;
		m_allocators.emplace_back(m_pDevice, allocDesc);
		m_allocators[m_allocators.size() - 1].AllocateSHRResouce(desc, initState, size, resource, clrValue);
	}

	return result;
}

void SHRSegregatedListSystem::CleanupSystem()
{
	for (size_t i = 0; i < m_allocators.size(); i++)
	{
		SHRResourceAllocator& allocator = m_allocators[i];
		allocator.CleanupHeap();
	}
}
