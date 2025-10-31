#include "SHRRenderEngine.h"
#include "SHRShaderPassObject.h"

SHRRenderEngine::SHRRenderEngine(uint32_t width, uint32_t height, std::wstring name) :
	m_width(width),
	m_height(height),
	m_title(name),
	m_viewport(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)),
	m_scissorRect(0, 0, static_cast<LONG>(width), static_cast<LONG>(height))
{
	m_aspectRatio = static_cast<float>(width) / static_cast<float>(height);
	m_frameIndexBackBuffer = 0;
	m_frameIndex = 0;
	m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (m_fenceEvent == nullptr) {
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}
}

void SHRRenderEngine::OnInit()
{
	InitializeRenderContext();

	SHRHeapSlotAllocator* rtvAllocator = m_renderContext->GetHeapSlotManager(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	IDXGISwapChain3* pSwapChain = m_renderContext->GetSwapChain();
	for (uint32_t n = 0; n < FrameCount; n++) {
		ID3D12Resource* pBackBuffer = nullptr;
		ThrowIfFailed(pSwapChain->GetBuffer(n, IID_PPV_ARGS(&pBackBuffer)));
		m_renderTargets.emplace_back(pBackBuffer); //(pBackBuffer, D3D12_RESOURCE_STATE_COMMON);
		rtvs.emplace_back(*(m_renderContext.get()), &m_renderTargets[n]);
	}

	CompileShaders();
}

void SHRRenderEngine::OnUpdate()
{
	// Define the geometry for a triangle.
	float positionStreamData[] = { 0.0f, 0.25f * m_aspectRatio, 0.0f, 0.25f, -0.25f * m_aspectRatio, 0.0f , -0.25f, -0.25f * m_aspectRatio, 0.0f };
	float colorStreamData[] = { 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f };

	SHRBuddySystem* pBufferAllocator = m_renderContext->GetBufferAllocator();

	SHRResource positionStream;
	SHRResource colorStream;

	pBufferAllocator->AllocateSHRResouce(D3D12_HEAP_TYPE_UPLOAD, CD3DX12_RESOURCE_DESC::Buffer(sizeof(positionStreamData)), D3D12_RESOURCE_STATE_GENERIC_READ, sizeof(positionStreamData), positionStream, nullptr);
	pBufferAllocator->AllocateSHRResouce(D3D12_HEAP_TYPE_UPLOAD, CD3DX12_RESOURCE_DESC::Buffer(sizeof(colorStreamData)), D3D12_RESOURCE_STATE_GENERIC_READ, sizeof(colorStreamData), colorStream, nullptr);

	UINT8* pPositionMapData = reinterpret_cast<UINT8*>(positionStream.Map(0));
	UINT8* pColorMapData = reinterpret_cast<UINT8*>(colorStream.Map(0));

	memcpy(pPositionMapData, positionStreamData, sizeof(positionStreamData));
	memcpy(pColorMapData, colorStreamData, sizeof(colorStreamData));

	if (vbvs.empty())
	{
		vbvs.emplace_back(*m_renderContext.get(), (UINT)3 * sizeof(float), (UINT)sizeof(positionStreamData), &positionStream);
		vbvs.emplace_back(*m_renderContext.get(), (UINT)4 * sizeof(float), (UINT)sizeof(colorStreamData), &colorStream);
	}
	else
	{
		vbvs[0] = SHRVertexBufferView(*m_renderContext.get(), (UINT)3 * sizeof(float), (UINT)sizeof(positionStreamData), &positionStream);
		vbvs[1] = SHRVertexBufferView(*m_renderContext.get(), (UINT)4 * sizeof(float), (UINT)sizeof(colorStreamData), &colorStream);
	}
}

void SHRRenderEngine::OnRender()
{
	BeginFrame();
	PopulateCommandList();
	EndFrame();
}

void SHRRenderEngine::OnDestory()
{
	WaitForGPUSynchronize();
	CloseHandle(m_fenceEvent);
}

void SHRRenderEngine::InitializeRenderContext()
{
	m_renderContext = std::make_unique<SHRRenderContext>(this);
}

void SHRRenderEngine::CompileShaders()
{
	std::vector<std::wstring> compileFlags;
#if defined(_DEBUG)
	compileFlags.push_back(DXC_ARG_DEBUG);
	compileFlags.push_back(DXC_ARG_SKIP_OPTIMIZATIONS);
#endif

	SHRShader vs(L"shaders", L"VSMain", L"vs_6_0", compileFlags);
	SHRShader ps(L"shaders", L"PSMain", L"ps_6_0", compileFlags);

	m_shaderMap["VS"] = std::make_unique<SHRShader>(vs);
	m_shaderMap["PS"] = std::make_unique<SHRShader>(ps);
}

void SHRRenderEngine::BeginFrame()
{

}

void SHRRenderEngine::EndFrame()
{
	ExecuteCommandQueue();
	WaitForGPUSynchronize();

	m_renderContext->CleanupContext();
	m_frameIndexBackBuffer = m_renderContext->GetCurrentBackBufferIndex();
	m_frameIndex++;
}

void SHRRenderEngine::PopulateCommandList()
{
	SHRShaderPassDesc passDesc;
	passDesc.pShaders[0] = m_shaderMap["VS"].get();
	passDesc.pShaders[1] = m_shaderMap["PS"].get();
	passDesc.pShaders[2] = nullptr;
	passDesc.pShaders[3] = nullptr;
	passDesc.pShaders[4] = nullptr;
	passDesc.pShaders[5] = nullptr;

	passDesc.blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	passDesc.rasterizerDesc = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc(D3D12_DEFAULT);
	depthStencilDesc.DepthEnable = false;
	passDesc.depthStencilDesc = depthStencilDesc;
	passDesc.primitiveType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	passDesc.rtvFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	passDesc.numRenderTargets = 1;
	passDesc.dsvFormat = DXGI_FORMAT_UNKNOWN;

	SHRShaderPassObject* passObject = SHRShaderPassObject::GetShaderPassObject(*m_renderContext.get(), passDesc);

	ID3D12GraphicsCommandList* cmdList = m_renderContext->GetCmdList();
	ID3D12CommandAllocator* cmdAllocator = m_renderContext->GetCmdAllocator();

	ThrowIfFailed(cmdAllocator->Reset());
	ThrowIfFailed(cmdList->Reset(cmdAllocator, passObject->m_pPipelineState));

	// Set necessary state.
	cmdList->SetGraphicsRootSignature(passObject->m_pRootSignature.Get());
	cmdList->RSSetViewports(1, &m_viewport);
	cmdList->RSSetScissorRects(1, &m_scissorRect);

	// Indicate that the back buffer will be used as a render target.
	CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndexBackBuffer].m_pSHRD3dResource->m_pResource.Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	cmdList->ResourceBarrier(1, &barrier);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvs[m_frameIndexBackBuffer].GetViewHandle();

	cmdList->OMSetRenderTargets(1, &rtvHandle, TRUE, nullptr);

	// Record commands.
	const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	cmdList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);


	D3D12_VERTEX_BUFFER_VIEW vertexBufferViews[] = { vbvs[0].GetVertexBufferView(), vbvs[1].GetVertexBufferView() };
	cmdList->IASetVertexBuffers(0, _countof(vertexBufferViews), vertexBufferViews);
	/*CD3DX12_RESOURCE_BARRIER barrierV0 = CD3DX12_RESOURCE_BARRIER::Aliasing(nullptr, vbvs[0].m_pResource->m_pResource.Get());
	cmdList->ResourceBarrier(1, &barrierV0);*/
	cmdList->DrawInstanced(3, 1, 0, 0);

	// Indicate that the back buffer will now be used to present.
	CD3DX12_RESOURCE_BARRIER barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(m_renderTargets[m_frameIndexBackBuffer].m_pSHRD3dResource->m_pResource.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	cmdList->ResourceBarrier(1, &barrier1);

	ThrowIfFailed(cmdList->Close());

}

void SHRRenderEngine::ExecuteCommandQueue()
{
	ID3D12CommandQueue* cmdQueue = m_renderContext->GetCmdQueue();
	ID3D12GraphicsCommandList* cmdList = m_renderContext->GetCmdList();

	ID3D12CommandList* ppCommandLists[] = { cmdList };
	cmdQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	m_renderContext->Present();
}

void SHRRenderEngine::WaitForGPUSynchronize()
{
	uint64_t& gpuFenceValue = m_renderContext->GetGPUFenceValue();

	ID3D12CommandQueue* cmdQueue = m_renderContext->GetCmdQueue();
	ID3D12Fence* fence = m_renderContext->GetFence();

	const uint64_t waitValue = gpuFenceValue;
	ThrowIfFailed(cmdQueue->Signal(fence, waitValue));
	gpuFenceValue++;

	if (fence->GetCompletedValue() < waitValue)
	{
		ThrowIfFailed(fence->SetEventOnCompletion(waitValue, m_fenceEvent));
		WaitForSingleObject(m_fenceEvent, INFINITE);
	}
}

