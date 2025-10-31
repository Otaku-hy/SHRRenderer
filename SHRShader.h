#pragma once

#include <string>
#include <sstream>
#include <vector>
#include <unordered_map>

#include <windows.h>
#include <wrl.h>
#include <d3d12.h>

#include "dxc/dxcapi.h"
#include "SHRUtils.h"
#include "SHRResourceView.h"

enum class SHRShaderType : uint8_t
{
	Vertex = 0,
	Pixel = Vertex + 1,
	Compute = Pixel + 1,
	Geometry = Compute + 1,
	Hull = Geometry + 1,
	Domain = Hull + 1,
	NumShaderTypes = Domain + 1
};

class SHRShader
{
public:
	struct ShaderResourceReflection
	{
        std::string resourceName;           // Name of the resource
        SHRResourceViewType type;
        UINT bindPoint;      // Starting bind point
        UINT bindCount;      // Number of contiguous bind points (for arrays)
        UINT space;          // Register space

       // D3D_SHADER_INPUT_TYPE       Type;           // Type of resource (e.g. texture, cbuffer, etc.)
        //D3D_RESOURCE_RETURN_TYPE    ReturnType;     // Return type (if texture)
        //D3D_SRV_DIMENSION           Dimension;      // Dimension (if texture)
	};
	struct VSInputElement
	{
		std::string semanticName;
		UINT semanticIndex; 
		DXGI_FORMAT Format;
	};

public:
	SHRShader(const std::wstring& name, const std::wstring& entryPoint, const std::wstring& target, const std::vector<std::wstring>& compileFlags = {}, const std::unordered_map<std::wstring, std::wstring> defines = {});

public:
	Microsoft::WRL::ComPtr<IDxcBlob> m_shaderBlob;

	std::vector<ShaderResourceReflection> m_shaderResourceReflections;
	std::vector<VSInputElement> m_vsInputElements;

	BYTE shaderHash[16];
};

class SHRShaderResoureLayout
{
public:
	struct ElementLayout
	{
		UINT rootParamIndex = -1;
		UINT offsetFromTableStart;
		const SHRShader::ShaderResourceReflection* pResouceReflection;
	};

	void AddElement(const std::string& name, const ElementLayout& layout)
	{
		m_layoutArray.push_back(layout);
		m_layoutMapByName[name] = m_layoutArray.end() - 1;
	}

	ElementLayout& GetElementLayout(const std::string& name)
	{
		auto it = m_layoutMapByName.find(name);
		if (it != m_layoutMapByName.end())
			return *(it->second);
		return ElementLayout();
	}

	const ElementLayout& GetElementLayout(const std::string& name) const
	{
		auto it = m_layoutMapByName.find(name);
		if (it != m_layoutMapByName.end())
			return *(it->second);
		return ElementLayout();
	}

public:
	UINT m_rootTableCount;
	UINT m_rootViewCount;

	std::vector<ElementLayout> m_layoutArray;
	std::unordered_map<std::string, std::vector<ElementLayout>::iterator> m_layoutMapByName;
	std::vector<SHRShader::ShaderResourceReflection>* m_pShaderResourceReflections;
};

extern std::unordered_map<std::wstring, SHRShader> g_globalShaderMap;

