#pragma once

#include <vector>

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>

#include "d3dx12.h"
#include "SHRUtils.h"

#define SHR_DEFAULT_DESCRIPTOR_HEAP_SIZE 1024

class SHRHeapSlotAllocator
{
public:
	typedef decltype(D3D12_CPU_DESCRIPTOR_HANDLE::ptr) CPUHandleRaw;
	
	struct HeapSlot
	{
		UINT heapIndex;
		D3D12_CPU_DESCRIPTOR_HANDLE slotHandle;
	};

	struct FreeRange
	{
		CPUHandleRaw begin;
		CPUHandleRaw end;

		FreeRange(CPUHandleRaw a, CPUHandleRaw b) : begin(a), end(b) {};
	};

	struct HeapEntry
	{
		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
		std::vector<FreeRange> freeList;
	};

public:
	static D3D12_DESCRIPTOR_HEAP_DESC CreateHeapDesc(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors);

	SHRHeapSlotAllocator(ID3D12Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE type, UINT numDescriptors);
	~SHRHeapSlotAllocator() = default;

	HeapSlot AllocateSlot();
	void DeallocateSlot(const HeapSlot& slot);

private:
	void AllocateNewHeap();

public:
	std::vector<HeapEntry> heapMap;
	D3D12_DESCRIPTOR_HEAP_DESC m_desc;
	UINT m_incrementSize;

protected:
	ID3D12Device* m_pDevice;
};
