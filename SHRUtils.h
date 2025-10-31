#pragma once

#include <windows.h>
#include <wrl.h>

#define UPPER_ALIGNMENT(A,B) ((UINT64)(((A) + ((B)-1)) &~((B)-1)))

#define ThrowIfFailed(hr) \
{\
	HRESULT _hr = (hr);\
	if (FAILED(_hr)) \
	{\
		throw COMException(_hr);\
	}\
}

class COMException
{
public:
	COMException(HRESULT hr) : error(hr) {}
	HRESULT Error() const { return error; }
private:
	const HRESULT error;
};