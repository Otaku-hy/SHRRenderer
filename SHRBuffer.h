#pragma once
#include "SHRResource.h"

class SHRBuffer
{
public:
	SHRResource* GetSHRResource() { return &mResource; };

private:
	SHRResource mResource;
};

class SHRConstantBuffer : SHRBuffer
{
public:
	SHRConstantBuffer();
	~SHRConstantBuffer();

private:
//	std::unique_ptr<>
};
