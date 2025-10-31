#include "SHRResourceView.h"

SHRResourceView::SHRResourceView(SHRRenderContext& renderContext, SHRResourceViewType type, SHRResource* pResource) : m_type(type), m_pResource(pResource->m_pSHRD3dResource.get())
{
	if (type == SHRResourceViewType::IBV || type == SHRResourceViewType::VBV)
	{
		m_heapSlotAllocator = nullptr;
		return;
	}

	D3D12_DESCRIPTOR_HEAP_TYPE heapType;
	if (type == SHRResourceViewType::CBV || type == SHRResourceViewType::SRV || type == SHRResourceViewType::UAV)
		heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	else if (type == SHRResourceViewType::RTV)
		heapType = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	else if (type == SHRResourceViewType::DSV)
		heapType = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	else if (type == SHRResourceViewType::Sampler)
		heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	m_heapSlotAllocator = renderContext.GetHeapSlotManager(heapType);
	m_viewLocation = m_heapSlotAllocator->AllocateSlot();
}

SHRResourceView::~SHRResourceView()
{
	if (m_heapSlotAllocator)
		Destory();
}

void SHRResourceView::Destory()
{
	m_heapSlotAllocator->DeallocateSlot(m_viewLocation);
}

SHRConstantBufferView::SHRConstantBufferView(SHRRenderContext& renderContext, D3D12_CONSTANT_BUFFER_VIEW_DESC& desc, SHRResource* pResouce) : SHRResourceView(renderContext, SHRResourceViewType::CBV, pResouce)
{
	CreateConstantBufferView(renderContext, desc);
}

void SHRConstantBufferView::CreateConstantBufferView(SHRRenderContext& renderContext, D3D12_CONSTANT_BUFFER_VIEW_DESC& desc)
{
	renderContext.GetDevice()->CreateConstantBufferView(&desc, m_viewLocation.slotHandle);
}

SHRShaderResourceView::SHRShaderResourceView(SHRRenderContext& renderContext, D3D12_SHADER_RESOURCE_VIEW_DESC& desc, SHRResource* pResource) : SHRResourceView(renderContext, SHRResourceViewType::SRV, pResource)
{
	CreateShaderResourceView(renderContext, desc, pResource);
}

void SHRShaderResourceView::CreateShaderResourceView(SHRRenderContext& renderContext, D3D12_SHADER_RESOURCE_VIEW_DESC& desc, SHRResource* pResource)
{
	renderContext.GetDevice()->CreateShaderResourceView(m_pResource->m_pResource.Get(), &desc, m_viewLocation.slotHandle);
}
SHRUnorderedAccessView::SHRUnorderedAccessView(SHRRenderContext& renderContext, D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, SHRResource* pResource) : SHRResourceView(renderContext, SHRResourceViewType::UAV, pResource)
{
	CreateShaderResourceView(renderContext, desc, pResource);
}

void SHRUnorderedAccessView::CreateShaderResourceView(SHRRenderContext& renderContext, D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, SHRResource* pResource)
{
	renderContext.GetDevice()->CreateUnorderedAccessView(m_pResource->m_pResource.Get(), nullptr, &desc, m_viewLocation.slotHandle);
}

SHRRenderTargetView::SHRRenderTargetView(SHRRenderContext& renderContext, D3D12_RENDER_TARGET_VIEW_DESC& desc, SHRResource* pResource) : SHRResourceView(renderContext, SHRResourceViewType::RTV, pResource)
{
	CreateRenderTargetView(renderContext, desc, pResource);
}

SHRRenderTargetView::SHRRenderTargetView(SHRRenderContext& renderContext, SHRResource* pResource) : SHRResourceView(renderContext, SHRResourceViewType::RTV, pResource)
{
	renderContext.GetDevice()->CreateRenderTargetView(m_pResource->m_pResource.Get(), nullptr, m_viewLocation.slotHandle);
}

void SHRRenderTargetView::CreateRenderTargetView(SHRRenderContext& renderContext, D3D12_RENDER_TARGET_VIEW_DESC& desc, SHRResource* pResource)
{
	renderContext.GetDevice()->CreateRenderTargetView(m_pResource->m_pResource.Get(), &desc, m_viewLocation.slotHandle);
}

SHRDepthStencilView::SHRDepthStencilView(SHRRenderContext& renderContext, D3D12_DEPTH_STENCIL_VIEW_DESC& desc, SHRResource* pResource) : SHRResourceView(renderContext, SHRResourceViewType::DSV, pResource)
{
	CreateDepthStencilView(renderContext, desc, pResource);
}

void SHRDepthStencilView::CreateDepthStencilView(SHRRenderContext& renderContext, D3D12_DEPTH_STENCIL_VIEW_DESC& desc, SHRResource* pResource)
{
	renderContext.GetDevice()->CreateDepthStencilView(m_pResource->m_pResource.Get(), &desc, m_viewLocation.slotHandle);
}

SHRSampler::SHRSampler(SHRRenderContext& renderContext, D3D12_SAMPLER_DESC& desc) : SHRResourceView(renderContext, SHRResourceViewType::Sampler, nullptr)
{
	CreateSampler(renderContext, desc);
}

void SHRSampler::CreateSampler(SHRRenderContext& renderContext, D3D12_SAMPLER_DESC& desc)
{
	renderContext.GetDevice()->CreateSampler(&desc, m_viewLocation.slotHandle);
}

SHRVertexBufferView::SHRVertexBufferView(SHRRenderContext& renderContext, UINT vertexSize, UINT totalSize, SHRResource* pResource) : SHRResourceView(renderContext, SHRResourceViewType::VBV, pResource)
{
	CreateVertexBufferView(pResource, vertexSize, totalSize);
}

void SHRVertexBufferView::CreateVertexBufferView(SHRResource* pResource, UINT vertexSize, UINT totalSize)
{
	m_vertexBufferView.BufferLocation = m_pResource->m_pResource->GetGPUVirtualAddress();
	m_vertexBufferView.StrideInBytes = vertexSize;
	m_vertexBufferView.SizeInBytes = totalSize;
}

SHRIndexBufferView::SHRIndexBufferView(SHRRenderContext& renderContext, UINT totalSize, SHRResource* pResource) : SHRResourceView(renderContext, SHRResourceViewType::IBV, pResource)
{
	CreateIndexBufferView(pResource, totalSize);
}

void SHRIndexBufferView::CreateIndexBufferView(SHRResource* pResource, UINT totalSize)
{
	m_indexBufferView.BufferLocation = m_pResource->m_pResource->GetGPUVirtualAddress();
	m_indexBufferView.SizeInBytes = totalSize;
	m_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
}