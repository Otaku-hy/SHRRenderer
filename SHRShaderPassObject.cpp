#include "SHRShaderPassObject.h"

template <typename T>
void hash_combine(size_t& seed, const T& value) {
	std::hash<T> hasher;
	seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

void hash_memory_block(size_t& seed, const void* data, size_t size) {
	const char* bytes = static_cast<const char*>(data);
	for (size_t i = 0; i < size; ++i) {
		hash_combine(seed, bytes[i]);
	}
}

namespace std
{
	template <>
	struct hash<SHRShaderPassDesc>
	{
		size_t operator()(const SHRShaderPassDesc& key) const
		{
			size_t seed = 0;
			hash_memory_block(seed, &key.blendDesc, sizeof(D3D12_BLEND_DESC));
			hash_memory_block(seed, &key.rasterizerDesc, sizeof(D3D12_RASTERIZER_DESC));
			hash_memory_block(seed, &key.depthStencilDesc, sizeof(D3D12_DEPTH_STENCIL_DESC));

			hash_combine(seed, key.primitiveType);

			hash_combine(seed, key.numRenderTargets);
			for (UINT i = 0; i < key.numRenderTargets; ++i) {
				hash_combine(seed, key.rtvFormats[i]);
			}
			hash_combine(seed, key.dsvFormat);

			for (size_t i = 0; i < static_cast<uint8_t>(SHRShaderType::NumShaderTypes); ++i) 
			{
				if (key.pShaders[i])
				{
					hash_memory_block(seed, key.pShaders[i]->shaderHash, 16 * sizeof(BYTE));
				}	
			}

			return seed;
		}
	};
}

std::unordered_map<SHRShaderPassDesc, std::unique_ptr<SHRShaderPassObject>> g_passObjectCache;

D3D12_SHADER_VISIBILITY GetShaderVisibility(SHRShaderType type)
{
	switch (type)
	{
	case SHRShaderType::Vertex:
		return D3D12_SHADER_VISIBILITY_VERTEX;
	case SHRShaderType::Pixel:
		return D3D12_SHADER_VISIBILITY_PIXEL;
	case SHRShaderType::Geometry:
		return D3D12_SHADER_VISIBILITY_GEOMETRY;
	case SHRShaderType::Hull:
		return D3D12_SHADER_VISIBILITY_HULL;
	case SHRShaderType::Domain:
		return D3D12_SHADER_VISIBILITY_DOMAIN;
	case SHRShaderType::Compute:
		return D3D12_SHADER_VISIBILITY_ALL;
	default:
		return D3D12_SHADER_VISIBILITY_ALL;
	}
}

D3D12_DESCRIPTOR_RANGE_TYPE GetDescriptorRangeType(SHRResourceViewType type)
{
	switch (type)
	{
	case SHRResourceViewType::CBV:
		return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	case SHRResourceViewType::SRV:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	case SHRResourceViewType::UAV:
		return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	case SHRResourceViewType::Sampler:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
	default:
		return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	}
}

SHRRootSignatureManager::SHRRootSignatureManager() : m_cbvSrvUavRootIndexMap(static_cast<uint8_t>(SHRShaderType::NumShaderTypes), -1), m_samplerRootIndexMap(static_cast<uint8_t>(SHRShaderType::NumShaderTypes), -1)
{
}

Microsoft::WRL::ComPtr<ID3D12RootSignature> SHRRootSignatureManager::CreateRootSignature(ID3D12Device* pDevice)
{
	Microsoft::WRL::ComPtr<ID3D12RootSignature> pRootSignature;

	Microsoft::WRL::ComPtr<ID3DBlob> pSignature;
	Microsoft::WRL::ComPtr<ID3DBlob> pError;

	std::vector<D3D12_ROOT_PARAMETER1> params(m_rootTables.size() + m_rootViews.size());
	auto FillParamStruct = [&params](const std::vector<RootParameter>& wrapParams)
	{
		for (size_t i = 0; i < wrapParams.size(); i++)
		{
			params[wrapParams[i].GetRootIndex()] = wrapParams[i];
		}
	};
	FillParamStruct(m_rootTables);
	FillParamStruct(m_rootViews);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(params.size(), params.data(), 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1_1, &pSignature, &pError));
	ThrowIfFailed(pDevice->CreateRootSignature(0, pSignature->GetBufferPointer(), pSignature->GetBufferSize(), IID_PPV_ARGS(&pRootSignature)));

	return pRootSignature;
}

