#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include "StaticMeshComponent.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

LPCTSTR gWindowsClassName = L"BattleFire";//ASCII
ID3D12Device* gD3D12Device = nullptr;
ID3D12CommandQueue* gCommandQueue = nullptr;
IDXGISwapChain3* gSwapChain = nullptr;
ID3D12Resource* gDSRT = nullptr, *gColorRTs[2];

int gCurrentRTIndex = 0;

ID3D12DescriptorHeap* gSwapChainRTVHeap = nullptr;
ID3D12DescriptorHeap* gSwapChainDSVHeap = nullptr;
UINT gRTVDescriptorSize = 0;
UINT gDSVDescriptorSize = 0;
ID3D12CommandAllocator* gCommandAllocator = nullptr;
ID3D12GraphicsCommandList* gCommandList = nullptr;
ID3D12Fence* gFence = nullptr;
HANDLE gFenceEvent = nullptr;
UINT64 gFenceValue = 0;

// 初始化状态转换
D3D12_RESOURCE_BARRIER InitResourceBarrier(
	ID3D12Resource* inResource, D3D12_RESOURCE_STATES inPrevState,
	D3D12_RESOURCE_STATES inNextState) {
	D3D12_RESOURCE_BARRIER d3d12ResourceBarrier;
	memset(&d3d12ResourceBarrier, 0, sizeof(d3d12ResourceBarrier));
	d3d12ResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3d12ResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3d12ResourceBarrier.Transition.pResource = inResource;
	d3d12ResourceBarrier.Transition.StateBefore = inPrevState;
	d3d12ResourceBarrier.Transition.StateAfter = inNextState;
	d3d12ResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	return d3d12ResourceBarrier;
}

// 初始化根签名
ID3D12RootSignature* InitRootSignature() {
	//1110001110101111111111111111111111
	D3D12_ROOT_PARAMETER _0Parameter = {};
	_0Parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	_0Parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	_0Parameter.Constants.RegisterSpace = 0;
	_0Parameter.Constants.ShaderRegister = 0;
	_0Parameter.Constants.Num32BitValues = 4;
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.NumParameters = 1;
	rootSignatureDesc.pParameters = &_0Parameter;
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// 64 DWORD 128 WORD 16BIT

	ID3DBlob* signature;
	HRESULT hResult = D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, nullptr);
	ID3D12RootSignature* d3d12RootSignature;
	gD3D12Device->CreateRootSignature(
		0, signature->GetBufferPointer(), signature->GetBufferSize(),
		IID_PPV_ARGS(&d3d12RootSignature));

	return d3d12RootSignature;
}

// 编译shader
void CreateShaderFromFile(
	LPCTSTR inShaderFilePath,
	const char* inMainFunctionName,
	const char* inTarget,//"vs_5_0","ps_5_0","vs_4_0"
	D3D12_SHADER_BYTECODE* inShader) {
	ID3DBlob* shaderBuffer = nullptr;
	ID3DBlob* errorBuffer = nullptr;
	HRESULT hResult = D3DCompileFromFile(inShaderFilePath, nullptr, nullptr,
		inMainFunctionName, inTarget, D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0, &shaderBuffer, &errorBuffer);
	if (FAILED(hResult)) {
		printf("CreateShaderFromFile error : [%s][%s]:[%s]\n", inMainFunctionName, inTarget, (char*)errorBuffer->GetBufferPointer());
		errorBuffer->Release();
		return;
	}
	inShader->pShaderBytecode = shaderBuffer->GetBufferPointer();
	inShader->BytecodeLength = shaderBuffer->GetBufferSize();
}

