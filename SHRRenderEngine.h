#pragma once

#include <string>
#include <vector>
#include <unordered_map>

#include <dxgi1_6.h>

#include "d3dx12.h"
#include "SHRUtils.h"
#include "SHRRenderContext.h"
#include "SHRResourceView.h"
#include "SHRShaderPassObject.h"

using Microsoft::WRL::ComPtr;

class SHRRenderEngine
{
public:
	SHRRenderEngine() = delete;
	SHRRenderEngine(uint32_t width, uint32_t height, std::wstring name);
	~SHRRenderEngine() = default;

	static uint32_t GetFrameCount() { return FrameCount; }

	void OnInit();
	void OnUpdate();
	void OnRender();
	void OnDestory();

	// Samples override the event handlers to handle specific messages.
	void OnKeyDown(UINT8 /*key*/) {}
	void OnKeyUp(UINT8 /*key*/) {}

	// Accessors.
	uint32_t GetWidth() const { return m_width; }
	uint32_t GetHeight() const { return m_height; }
	const wchar_t* GetTitle() const { return m_title.c_str(); }

private:
	void InitializeRenderContext();
	void CompileShaders();

	void BeginFrame();
	void EndFrame();
	void PopulateCommandList();
	void ExecuteCommandQueue();
	void WaitForGPUSynchronize();

private:
	static const uint32_t FrameCount = 2;

	/*  std::wstring GetAssetFullPath(LPCWSTR assetName);

	  void GetHardwareAdapter(
		  _In_ IDXGIFactory1* pFactory,
		  _Outptr_result_maybenull_ IDXGIAdapter1** ppAdapter,
		  bool requestHighPerformanceAdapter = false);

	  void SetCustomWindowText(LPCWSTR text);*/

	  // Viewport dimensions.
	uint32_t m_width;
	uint32_t m_height;
	float m_aspectRatio;
	std::wstring m_title;

	CD3DX12_VIEWPORT m_viewport;
	CD3DX12_RECT m_scissorRect;

	std::unique_ptr<SHRRenderContext> m_renderContext;

	std::vector<SHRResource> m_renderTargets;
	std::vector<SHRResource> m_depthStencils;
	std::vector<SHRRenderTargetView> rtvs;

	std::unordered_map<std::string, std::unique_ptr<SHRShader>> m_shaderMap;

	std::vector<SHRVertexBufferView> vbvs;

	// Synchronization objects.
	uint32_t m_frameIndexBackBuffer;
	uint32_t m_frameIndex;
	HANDLE m_fenceEvent;
};

