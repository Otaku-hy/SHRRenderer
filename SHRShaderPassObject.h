#pragma once

#include <unordered_map>

#include "SHRShader.h"
#include "SHRShaderResouceBinding.h"

struct SHRRootSignatureManager
{
public:
	struct RootParameter : public CD3DX12_ROOT_PARAMETER1
	{
	public:
		void AddDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
			UINT numDescriptors,
			UINT baseShaderRegister,
			UINT registerSpace = 0,
			D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
			UINT offsetInDescriptorsFromTableStart = 0)
		{
			m_descriptorRanges.emplace_back(rangeType, numDescriptors, baseShaderRegister, registerSpace, flags, offsetInDescriptorsFromTableStart);
			DescriptorTable.NumDescriptorRanges = static_cast<UINT>(m_descriptorRanges.size());
			DescriptorTable.pDescriptorRanges = m_descriptorRanges.data();
			descriptorOffsetFromTableBegin += numDescriptors;
		}

		UINT GetRootIndex() const
		{
			return rootParamIndex;
		}

		UINT GetDescriptorOffset() const
		{
			return descriptorOffsetFromTableBegin;
		}
	public:
		std::vector<CD3DX12_DESCRIPTOR_RANGE1> m_descriptorRanges;

		UINT rootParamIndex;
		UINT descriptorOffsetFromTableBegin;
	};

public:
	SHRRootSignatureManager();

	int GetRootIndexForCBVSRVUAV(int key) const
	{
		return m_cbvSrvUavRootIndexMap[key];
	}
	int GetRootIndexForSampler(int key) const
	{
		return m_samplerRootIndexMap[key];
	}

	Microsoft::WRL::ComPtr<ID3D12RootSignature> CreateRootSignature(ID3D12Device* pDevice);

	void AllocateRootParameterSlot(SHRShaderType type, const SHRShader::ShaderResourceReflection& reflection, UINT& rootParamIndex, UINT& offsetFromTableStart);

private:
	UINT AddRootDescriptor(UINT rootParamIndex, D3D12_ROOT_PARAMETER_TYPE type,
		UINT shaderRegister, UINT registerSpace,
		D3D12_SHADER_VISIBILITY visibility);
	UINT AddDescriptorTable(UINT rootParamIndex, D3D12_SHADER_VISIBILITY visibility);

public:
	std::vector<RootParameter> m_rootTables;
	std::vector<RootParameter> m_rootViews;

	// every shader takes a root signature table slot in the root signature
	std::vector<int> m_cbvSrvUavRootIndexMap;
	std::vector<int> m_samplerRootIndexMap;
};

struct SHRShaderPassDesc
{
	//in our model, when shaders are compiled, the rootSig and inputLayout is fixed, so the desc will not contain them	
	D3D12_BLEND_DESC blendDesc;
	D3D12_RASTERIZER_DESC rasterizerDesc;
	D3D12_DEPTH_STENCIL_DESC depthStencilDesc;
	D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType;
	DXGI_FORMAT rtvFormats[8];
	UINT numRenderTargets;
	DXGI_FORMAT dsvFormat;

	const SHRShader* pShaders[static_cast<uint8_t>(SHRShaderType::NumShaderTypes)];

	bool operator==(const SHRShaderPassDesc& other) const {
		if (memcmp(&blendDesc, &other.blendDesc, sizeof(D3D12_BLEND_DESC)) != 0) return false;
		if (memcmp(&rasterizerDesc, &other.rasterizerDesc, sizeof(D3D12_RASTERIZER_DESC)) != 0) return false;
		if (memcmp(&depthStencilDesc, &other.depthStencilDesc, sizeof(D3D12_DEPTH_STENCIL_DESC)) != 0) return false;

		if (primitiveType != other.primitiveType) return false;

		if (numRenderTargets != other.numRenderTargets) return false;
		for (UINT i = 0; i < numRenderTargets; ++i) {
			if (rtvFormats[i] != other.rtvFormats[i]) return false;
		}
		if (dsvFormat != other.dsvFormat) return false;

		for (size_t i = 0; i < static_cast<uint8_t>(SHRShaderType::NumShaderTypes); ++i) {
			if (pShaders[i] != other.pShaders[i]) return false;
		}

		return true;
	}
};

class SHRShaderPassObject
{
public:
	SHRShaderPassObject(SHRRenderContext& renderContext, const SHRShaderPassDesc& desc);
	static SHRShaderPassObject* GetShaderPassObject(SHRRenderContext& renderContext, const SHRShaderPassDesc& desc);

private:
	void InitializeShaderResoureLayout();
	void InitializeInputLayout();
	void InitializePipelineState(const SHRShaderPassDesc& desc);

	void BindRootParameters(const SHRShaderResouceBinding& resoucebindings);

public:
	std::vector<const SHRShader*> m_pPassShader;
	std::vector<SHRShaderResoureLayout> m_resourceLayouts;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_pRootSignature;

	std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
	ID3D12PipelineState* m_pPipelineState;

private:
	SHRRenderContext& m_renderContext;
};

extern std::unordered_map<SHRShaderPassDesc, std::unique_ptr<SHRShaderPassObject>> g_passObjectCache;

