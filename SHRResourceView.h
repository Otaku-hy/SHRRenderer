#pragma once

#include "d3dx12.h"
#include "SHRUtils.h"
#include "SHRHeapSlotAllocator.h"
#include "SHRRenderContext.h"

enum class SHRResourceViewType : uint8_t
{
	CBV,
	SRV,
	UAV,
	RTV,
	DSV,
	Sampler,
	VBV,
	IBV
};

class SHRResourceView
{
public:
	SHRResourceView(SHRRenderContext& renderContext, SHRResourceViewType type, SHRResource* pResouce);
	virtual ~SHRResourceView();

	D3D12_CPU_DESCRIPTOR_HANDLE GetViewHandle() { return m_viewLocation.slotHandle; }

private:
	void Destory();

public:
	SHRResourceViewType m_type;
	SHRHeapSlotAllocator::HeapSlot m_viewLocation;
	SHRHeapSlotAllocator* m_heapSlotAllocator;
	SHRD3D12Resource* m_pResource;
};

class SHRConstantBufferView : public SHRResourceView
{
public:
	SHRConstantBufferView(SHRRenderContext& renderContext, D3D12_CONSTANT_BUFFER_VIEW_DESC& desc, SHRResource* pResource);
	virtual ~SHRConstantBufferView() = default;

protected:
	void CreateConstantBufferView(SHRRenderContext& renderContext, D3D12_CONSTANT_BUFFER_VIEW_DESC& desc);
};

class SHRShaderResourceView : public SHRResourceView
{
public:
	SHRShaderResourceView(SHRRenderContext& renderContext, D3D12_SHADER_RESOURCE_VIEW_DESC& desc, SHRResource* pResource);
	virtual ~SHRShaderResourceView() = default;

protected:
	void CreateShaderResourceView(SHRRenderContext& renderContext, D3D12_SHADER_RESOURCE_VIEW_DESC& desc, SHRResource* pResource);
};

class SHRUnorderedAccessView : public SHRResourceView
{
public:
	SHRUnorderedAccessView(SHRRenderContext& renderContext, D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, SHRResource* pResource);
	virtual ~SHRUnorderedAccessView() = default;

protected:
	void CreateShaderResourceView(SHRRenderContext& renderContext, D3D12_UNORDERED_ACCESS_VIEW_DESC& desc, SHRResource* pResource);
};

class SHRRenderTargetView : public SHRResourceView
{
public:
	SHRRenderTargetView(SHRRenderContext& renderContext, D3D12_RENDER_TARGET_VIEW_DESC& desc, SHRResource* pResource);
	SHRRenderTargetView(SHRRenderContext& renderContext, SHRResource* pResource);
	virtual ~SHRRenderTargetView() = default;

protected:
	void CreateRenderTargetView(SHRRenderContext& renderContext, D3D12_RENDER_TARGET_VIEW_DESC& desc, SHRResource* pResource);
};

class SHRDepthStencilView : public SHRResourceView
{
public:
	SHRDepthStencilView(SHRRenderContext& renderContext, D3D12_DEPTH_STENCIL_VIEW_DESC& desc, SHRResource* pResource);
	virtual ~SHRDepthStencilView() = default;

protected:
	void CreateDepthStencilView(SHRRenderContext& renderContext, D3D12_DEPTH_STENCIL_VIEW_DESC& desc, SHRResource* pResource);
};

class SHRSampler : public SHRResourceView
{
public:
	SHRSampler(SHRRenderContext& renderContext, D3D12_SAMPLER_DESC& desc);
	virtual ~SHRSampler() = default;

protected:
	void CreateSampler(SHRRenderContext& renderContext, D3D12_SAMPLER_DESC& desc);
};

class SHRVertexBufferView : public SHRResourceView
{
public:
	SHRVertexBufferView(SHRRenderContext& renderContext, UINT vertexSize, UINT totalSize, SHRResource* pResource);
	virtual ~SHRVertexBufferView() = default;

	D3D12_VERTEX_BUFFER_VIEW& GetVertexBufferView() { return m_vertexBufferView; }

protected:
	void CreateVertexBufferView(SHRResource* pResource, UINT vertexSize, UINT totalSize);

private:
	D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;
};

class SHRIndexBufferView : public SHRResourceView
{
public:
	SHRIndexBufferView(SHRRenderContext& renderContext, UINT totalSize, SHRResource* pResource);
	virtual ~SHRIndexBufferView() = default;

	D3D12_INDEX_BUFFER_VIEW& GetIndexBufferView() { return m_indexBufferView; }

protected:
	void CreateIndexBufferView(SHRResource* pResource, UINT totalSize);

private:
	D3D12_INDEX_BUFFER_VIEW m_indexBufferView;
};