void SHRRootSignatureManager::AllocateRootParameterSlot(SHRShaderType type, const SHRShader::ShaderResourceReflection& reflection, UINT& rootParamIndex, UINT& offsetFromTableStart)
{
	D3D12_SHADER_VISIBILITY visibility = GetShaderVisibility(type);
	UINT key = static_cast<UINT>(type);

	rootParamIndex = m_rootTables.size() + m_rootViews.size();
	offsetFromTableStart = -1;

	INT& rootIndex = (reflection.type == SHRResourceViewType::Sampler ? m_samplerRootIndexMap : m_cbvSrvUavRootIndexMap)[key];
	if (rootIndex == -1)
	{
		rootIndex = AddDescriptorTable(rootParamIndex, visibility);
	}

	RootParameter& rootParameter = m_rootTables[rootIndex];
	D3D12_DESCRIPTOR_RANGE_TYPE rangeType = GetDescriptorRangeType(reflection.type);

	offsetFromTableStart = rootParameter.GetDescriptorOffset();
	rootParameter.AddDescriptorRange(rangeType, reflection.bindCount, reflection.bindPoint, reflection.space, D3D12_DESCRIPTOR_RANGE_FLAG_NONE, offsetFromTableStart);
}

UINT SHRRootSignatureManager::AddRootDescriptor(UINT rootParamIndex, D3D12_ROOT_PARAMETER_TYPE type,
	UINT shaderRegister, UINT registerSpace,
	D3D12_SHADER_VISIBILITY visibility)
{
	RootParameter rootParameter;
	rootParameter.ParameterType = type;
	rootParameter.Descriptor.ShaderRegister = shaderRegister;
	rootParameter.Descriptor.RegisterSpace = registerSpace;
	rootParameter.ShaderVisibility = visibility;
	rootParameter.rootParamIndex = rootParamIndex;

	m_rootViews.push_back(rootParameter);

	return m_rootViews.size() - 1;
}

UINT SHRRootSignatureManager::AddDescriptorTable(UINT rootParamIndex, D3D12_SHADER_VISIBILITY visibility)
{
	RootParameter rootParameter;
	rootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameter.DescriptorTable.NumDescriptorRanges = 0;
	rootParameter.DescriptorTable.pDescriptorRanges = nullptr;
	rootParameter.ShaderVisibility = visibility;
	rootParameter.rootParamIndex = rootParamIndex;

	m_rootTables.push_back(rootParameter);

	return m_rootTables.size() - 1;
}

SHRShaderPassObject* SHRShaderPassObject::GetShaderPassObject(SHRRenderContext& renderContext, const SHRShaderPassDesc& desc)
{
	auto it = g_passObjectCache.find(desc);
	if (it == g_passObjectCache.end())
	{
		g_passObjectCache[desc] = std::make_unique<SHRShaderPassObject>(renderContext, desc);
	}
	return g_passObjectCache[desc].get();
}

SHRShaderPassObject::SHRShaderPassObject(SHRRenderContext& renderContext, const SHRShaderPassDesc& desc) : m_renderContext(renderContext)
{
	for (size_t i = 0; i < static_cast<uint8_t>(SHRShaderType::NumShaderTypes); i++)
	{
		m_pPassShader.push_back(desc.pShaders[i]);
	}

	InitializeShaderResoureLayout();
	InitializeInputLayout();
	InitializePipelineState(desc);
}

void SHRShaderPassObject::InitializePipelineState(const SHRShaderPassDesc& desc)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.InputLayout = { m_inputLayout.data(), static_cast<UINT>(m_inputLayout.size()) };
	psoDesc.pRootSignature = m_pRootSignature.Get();
	psoDesc.RasterizerState = desc.rasterizerDesc;
	psoDesc.BlendState = desc.blendDesc;
	psoDesc.DepthStencilState = desc.depthStencilDesc;
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = desc.primitiveType;
	psoDesc.NumRenderTargets = desc.numRenderTargets;
	for (size_t i = 0; i < desc.numRenderTargets; i++)
	{
		psoDesc.RTVFormats[i] = desc.rtvFormats[i];
	}
	psoDesc.DSVFormat = desc.dsvFormat;

	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;

	auto GetShaderCode = [](const SHRShader* pShader) -> CD3DX12_SHADER_BYTECODE { return pShader ? CD3DX12_SHADER_BYTECODE(pShader->m_shaderBlob->GetBufferPointer(), pShader->m_shaderBlob->GetBufferSize()) : CD3DX12_SHADER_BYTECODE(); };

	psoDesc.VS = GetShaderCode(m_pPassShader[static_cast<size_t>(SHRShaderType::Vertex)]);
	psoDesc.PS = GetShaderCode(m_pPassShader[static_cast<size_t>(SHRShaderType::Pixel)]);
	psoDesc.GS = GetShaderCode(m_pPassShader[static_cast<size_t>(SHRShaderType::Geometry)]);
	psoDesc.HS = GetShaderCode(m_pPassShader[static_cast<size_t>(SHRShaderType::Hull)]);
	psoDesc.DS = GetShaderCode(m_pPassShader[static_cast<size_t>(SHRShaderType::Domain)]);

	ThrowIfFailed(m_renderContext.GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState)));
}

