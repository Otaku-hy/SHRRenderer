#include "SHRRenderContext.h"
#include "Win32Application.h"
#include "SHRRenderEngine.h"

void GetHardwareAdaptor(IDXGIFactory1* pFactory, IDXGIAdapter1** ppAdapter)
{
	using Microsoft::WRL::ComPtr;

	*ppAdapter = nullptr;

	ComPtr<IDXGIAdapter1> adapter;
	ComPtr<IDXGIFactory6> pFactory6;
	if (SUCCEEDED(pFactory->QueryInterface(IID_PPV_ARGS(&pFactory6))))
	{
		for (uint32_t adapterIndex = 0;
			SUCCEEDED(pFactory6->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)));
			++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	if (adapter.Get() == nullptr)
	{
		for (uint32_t adapterIndex = 0; SUCCEEDED(pFactory->EnumAdapters1(adapterIndex, &adapter)); ++adapterIndex)
		{
			DXGI_ADAPTER_DESC1 desc;
			adapter->GetDesc1(&desc);

			if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}

			if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, _uuidof(ID3D12Device), nullptr)))
			{
				break;
			}
		}
	}

	*ppAdapter = adapter.Detach();
}

void SHRDevice::InitializeDevice(SHRRenderEngine* pEngine)
{
	using Microsoft::WRL::ComPtr;

	UINT dxgiFactoryFlag = 0;
#if defined(_DEBUG)
	{
		ComPtr<ID3D12Debug> pDebugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&pDebugController))))
		{
			pDebugController->EnableDebugLayer();
			dxgiFactoryFlag |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	//as dxgi factory is only used to create device (adapter & display), we dont put it as a member of SHR device
	ComPtr<IDXGIFactory4> pFactory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlag, IID_PPV_ARGS(&pFactory)));

	ComPtr<IDXGIAdapter1> pAdapter;
	GetHardwareAdaptor(pFactory.Get(), &pAdapter);
	ThrowIfFailed(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&m_pD3dDevice)));

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(m_pD3dDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_pCommandQueue)));

	ComPtr<IDXGISwapChain1> pSwapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = SHRRenderEngine::GetFrameCount();
	swapChainDesc.Width = pEngine->GetWidth();
	swapChainDesc.Height = pEngine->GetHeight();
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;
	pFactory->CreateSwapChainForHwnd(m_pCommandQueue.Get(), Win32Application::GetWindowHandle(), &swapChainDesc, nullptr, nullptr, &pSwapChain);

	ThrowIfFailed(pFactory->MakeWindowAssociation(Win32Application::GetWindowHandle(), DXGI_MWA_NO_ALT_ENTER));
	ThrowIfFailed(pSwapChain.As(&m_pSwapChain));

	m_fenceValue = 0;
	ThrowIfFailed(m_pD3dDevice->CreateFence(m_fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
	m_fenceValue++;
}

SHRHeapSlotAllocator* SHRRenderContext::GetHeapSlotManager(D3D12_DESCRIPTOR_HEAP_TYPE type)
{
	switch (type)
	{
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
		return m_pCBVSRVUAVHeapSlotManager.get();
		break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
		return m_pSamplerHeapSlotManager.get();
		break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:
		return m_pRTVHeapSlotManager.get();
		break;
	case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:
		return m_pDSVHeapSlotManager.get();
		break;
	default:
		return nullptr;
		break;
	}
}

SHRRenderContext::SHRRenderContext(SHRRenderEngine* pEngine)
{
	m_pDevice = std::make_unique<SHRDevice>();
	m_pDevice->InitializeDevice(pEngine);

	ID3D12Device* pD3dDevice = m_pDevice->m_pD3dDevice.Get();

	ThrowIfFailed(pD3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator)));
	ThrowIfFailed(pD3dDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, m_pCommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pCommandList)));
	ThrowIfFailed(m_pCommandList->Close());

	m_pRTVHeapSlotManager = std::make_unique<SHRHeapSlotAllocator>(pD3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, SHR_DEFAULT_DESCRIPTOR_HEAP_SIZE);
	m_pDSVHeapSlotManager = std::make_unique<SHRHeapSlotAllocator>(pD3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, SHR_DEFAULT_DESCRIPTOR_HEAP_SIZE);
	m_pCBVSRVUAVHeapSlotManager = std::make_unique<SHRHeapSlotAllocator>(pD3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, SHR_DEFAULT_DESCRIPTOR_HEAP_SIZE);
	m_pSamplerHeapSlotManager = std::make_unique<SHRHeapSlotAllocator>(pD3dDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, SHR_DEFAULT_DESCRIPTOR_HEAP_SIZE);

	m_pGPUDescriptorCache = std::make_unique<SHRDescriptorCache>(pD3dDevice);

	m_pTextureAllocateSystem = std::make_unique<SHRSegregatedListSystem>(pD3dDevice);
	m_pBufferAllocateSystem = std::make_unique<SHRBuddySystem>(pD3dDevice);
}

void SHRRenderContext::CleanupContext()
{
	m_pBufferAllocateSystem->CleanupSystem();
	m_pTextureAllocateSystem->CleanupSystem();

	m_pGPUDescriptorCache->ClearCache();
}