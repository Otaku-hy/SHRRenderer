#include "SHRShader.h"
#include <d3d12shader.h>

std::unordered_map<std::wstring, SHRShader> g_globalShaderMap;

Microsoft::WRL::ComPtr<IDxcResult> CompileShaderWithDXC(const std::wstring& shaderSourcePath, const std::wstring& entryPoint, const std::wstring& target, const std::vector<std::wstring>& compileFlags, const std::unordered_map<std::wstring, std::wstring> defines = {})
{
	Microsoft::WRL::ComPtr<IDxcUtils> pUtils;
	Microsoft::WRL::ComPtr<IDxcCompiler3> pCompiler;
	Microsoft::WRL::ComPtr<IDxcIncludeHandler> pIncludeHandler;

	ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pCompiler)));
	ThrowIfFailed(pUtils->CreateDefaultIncludeHandler(&pIncludeHandler));

	Microsoft::WRL::ComPtr<IDxcBlobEncoding> pSourceCode = nullptr;
	ThrowIfFailed(pUtils->LoadFile(shaderSourcePath.c_str(), nullptr, &pSourceCode));
	DxcBuffer sourceCode;
	sourceCode.Encoding = DXC_CP_ACP;
	sourceCode.Ptr = pSourceCode->GetBufferPointer();
	sourceCode.Size = pSourceCode->GetBufferSize();

	std::vector<LPCWSTR> arguments =
	{
		shaderSourcePath.c_str(),
		L"-E", entryPoint.c_str(),
		L"-T", target.c_str(),
		L"-Qstrip_debug",
		L"-Qstrip_reflect",
	};

	for (auto& flag : compileFlags)
	{
		arguments.push_back(flag.c_str());
	}

	std::vector<std::wstring> defineStorage;
	for (auto& define : defines)
	{
		std::wstring defineArg = define.first;
		if (!define.second.empty())
		{
			defineArg += L"=" + define.second;
		}

		defineStorage.push_back(defineArg);
		arguments.push_back(L"-D");
		arguments.push_back(defineStorage.back().c_str());
	}

	Microsoft::WRL::ComPtr<IDxcResult> pResult;
	ThrowIfFailed(pCompiler->Compile(&sourceCode, arguments.data(), arguments.size(), pIncludeHandler.Get(), IID_PPV_ARGS(&pResult)));

	Microsoft::WRL::ComPtr<IDxcBlobUtf8> pErrors;
	pResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&pErrors), nullptr);
	if (pErrors && pErrors->GetStringLength() != 0)
	{
		OutputDebugStringA(pErrors->GetStringPointer());
	}

	return pResult;
}