void SHRShaderPassObject::InitializeShaderResoureLayout()
{
	SHRRootSignatureManager rootSignatureManager;

	for (size_t i = 0; i < static_cast<size_t>(SHRShaderType::NumShaderTypes); i++)
	{
		const SHRShader* pShader = m_pPassShader[i];
		if (!pShader) continue;

		m_resourceLayouts.emplace_back();
		SHRShaderResoureLayout& resourceLayout = m_resourceLayouts.back();
		const std::vector<SHRShader::ShaderResourceReflection>& reflections = pShader->m_shaderResourceReflections;

		UINT rootTableCount = rootSignatureManager.m_rootTables.size();
		UINT rootViewCount = rootSignatureManager.m_rootViews.size();

		for (const SHRShader::ShaderResourceReflection& reflection : reflections)
		{
			UINT rootParamIndex, offsetFromTableStart;
			rootSignatureManager.AllocateRootParameterSlot(static_cast<SHRShaderType>(i), reflection, rootParamIndex, offsetFromTableStart);

			SHRShaderResoureLayout::ElementLayout layout;
			layout.offsetFromTableStart = offsetFromTableStart;
			layout.rootParamIndex = rootParamIndex;
			layout.pResouceReflection = &reflection;

			resourceLayout.AddElement(reflection.resourceName, layout);
		}

		resourceLayout.m_rootTableCount = rootSignatureManager.m_rootTables.size() - rootTableCount;
		resourceLayout.m_rootViewCount = rootSignatureManager.m_rootViews.size() - rootViewCount;
	}

	m_pRootSignature = rootSignatureManager.CreateRootSignature(m_renderContext.GetDevice());
}

void SHRShaderPassObject::InitializeInputLayout()
{
	const SHRShader* pVertexShader = m_pPassShader[static_cast<size_t>(SHRShaderType::Vertex)];
	if (!pVertexShader) return;

	UINT inputSlot = 0;
	for (const SHRShader::VSInputElement& inputElement : pVertexShader->m_vsInputElements)
	{
		D3D12_INPUT_ELEMENT_DESC inputElementDesc;

		inputElementDesc.SemanticName = inputElement.semanticName.c_str();
		inputElementDesc.SemanticIndex = inputElement.semanticIndex;
		inputElementDesc.Format = inputElement.Format;

		inputElementDesc.InstanceDataStepRate = 0;
		inputElementDesc.InputSlot = inputSlot++;
		inputElementDesc.AlignedByteOffset = 0;
		inputElementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;

		m_inputLayout.push_back(inputElementDesc);
	}
}

void SHRShaderPassObject::BindRootParameters(const SHRShaderResouceBinding& resoucebindings)
{
	ID3D12GraphicsCommandList* cmdList = m_renderContext.GetCmdList();

	for (size_t i = 0; i < resoucebindings.m_resoureCache.numTable; i++)
	{
		const SHRShaderResouceCache::RootTable& rootTable = resoucebindings.m_resoureCache.GetRootTable(i);
		const SHRShaderResouceCache::ResourceView& resouce = rootTable.GetResouce(0);
		if (rootTable.offsetFromHeapStart == -1)
		{
			//rootView		
			if (resouce.type == SHRResourceViewType::CBV)
				cmdList->SetGraphicsRootConstantBufferView(i, resouce.pResouceObj->m_resourceGPUAddress);
			else if (resouce.type == SHRResourceViewType::SRV)
				cmdList->SetGraphicsRootShaderResourceView(i, resouce.pResouceObj->m_resourceGPUAddress);
			else if (resouce.type == SHRResourceViewType::UAV)
				cmdList->SetGraphicsRootUnorderedAccessView(i, resouce.pResouceObj->m_resourceGPUAddress);
		}
		else
		{
			//rootTable
			SHRDescriptorCache* descriptorCache = m_renderContext.GetDescriptorCache();
			D3D12_GPU_DESCRIPTOR_HANDLE baseHandle = resouce.type == SHRResourceViewType::Sampler ? descriptorCache->GetSamplerHeapBaseHandle() : descriptorCache->GetCbvSrvUavHeapBaseHandle();
			UINT incrementSize = resouce.type == SHRResourceViewType::Sampler ? descriptorCache->m_samplerIncrementSize : descriptorCache->m_cbvSrvUavOffset;

			cmdList->SetGraphicsRootDescriptorTable(i, CD3DX12_GPU_DESCRIPTOR_HANDLE(baseHandle, rootTable.offsetFromHeapStart, incrementSize));
		}
	}
}