// 创建缓冲区对象（初始化VBO）
ID3D12Resource* CreateBufferObject(ID3D12GraphicsCommandList* inCommandList,
	void* inData, int inDataLen, D3D12_RESOURCE_STATES inFinalResourceState) {
	D3D12_HEAP_PROPERTIES d3dHeapProperties = {};
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;//gpu
	D3D12_RESOURCE_DESC d3d12ResourceDesc = {};
	d3d12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	d3d12ResourceDesc.Alignment = 0;
	d3d12ResourceDesc.Width = inDataLen;
	d3d12ResourceDesc.Height = 1;
	d3d12ResourceDesc.DepthOrArraySize = 1;
	d3d12ResourceDesc.MipLevels = 1;
	d3d12ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	d3d12ResourceDesc.SampleDesc.Count = 1;
	d3d12ResourceDesc.SampleDesc.Quality = 0;
	d3d12ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	d3d12ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* bufferObject = nullptr;
	gD3D12Device->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3d12ResourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&bufferObject)
	);
	d3d12ResourceDesc = bufferObject->GetDesc();
	UINT64 memorySizeUsed = 0;
	UINT64 rowSizeInBytes = 0;
	UINT rowUsed = 0;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT subresourceFootprint;
	gD3D12Device->GetCopyableFootprints(&d3d12ResourceDesc, 0, 1, 0,
		&subresourceFootprint, &rowUsed, &rowSizeInBytes, &memorySizeUsed);
	// 3x4x4 = 48bytes分配48个字节，假如GPU每次读取数据室32个字节，48字节要存储2行，每行24字节每行后面有8字节不用（也可能其他处理方法）
	// 申请临时缓存区
	ID3D12Resource* tempBufferObject = nullptr;
	d3dHeapProperties = {};
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;//CPU GPU可见内存
	gD3D12Device->CreateCommittedResource(
		&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3d12ResourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&tempBufferObject)
	);

	BYTE* pData;
	tempBufferObject->Map(0, nullptr, reinterpret_cast<void**>(&pData));
	BYTE* pDstTempBuffer = reinterpret_cast<BYTE*>(pData + subresourceFootprint.Offset);
	const BYTE* pSrcData = reinterpret_cast<BYTE*>(inData);
	for (UINT i = 0; i < rowUsed; i++) {
		memcpy(pDstTempBuffer + subresourceFootprint.Footprint.RowPitch * i, pSrcData + rowSizeInBytes * i, rowSizeInBytes);
	}
	tempBufferObject->Unmap(0, nullptr);
	inCommandList->CopyBufferRegion(bufferObject, 0, tempBufferObject, 0, subresourceFootprint.Footprint.Width);
	D3D12_RESOURCE_BARRIER barrier = InitResourceBarrier(bufferObject, D3D12_RESOURCE_STATE_COPY_DEST, inFinalResourceState);
	inCommandList->ResourceBarrier(1, &barrier);
	return bufferObject;
}

// 创建管线状态对象（PSO初始化）
ID3D12PipelineState* CreatePSO(ID3D12RootSignature* inID3D12RootSignature,
	D3D12_SHADER_BYTECODE inVertexShader, D3D12_SHADER_BYTECODE inPixelShader) {
	D3D12_INPUT_ELEMENT_DESC vertexDataElementDesc[] = {
		{"POSITION",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TEXCOORD",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,sizeof(float) * 4,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"NORMAL",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,sizeof(float) * 8,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},
		{"TANGENT",0,DXGI_FORMAT_R32G32B32A32_FLOAT,0,sizeof(float) * 12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}
	};
	D3D12_INPUT_LAYOUT_DESC vertexDataLayoutDesc = {};
	vertexDataLayoutDesc.NumElements = 4;
	vertexDataLayoutDesc.pInputElementDescs = vertexDataElementDesc;

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = inID3D12RootSignature;
	psoDesc.VS = inVertexShader;
	psoDesc.PS = inPixelShader;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.InputLayout = vertexDataLayoutDesc;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;

	psoDesc.DepthStencilState.DepthEnable = true;
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	psoDesc.BlendState = { 0 };
	D3D12_RENDER_TARGET_BLEND_DESC rtBlendDesc = {
		FALSE,FALSE,
		D3D12_BLEND_ONE,D3D12_BLEND_ZERO,D3D12_BLEND_OP_ADD,
		D3D12_BLEND_ONE,D3D12_BLEND_ZERO,D3D12_BLEND_OP_ADD,
		D3D12_LOGIC_OP_NOOP,
		D3D12_COLOR_WRITE_ENABLE_ALL,
	};
	for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
		psoDesc.BlendState.RenderTarget[i] = rtBlendDesc;
	psoDesc.NumRenderTargets = 1;
	ID3D12PipelineState* d3d12PSO = nullptr;

	HRESULT hResult = gD3D12Device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&d3d12PSO));
	if (FAILED(hResult)) {
		return nullptr;
	}
	return d3d12PSO;
}

