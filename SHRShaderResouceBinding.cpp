#include "SHRShaderResouceBinding.h"

SHRShaderResouceBinding::SHRShaderResouceBinding(const std::vector<SHRShaderResoureLayout>& resourceLayout) : m_resourceLayout(resourceLayout)
{
	UINT paramCount = 0;
	for (size_t i = 0; i < m_resourceLayout.size(); i++)
	{
		paramCount += m_resourceLayout[i].m_rootTableCount + m_resourceLayout[i].m_rootViewCount;
	}

	std::vector<UINT> numResouces(paramCount, 0);
	for (size_t i = 0; i < m_resourceLayout.size(); i++)
	{
		const SHRShaderResoureLayout& layout = m_resourceLayout[i];
		for (const SHRShaderResoureLayout::ElementLayout& element : layout.m_layoutArray)
		{
			numResouces[element.rootParamIndex] += element.pResouceReflection->bindCount;		
		}
	}

	UINT totalResouce = 0;
	for (auto count : numResouces) totalResouce += count;

	m_resoureCache.cachedResouce.resize(totalResouce);
	SHRShaderResouceCache::ResourceView* pResource = &m_resoureCache.cachedResouce.front();

	m_resoureCache.cachedRootTable.resize(paramCount);
	for (size_t i = 0; i < paramCount; i++)
	{
		m_resoureCache.cachedRootTable[i].numResouce = numResouces[i];
		m_resoureCache.cachedRootTable[i].pResouce = pResource;
		pResource += numResouces[i];
	}
}

void SHRShaderResouceBinding::SetResouce(const std::string& name, const std::vector<SHRShaderResourceView>& resouce)
{
	for (const SHRShaderResoureLayout& layout : m_resourceLayout)
	{
		SHRShaderResoureLayout::ElementLayout element = layout.GetElementLayout(name);
		if (element.rootParamIndex == -1) continue;

		for (size_t arrayIndex = 0; arrayIndex < resouce.size(); arrayIndex++)
		{
			SHRShaderResouceCache::ResourceView& dstResouce = m_resoureCache.GetRootTable(element.rootParamIndex).GetResouce(element.offsetFromTableStart + arrayIndex);

			if (element.offsetFromTableStart != -1)
				dstResouce.viewHandle.ptr = 0;
			else
				dstResouce.viewHandle = resouce[arrayIndex].m_viewLocation.slotHandle;
			dstResouce.type = element.pResouceReflection->type;
			dstResouce.pResouceObj = resouce[arrayIndex].m_pResource;
		}
	}
}

void SHRShaderResouceBinding::CommitResouce(SHRDescriptorCache* GPUDescriptorCache)
{
	for (SHRShaderResouceCache::RootTable& rootTable : m_resoureCache.cachedRootTable)
	{
		SHRShaderResouceCache::ResourceView & resouce= rootTable.GetResouce(0);
		if (resouce.viewHandle.ptr == 0) continue;
		
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles(rootTable.numResouce);
		for (size_t i = 0; i < rootTable.numResouce; i++)
		{
			handles[i] = rootTable.GetResouce(i).viewHandle;
		}

		if (resouce.type != SHRResourceViewType::Sampler)
		{
			auto [offset, gpuHandle] = GPUDescriptorCache->CacheCbvSrvUavDescriptor(handles);
			rootTable.offsetFromHeapStart = offset;
		}
		else
		{
			auto [offset, gpuHandle] = GPUDescriptorCache->CacheSamplerDescriptor(handles);
			rootTable.offsetFromHeapStart = offset;
		}
	}
}
