#pragma once

#include "SHRShader.h"

struct SHRShaderResouceCache
{
public:
	struct ResourceView
	{
		SHRResourceViewType type;
		D3D12_CPU_DESCRIPTOR_HANDLE viewHandle;
		SHRD3D12Resource* pResouceObj;
	};

	struct RootTable
	{
		UINT offsetFromHeapStart = -1;
		UINT numResouce = 0;
		ResourceView* pResouce = nullptr;

		ResourceView& GetResouce(UINT offsetFromTableBegin) { return pResouce[offsetFromTableBegin]; };
		const ResourceView& GetResouce(UINT offsetFromTableBegin) const { return pResouce[offsetFromTableBegin]; };
	};

public:
	RootTable& GetRootTable(UINT rootParamIndex) { return cachedRootTable[rootParamIndex]; };
	const RootTable& GetRootTable(UINT rootParamIndex) const { return cachedRootTable[rootParamIndex]; };

public:
	//table & rootview need cache slot
	UINT numTable;
	std::vector<RootTable> cachedRootTable;
	std::vector<ResourceView> cachedResouce;
};

class SHRShaderResouceBinding
{
public:
	SHRShaderResouceBinding(const std::vector<SHRShaderResoureLayout>& resourceLayout);

	void SetResouce(const std::string& name, const std::vector<SHRShaderResourceView>& resouce);

	void CommitResouce(SHRDescriptorCache* GPUDescriptorCache);

public:
	const std::vector<SHRShaderResoureLayout>& m_resourceLayout;
	SHRShaderResouceCache m_resoureCache;
};

