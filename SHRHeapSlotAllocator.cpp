#include "SHRHeapSlotAllocator.h"

D3D12_DESCRIPTOR_HEAP_DESC SHRHeapSlotAllocator::CreateHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors)
{
	//heap allocator uses to store view in CPU heap then copy to GPU heap
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;
	desc.NumDescriptors = numDescriptors;
	desc.Type = type;

	return desc;
}

SHRHeapSlotAllocator::SHRHeapSlotAllocator(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors)
{
	m_pDevice = pDevice;
	m_desc = SHRHeapSlotAllocator::CreateHeapDesc(type, numDescriptors);
	m_incrementSize = m_pDevice->GetDescriptorHandleIncrementSize(type);
	
	AllocateNewHeap();
}

void SHRHeapSlotAllocator::AllocateNewHeap()
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
	ThrowIfFailed(m_pDevice->CreateDescriptorHeap(&m_desc, IID_PPV_ARGS(&pHeap)));

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(pHeap->GetCPUDescriptorHandleForHeapStart());

	HeapEntry entry;
	entry.pHeap = pHeap;
	entry.freeList.emplace_back(handle.ptr,handle.ptr + m_incrementSize * m_desc.NumDescriptors);
	
	heapMap.emplace_back(std::move(entry));
}

SHRHeapSlotAllocator::HeapSlot SHRHeapSlotAllocator::AllocateSlot()
{
	HeapSlot slot = {};

	int heapIndex = -1;
	for (size_t i = 0; i < heapMap.size(); i++)
	{
		HeapEntry& entry = heapMap[i];
		if (entry.freeList.size() != 0)
		{
			heapIndex = i;
			break;
		}
	}
	if (heapIndex == -1)
	{
		AllocateNewHeap();
		heapIndex = heapMap.size() - 1;
	}

	HeapEntry& entry = heapMap[heapIndex];
	FreeRange& range = entry.freeList.front();
	slot.heapIndex = heapIndex;
	slot.slotHandle.ptr = range.begin;

	range.begin += m_incrementSize;
	if (range.begin == range.end)
	{
		entry.freeList.erase(entry.freeList.begin());
	}

	return slot;
}

void SHRHeapSlotAllocator::DeallocateSlot(const HeapSlot& slot)
{
	//not the full version
	//lack the situation that the deallocate block is next to both the prev and next block, so that they should be merged into one
	//large block
	//but it's not a big deal, the heap slot allocator is used to manage the descriptor heap, the availble size does not matter much 
	HeapEntry& entry = heapMap[slot.heapIndex];
	
	for (auto& range : entry.freeList)
	{
		if (slot.slotHandle.ptr == range.end)
		{
			range.end += m_incrementSize;
			return;
		}
		else if (slot.slotHandle.ptr + m_incrementSize == range.begin)
		{
			range.begin -= m_incrementSize;
			return;
		}
	}
	entry.freeList.emplace_back(slot.slotHandle.ptr,slot.slotHandle.ptr + m_incrementSize);
}