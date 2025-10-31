#pragma once

#include <vector>

#include "d3dx12.h"

#include "SHRUtils.h"
#include "SHRResource.h"

#define SHR_BUDDY_HEAP_DEFAULT_MAX_SIZE 8192 * 1024  //KB = 8MB
#define SHR_BUDDY_HEAP_DEFAULT_BLOCK_SIZE D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT * 4 //32KB
#define SHR_BUDDY_HEAP_DEFAULT_BLOCK_COUNT 8

#define SHR_SEGREGATED_HEAP_DEFAULT_BLOCK_COUNT 4
#define SHR_SEGREGATED_HEAP_DEFAULT_LAYER_NUM 4
//#define SHR_SEGREGATED_HEAP_DEFAULT_MIN_BLOCK_SIZE 32

enum class SHRAllocatorType : uint8_t
{
	Buddy,
	Segregated
};

struct SHRBuddyAllocatorParams
{
	UINT64 heapMaxSize;
};

struct SHRSFLAllocatorParams
{
	UINT64 heapLayerNum;
};

struct SHRAllocatorDesc
{
	D3D12_HEAP_FLAGS flags;
	D3D12_HEAP_PROPERTIES properties;
	UINT64 alignment;
	SHRAllocatorType type;
	UINT64 heapBlockSize;
	union
	{
		SHRBuddyAllocatorParams buddyParams;
		SHRSFLAllocatorParams sflParams;
	};
};


class SHRResourceAllocator
{
public:
	SHRResourceAllocator(ID3D12Device* pDevice, const SHRAllocatorDesc& initData);
	virtual ~SHRResourceAllocator() = default;

	virtual void Initialize(ID3D12Device* pDevice, const SHRAllocatorDesc& initData);

	virtual bool AllocateSHRResouce(const D3D12_RESOURCE_DESC& desc,
		const D3D12_RESOURCE_STATES& initState,
		UINT size, SHRResource& resource,
		const D3D12_CLEAR_VALUE* clrValue = nullptr) = 0;

	virtual void DeallocateSHRResource(SHRResource::ResourceBlock& block) = 0;
	virtual void DeallocateImmediate(SHRResource::ResourceBlock& block) = 0;
	virtual void CleanupHeap() = 0;

	virtual std::pair<INT, UINT> CanAllocate(UINT64 size) = 0;
	virtual void AllocateBlock(UINT layer, UINT offset, SHRResource& resource) = 0;

	virtual void DeallocateBlock(UINT64 layer, UINT64 offset) = 0;
	virtual UINT64 GetAllocateSize(UINT64 size, UINT64 alignment) = 0;
	virtual UINT64 GetRealAllocatedLocation(UINT64 size, UINT64 alignment, UINT64 layer, UINT64 offset) = 0;

	SHRAllocatorDesc GetDesc();

public:
	struct BlockState
	{
		UINT valid;
		SHRResource* pResource;
	};

	std::vector<SHRResource::ResourceBlock> m_defferedDeletionList;
	SHRAllocatorDesc m_allocatorDesc;

protected:
	ID3D12Device* m_pDevice = nullptr;
};

class SHRBuddyAllocator : public SHRResourceAllocator
{
public:
	SHRBuddyAllocator() = default;
	~SHRBuddyAllocator() = default;

	SHRBuddyAllocator(ID3D12Device* pDevice, const SHRAllocatorDesc& initData);

	void Initialize(ID3D12Device* pDevice, const SHRAllocatorDesc& initData);

	bool AllocateSHRResouce(const D3D12_RESOURCE_DESC& desc,
		const D3D12_RESOURCE_STATES& initState,
		UINT size, SHRResource& resource,
		const D3D12_CLEAR_VALUE* clrValue = nullptr);

	void DeallocateSHRResource(SHRResource::ResourceBlock& block);
	void DeallocateImmediate(SHRResource::ResourceBlock& block);
	void CleanupHeap();

