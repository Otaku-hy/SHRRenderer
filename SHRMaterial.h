#pragma once

#include <vector>
#include <memory>

#include "SHRShader.h"
#include "SHRShaderPassObject.h"

class SHRMaterial
{
public:
	SHRMaterial();
	~SHRMaterial();

public:
	std::vector<std::unique_ptr<SHRShader>> m_shaderMap;
};