bool InitD3D12(HWND inHWND, int inWidth, int inHeight) {
	HRESULT hResult;
	UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
	// 启用调试层
	ID3D12Debug* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
	debugController->EnableDebugLayer();
	// 之后创建DXGI工厂时需要使用这个标志
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
	}
#endif
	IDXGIFactory4* dxgiFactory;
	hResult = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
	if (FAILED(hResult)) {
		return false;
	}
	IDXGIAdapter1* adapter;
	int adapterIndex = 0;
	bool adapterFound = false;
	while (dxgiFactory->EnumAdapters1(adapterIndex,&adapter)!=DXGI_ERROR_NOT_FOUND) {
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}
		hResult = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
		if (SUCCEEDED(hResult)) {
			adapterFound = true;
			break;
		}
		adapterIndex++;
	}
	if (false == adapterFound) {
		return false;
	}
	hResult = D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&gD3D12Device));
	if(FAILED(hResult)) {
		return false;
	}
	// 创建命令队列
	D3D12_COMMAND_QUEUE_DESC d3d12CommandQueueDesc = {};
	hResult = gD3D12Device->CreateCommandQueue(&d3d12CommandQueueDesc, IID_PPV_ARGS(&gCommandQueue));
	if (FAILED(hResult)) {
		return false;
	}
	DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
	swapChainDesc.BufferCount = 2;
	swapChainDesc.BufferDesc = {};
	swapChainDesc.BufferDesc.Width = inWidth;
	swapChainDesc.BufferDesc.Height = inHeight;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.OutputWindow = inHWND;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	IDXGISwapChain* swapChain = nullptr;
	dxgiFactory->CreateSwapChain(gCommandQueue, &swapChainDesc, &swapChain);
	gSwapChain = static_cast<IDXGISwapChain3*>(swapChain);

	// 创建深度缓冲（这里没有用Depth命名是因为Resource这段代码之后会被抽象）
	D3D12_HEAP_PROPERTIES d3dHeapProperties = {};
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC d3d12ResourceDesc = {};
	d3d12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3d12ResourceDesc.Alignment = 0;
	d3d12ResourceDesc.Width = inWidth;
	d3d12ResourceDesc.Height = inHeight;
	d3d12ResourceDesc.DepthOrArraySize = 1;
	d3d12ResourceDesc.MipLevels = 1;
	d3d12ResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3d12ResourceDesc.SampleDesc.Count = 1;
	d3d12ResourceDesc.SampleDesc.Quality = 0;
	d3d12ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3d12ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE dsClearValue = {};
	dsClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsClearValue.DepthStencil.Depth = 1.0f;
	dsClearValue.DepthStencil.Stencil = 0;

	gD3D12Device->CreateCommittedResource(&d3dHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&d3d12ResourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&dsClearValue,
		IID_PPV_ARGS(&gDSRT)
	);

	// RTV,DSV
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDescRTV = {};
	d3dDescriptorHeapDescRTV.NumDescriptors = 2;
	d3dDescriptorHeapDescRTV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	gD3D12Device->CreateDescriptorHeap(&d3dDescriptorHeapDescRTV, IID_PPV_ARGS(&gSwapChainRTVHeap));
	gRTVDescriptorSize = gD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDescDSV = {};
	d3dDescriptorHeapDescDSV.NumDescriptors = 1;
	d3dDescriptorHeapDescDSV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	gD3D12Device->CreateDescriptorHeap(&d3dDescriptorHeapDescDSV, IID_PPV_ARGS(&gSwapChainDSVHeap));
	gDSVDescriptorSize = gD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapStart = gSwapChainRTVHeap->GetCPUDescriptorHandleForHeapStart();
	for (int i = 0; i < 2; i++) {
		gSwapChain->GetBuffer(i, IID_PPV_ARGS(&gColorRTs[i]));
		D3D12_CPU_DESCRIPTOR_HANDLE rtvPointer;
		rtvPointer.ptr = rtvHeapStart.ptr + i * gRTVDescriptorSize;
		gD3D12Device->CreateRenderTargetView(gColorRTs[i], nullptr, rtvPointer);
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC d3dDSViewDesc = {};
	d3dDSViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dDSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	gD3D12Device->CreateDepthStencilView(gDSRT, &d3dDSViewDesc, gSwapChainDSVHeap->GetCPUDescriptorHandleForHeapStart());

	// 完成渲染初始化（创建命令分配器）
	gD3D12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&gCommandAllocator));
	gD3D12Device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, gCommandAllocator, nullptr, IID_PPV_ARGS(&gCommandList));

	gD3D12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&gFence));
	gFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);



	return true;
}