SHRShader::SHRShader(const std::wstring& name, const std::wstring& entryPoint, const std::wstring& target, const std::vector<std::wstring>& compileFlags, const std::unordered_map<std::wstring, std::wstring> defines)
{
	using namespace Microsoft::WRL;

	ComPtr<IDxcUtils> pUtils;
	ThrowIfFailed(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&pUtils)));

	std::wstring path = L"./Shaders/" + name + L".hlsl";
	ComPtr<IDxcResult> pResult = CompileShaderWithDXC(path, entryPoint, target, compileFlags, defines);

	ThrowIfFailed(pResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&m_shaderBlob), nullptr));

	ComPtr<IDxcBlob> hashBlob;
	ThrowIfFailed(pResult->GetOutput(DXC_OUT_SHADER_HASH, IID_PPV_ARGS(&hashBlob), nullptr));
	if (hashBlob)
	{
		const DxcShaderHash* hash = reinterpret_cast<DxcShaderHash*>(hashBlob->GetBufferPointer());
		for (size_t i = 0; i < 16; i++)
			shaderHash[i] = hash->HashDigest[i];
	}

	ComPtr<ID3D12ShaderReflection> pReflection;

	DxcBuffer reflectionData;
	ComPtr<IDxcBlob> pReflectionBlob;
	ThrowIfFailed(pResult->GetOutput(DXC_OUT_REFLECTION, IID_PPV_ARGS(&pReflectionBlob), nullptr));
	if (pReflectionBlob)
	{
		reflectionData.Encoding = DXC_CP_ACP;
		reflectionData.Ptr = pReflectionBlob->GetBufferPointer();
		reflectionData.Size = pReflectionBlob->GetBufferSize();
		ThrowIfFailed(pUtils->CreateReflection(&reflectionData, IID_PPV_ARGS(&pReflection)));
	}

	D3D12_SHADER_DESC shaderDesc;
	pReflection->GetDesc(&shaderDesc);

	for (size_t i = 0; i < shaderDesc.BoundResources; i++)
	{
		D3D12_SHADER_INPUT_BIND_DESC resourceBindingDesc;
		pReflection->GetResourceBindingDesc(i, &resourceBindingDesc);

		ShaderResourceReflection reflection;
		reflection.bindCount = resourceBindingDesc.BindCount;
		reflection.bindPoint = resourceBindingDesc.BindPoint;
		reflection.resourceName = resourceBindingDesc.Name;
		reflection.space = resourceBindingDesc.Space;

		D3D_SHADER_INPUT_TYPE type = resourceBindingDesc.Type;
		switch (type)
		{
		case D3D_SIT_CBUFFER:
			reflection.type = SHRResourceViewType::CBV;
			break;
		case D3D_SIT_TEXTURE:
		case D3D_SIT_STRUCTURED:
			reflection.type = SHRResourceViewType::SRV;
			break;
		case D3D_SIT_UAV_RWTYPED:
		case D3D_SIT_UAV_RWSTRUCTURED:
			reflection.type = SHRResourceViewType::UAV;
			break;
		case D3D_SIT_SAMPLER:
			reflection.type = SHRResourceViewType::Sampler;
			break;
		default:
			break;
		}
		m_shaderResourceReflections.push_back(reflection);
	}

	for (size_t i = 0; i < shaderDesc.InputParameters; i++)
	{
		D3D12_SIGNATURE_PARAMETER_DESC inputParameterDesc;
		pReflection->GetInputParameterDesc(i, &inputParameterDesc);

		VSInputElement element;
		element.semanticName = inputParameterDesc.SemanticName;
		element.semanticIndex = inputParameterDesc.SemanticIndex;
		switch (inputParameterDesc.ComponentType)
		{
		case D3D_REGISTER_COMPONENT_FLOAT32:
		{
			if ((inputParameterDesc.Mask & 8))
				element.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			else if ((inputParameterDesc.Mask & 4))
				element.Format = DXGI_FORMAT_R32G32B32_FLOAT;
			else if ((inputParameterDesc.Mask & 2))
				element.Format = DXGI_FORMAT_R32G32_FLOAT;
			else if ((inputParameterDesc.Mask & 1))
				element.Format = DXGI_FORMAT_R32_FLOAT;
		}
		break;
		case D3D_REGISTER_COMPONENT_UINT32:
		{
			if ((inputParameterDesc.Mask & 8))
				element.Format = DXGI_FORMAT_R32G32B32A32_UINT;
			else if ((inputParameterDesc.Mask & 4))
				element.Format = DXGI_FORMAT_R32G32B32_UINT;
			else if ((inputParameterDesc.Mask & 2))
				element.Format = DXGI_FORMAT_R32G32_UINT;
			else if ((inputParameterDesc.Mask & 1))
				element.Format = DXGI_FORMAT_R32_UINT;
		}
		break;
		case D3D_REGISTER_COMPONENT_SINT32:
		{
			if ((inputParameterDesc.Mask & 8))
				element.Format = DXGI_FORMAT_R32G32B32A32_SINT;
			else if ((inputParameterDesc.Mask & 4))
				element.Format = DXGI_FORMAT_R32G32B32_SINT;
			else if ((inputParameterDesc.Mask & 2))
				element.Format = DXGI_FORMAT_R32G32_SINT;
			else if ((inputParameterDesc.Mask & 1))
				element.Format = DXGI_FORMAT_R32_SINT;
		}
		break;
		default:
			break;
		}
		m_vsInputElements.push_back(element);
	}

}
