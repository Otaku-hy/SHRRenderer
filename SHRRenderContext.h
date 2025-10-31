#pragma once

#include <dxgi1_6.h>

#include "d3dx12.h"
#include "SHRUtils.h"
#include "SHRHeapSlotAllocator.h"
#include "SHRDescriptorCache.h"
#include "SHRResourceAllocator.h"

class SHRRenderEngine;

struct SHRDevice
{
	Microsoft::WRL::ComPtr<IDXGISwapChain3> m_pSwapChain;
	Microsoft::WRL::ComPtr<ID3D12Device> m_pD3dDevice;
	Microsoft::WRL::ComPtr<ID3D12Fence> m_pFence;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	uint64_t m_fenceValue;

	void InitializeDevice(SHRRenderEngine* pEngine);
};

class SHRRenderContext
{
public:
	SHRRenderContext(SHRRenderEngine* pEngine);
	~SHRRenderContext() = default;

	ID3D12Device* GetDevice() { return m_pDevice->m_pD3dDevice.Get(); }
	SHRHeapSlotAllocator* GetHeapSlotManager(D3D12_DESCRIPTOR_HEAP_TYPE type);
	ID3D12GraphicsCommandList* GetCmdList() { return m_pCommandList.Get(); }
	ID3D12CommandAllocator* GetCmdAllocator() { return m_pCommandAllocator.Get(); }
	SHRDescriptorCache* GetDescriptorCache() { return m_pGPUDescriptorCache.get(); }
	ID3D12CommandQueue* GetCmdQueue() { return m_pDevice->m_pCommandQueue.Get(); }
	ID3D12Fence* GetFence() { return m_pDevice->m_pFence.Get(); }
	IDXGISwapChain3* GetSwapChain() { return m_pDevice->m_pSwapChain.Get(); };

	SHRSegregatedListSystem* GetTextureAllocator() { return m_pTextureAllocateSystem.get(); }
	SHRBuddySystem* GetBufferAllocator() { return m_pBufferAllocateSystem.get(); }

	UINT GetCurrentBackBufferIndex() { return m_pDevice->m_pSwapChain->GetCurrentBackBufferIndex(); }
	uint64_t& GetGPUFenceValue() { return m_pDevice->m_fenceValue; }
	void Present() { ThrowIfFailed(m_pDevice->m_pSwapChain->Present(1, 0)); }

	void CleanupContext();

private:
	std::unique_ptr<SHRDevice> m_pDevice;

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pCommandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommandList;

	std::unique_ptr<SHRSegregatedListSystem> m_pTextureAllocateSystem;
	std::unique_ptr<SHRBuddySystem> m_pBufferAllocateSystem;

	std::unique_ptr<SHRHeapSlotAllocator> m_pRTVHeapSlotManager;
	std::unique_ptr<SHRHeapSlotAllocator> m_pDSVHeapSlotManager;
	std::unique_ptr<SHRHeapSlotAllocator> m_pCBVSRVUAVHeapSlotManager;
	std::unique_ptr<SHRHeapSlotAllocator> m_pSamplerHeapSlotManager;

	std::unique_ptr<SHRDescriptorCache> m_pGPUDescriptorCache;
};