	std::pair<INT, UINT> CanAllocate(UINT64 size);					//return {layer, offset}
	void AllocateBlock(UINT layer, UINT offset, SHRResource& resource);					//return offset

	void DeallocateBlock(UINT64 layer, UINT64 offset);
	UINT64 GetAllocateSize(UINT64 size, UINT64 alignment);
	UINT64 GetRealAllocatedLocation(UINT64 size, UINT64 alignment, UINT64 layer, UINT64 offset);

public:
	std::vector<BlockState> m_blockStates;
	Microsoft::WRL::ComPtr<ID3D12Heap> m_pHeap = nullptr;
};

class SHRSegregatedAllocator : public SHRResourceAllocator
{
public:
	SHRSegregatedAllocator() = default;
	SHRSegregatedAllocator(const SHRSegregatedAllocator& other) = delete;
	SHRSegregatedAllocator(SHRSegregatedAllocator&& other) = default;
	SHRSegregatedAllocator& operator=(const SHRSegregatedAllocator& other) = delete;
	SHRSegregatedAllocator& operator=(SHRSegregatedAllocator&& other) = default;
	~SHRSegregatedAllocator();

	SHRSegregatedAllocator(ID3D12Device* pDevice, const SHRAllocatorDesc& initData);

	void Initialize(ID3D12Device* pDevice, const SHRAllocatorDesc& initData);

	bool AllocateSHRResouce(const D3D12_RESOURCE_DESC& desc,
		const D3D12_RESOURCE_STATES& initState,
		UINT size, SHRResource& resource,
		const D3D12_CLEAR_VALUE* clrValue = nullptr);

	void DeallocateSHRResource(SHRResource::ResourceBlock& block);
	void DeallocateImmediate(SHRResource::ResourceBlock& block);
	void CleanupHeap();

	std::pair<INT, UINT> CanAllocate(UINT64 size);
	void AllocateBlock(UINT layer, UINT offset, SHRResource& resource);

	void DeallocateBlock(UINT64 layer, UINT64 offset);
	UINT64 GetAllocateSize(UINT64 size, UINT64 alignment);
	UINT64 GetRealAllocatedLocation(UINT64 size, UINT64 alignment, UINT64 layer, UINT64 offset);

private:
	void CreateHeap(UINT layer);

public:
	std::vector<Microsoft::WRL::ComPtr<ID3D12Heap>> m_pHeaps;
	std::vector<BlockState>* m_pBlockStates;

private:

};

class SHRBuddySystem
{
public:
	SHRBuddySystem(ID3D12Device* pDevice);
	void Initialize(ID3D12Device* pDevice);

	bool AllocateSHRResouce(D3D12_HEAP_TYPE requiredType, const D3D12_RESOURCE_DESC& desc,
		const D3D12_RESOURCE_STATES& initState,
		UINT size, SHRResource& resource,
		const D3D12_CLEAR_VALUE* clrValue = nullptr);

	void CleanupSystem();

public:
	std::vector<SHRBuddyAllocator> m_allocators;

private:
	ID3D12Device* m_pDevice = nullptr;
};

class SHRSegregatedListSystem
{
public:
	SHRSegregatedListSystem(ID3D12Device* pDevice);
	void Initialize(ID3D12Device* pDevice);

	bool AllocateSHRResouce(D3D12_HEAP_TYPE requiredType, const D3D12_RESOURCE_DESC& desc,
		const D3D12_RESOURCE_STATES& initState,
		UINT size, SHRResource& resource,
		const D3D12_CLEAR_VALUE* clrValue = nullptr);

	void CleanupSystem();
public:
	std::vector<SHRSegregatedAllocator> m_allocators;

private:
	ID3D12Device* m_pDevice = nullptr;
};

//class SHRMemorySystem
//{
//public:
//
//
//public:
//	std::unique_ptr<SHRBuddySystem> m_pBuddySystem;
//	std::unique_ptr<SHRSegregatedListSystem> m_pSegregatedListSystem;
//
//private:
//	ID3D12Device* m_pDevice = nullptr;
//};