void WaitForCompletionOfCommandList() {
	// 等待CommandList执行完毕
	if (gFence->GetCompletedValue() < gFenceValue) {
		gFence->SetEventOnCompletion(gFenceValue, gFenceEvent);
		WaitForSingleObject(gFenceEvent, INFINITE);
	}
}

void EndCommandList() {
	gCommandList->Close();// 关闭CommandList
	ID3D12CommandList* ppComandLists[] = { gCommandList };
	gCommandQueue->ExecuteCommandLists(1, ppComandLists);
	// 执行CommandList
	gFenceValue += 1;
	gCommandQueue->Signal(gFence, gFenceValue);
}

void BeginRenderTOSwapChain(ID3D12GraphicsCommandList* inCommandList) {
	gCurrentRTIndex = gSwapChain->GetCurrentBackBufferIndex();
	D3D12_RESOURCE_BARRIER barrier = InitResourceBarrier(gColorRTs[gCurrentRTIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	inCommandList->ResourceBarrier(1, &barrier);
	D3D12_CPU_DESCRIPTOR_HANDLE colorRT,dsv;
	dsv.ptr = gSwapChainDSVHeap->GetCPUDescriptorHandleForHeapStart().ptr;
	colorRT.ptr = gSwapChainRTVHeap->GetCPUDescriptorHandleForHeapStart().ptr + gCurrentRTIndex * gRTVDescriptorSize;
	inCommandList->OMSetRenderTargets(1, &colorRT, FALSE, &dsv);
	D3D12_VIEWPORT viewport = { 0.0f,0.0f,1280.0f,720.0f };
	D3D12_RECT scissorRect = { 0,0,1280,720 };
	inCommandList->RSSetViewports(1, &viewport);
	inCommandList->RSSetScissorRects(1, &scissorRect);
	const float clearColor[] = { 0.847f,0.718f,0.867f,1.0f };
	inCommandList->ClearRenderTargetView(colorRT, clearColor, 0, nullptr);
	inCommandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0.0f, nullptr);
}

void EndRenderToSwapChain(ID3D12GraphicsCommandList* inCommandList) {
	D3D12_RESOURCE_BARRIER barrier = InitResourceBarrier(gColorRTs[gCurrentRTIndex], D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	inCommandList->ResourceBarrier(1, &barrier);
}

LRESULT CALLBACK WindowProc(HWND inHWND, UINT inMSG, WPARAM inWParam, LPARAM inLParam){
	switch (inMSG) {
	case WM_CLOSE:
		PostQuitMessage(0);// enqueue WM_QUIT
		break;
	}
	return DefWindowProc(inHWND, inMSG, inWParam, inLParam);
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int inShowCmd){
	// 注册regiter
	WNDCLASSEX wndClassEx;
	wndClassEx.cbSize = sizeof(WNDCLASSEX);
	wndClassEx.style = CS_HREDRAW | CS_VREDRAW;
	wndClassEx.cbClsExtra = NULL; //class
	wndClassEx.cbWndExtra = NULL; //instance
	wndClassEx.hInstance = hInstance;
	wndClassEx.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndClassEx.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	wndClassEx.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndClassEx.hbrBackground = NULL;
	wndClassEx.lpszMenuName = NULL;
	wndClassEx.lpszClassName = gWindowsClassName;
	wndClassEx.lpfnWndProc = WindowProc;
	if (!RegisterClassEx(&wndClassEx))
	{
		MessageBox(NULL, L"Register Class Failed!", L"Error", MB_OK | MB_ICONERROR);
		return -1;
	}
	// 创建create
	int viewportWidth = 1280;
	int viewportHeight = 720;
	RECT rect;
	rect.left = 0;
	rect.top = 0;
	rect.right = viewportWidth;
	rect.bottom = viewportHeight;
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	int windowWidth = rect.right - rect.left;
	int windowHeight = rect.bottom - rect.top;
	HWND hwnd = CreateWindowEx(NULL,
		gWindowsClassName,
		L"D3D12Learn",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		windowWidth, windowHeight,
		NULL,
		NULL,
		hInstance,
		NULL);
	if (!hwnd) {
		MessageBox(NULL, L"Create Window Failed!", L"Error", MB_OK | MB_ICONERROR);
		return -1;
	}

	// 显示show
	InitD3D12(hwnd, 1280, 720);
	StaticMeshComponent staticMeshComponent;
	staticMeshComponent.SetVertexCount(3);
	staticMeshComponent.SetVertexPosition(0, -0.5f, -0.5f, 0.5f, 1.0f);
	staticMeshComponent.SetVertexTexcoord(0, 1.0f, 0.0f, 0.0f, 1.0f);
	staticMeshComponent.SetVertexPosition(1, 0.0f, 0.5f, 0.5f, 1.0f);
	staticMeshComponent.SetVertexTexcoord(1, 0.0f, 1.0f, 0.0f, 1.0f);
	staticMeshComponent.SetVertexPosition(2, 0.5f, -0.5f, 0.5f, 1.0f);
	staticMeshComponent.SetVertexTexcoord(2, 0.0f, 0.0f, 1.0f, 1.0f);

	staticMeshComponent.mVBO = CreateBufferObject(gCommandList,
		staticMeshComponent.mVertexData,
		sizeof(StaticMeshComponentVertexData) * staticMeshComponent.mVertexCount,
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);

	staticMeshComponent.InitFromFile(gCommandList, "Res/Model/Sphere.staticmesh");

	ID3D12RootSignature* rootSignature = InitRootSignature();
	D3D12_SHADER_BYTECODE vs, ps;
	CreateShaderFromFile(L"Res/Shader/ndctriangle.hlsl", "MainVS", "vs_5_0", &vs);
	CreateShaderFromFile(L"Res/Shader/ndctriangle.hlsl", "MainPS", "ps_5_0", &ps);
	ID3D12PipelineState* pso = CreatePSO(rootSignature, vs, ps);
	EndCommandList();
	WaitForCompletionOfCommandList();

	D3D12_VERTEX_BUFFER_VIEW vbos[] = {
		staticMeshComponent.mVBOView
	};

	ShowWindow(hwnd, inShowCmd);
	UpdateWindow(hwnd);

	float color[] = { 0.5f,0.5f,0.5f,1.0f };


	MSG msg;
	while (true) {
		// 可以把内存置于0
		ZeroMemory(&msg, sizeof(MSG));
		// 大系统中不可以这样用，会导致系统很卡，因为消息处理不及时
		if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
			// 跳到程序外处理
			if (msg.message == WM_QUIT) {
				break;
			}
			// 转换成应用程序能识别的消息
			TranslateMessage(&msg);
			// 直接调用WindowProc
			DispatchMessage(&msg);
		} else {
			// rendering
			WaitForCompletionOfCommandList();
			gCommandAllocator->Reset();
			gCommandList->Reset(gCommandAllocator, nullptr);
			BeginRenderTOSwapChain(gCommandList);
			//draw
			gCommandList->SetPipelineState(pso);
			gCommandList->SetGraphicsRootSignature(rootSignature);
			gCommandList->SetGraphicsRoot32BitConstants(0, 4, color, 0);
			gCommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			gCommandList->IASetVertexBuffers(0, 1, vbos);
			gCommandList->DrawInstanced(3,1,0,0);
			EndRenderToSwapChain(gCommandList);
			EndCommandList();
			gSwapChain->Present(0, 0);
		}
	}
	return 0;
}