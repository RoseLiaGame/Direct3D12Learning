#include <windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

LPCTSTR gWindowsClassName = L"BattleFire";//ASCII
ID3D12Device* gD3D12Device = nullptr;
ID3D12CommandQueue* gCommandQueue = nullptr;
IDXGISwapChain3* gSwapChain = nullptr;
ID3D12Resource* gDSRT = nullptr, *gColorRTs[2];
ID3D12DescriptorHeap* gSwapChainRTVHeap = nullptr;
ID3D12DescriptorHeap* gSwapChainDSVHeap = nullptr;
UINT gRTVDescriptorSize = 0;
UINT gDSVDescriptorSize = 0;
bool InitD3D12(HWND inHWND, int inWidth, int inHeight) {
	HRESULT hResult;
	UINT dxgiFactoryFlags = 0;
#ifdef _DEBUG
	// ���õ��Բ�
	ID3D12Debug* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
	debugController->EnableDebugLayer();
	// ֮�󴴽�DXGI����ʱ��Ҫʹ�������־
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
	// �����������
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

	// ������Ȼ��壨����û����Depth��������ΪResource��δ���֮��ᱻ����
	D3D12_HEAP_PROPERTIES d3dHeapProperties = {};
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC d3d12ResourceDesc = {};
	d3d12ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3d12ResourceDesc.Alignment = 0;
	d3d12ResourceDesc.Width = inWidth;
	d3d12ResourceDesc.Height = inHeight;
	d3d12ResourceDesc.DepthOrArraySize = 1;
	d3d12ResourceDesc.MipLevels = 0;
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
	gD3D12Device->CreateDescriptorHeap(&d3dDescriptorHeapDescRTV,IID_PPV_ARGS(&gSwapChainRTVHeap));
	gRTVDescriptorSize = gD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDescDSV = {};
	d3dDescriptorHeapDescDSV.NumDescriptors = 1;
	d3dDescriptorHeapDescDSV.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	gD3D12Device->CreateDescriptorHeap(&d3dDescriptorHeapDescDSV, IID_PPV_ARGS(&gSwapChainDSVHeap));
	gDSVDescriptorSize = gD3D12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHeapStart = gSwapChainRTVHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < 2; i++) {
		gSwapChain->GetBuffer(i, IID_PPV_ARGS(&gColorRTs[i]));
		D3D12_CPU_DESCRIPTOR_HANDLE rtvPointer;
		rtvPointer.ptr = rtvHeapStart.ptr + i * gRTVDescriptorSize;
		gD3D12Device->CreateRenderTargetView(gColorRTs[i], nullptr, rtvPointer);
	}
	D3D12_DEPTH_STENCIL_VIEW_DESC d3dDSViewDesc = {};
	d3dDSViewDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dDSViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	gD3D12Device->CreateDepthStencilView(gDSRT, &d3dDSViewDesc, gSwapChainDSVHeap->GetCPUDescriptorHandleForHeapStart());

	return true;
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
	// ע��regiter
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
	// ����create
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
	// ��ʾshow
	InitD3D12(hwnd, 1280, 720);
	ShowWindow(hwnd, inShowCmd);
	UpdateWindow(hwnd);

	MSG msg;
	while (true) {
		// ���԰��ڴ�����0
		ZeroMemory(&msg, sizeof(MSG));
		// ��ϵͳ�в����������ã��ᵼ��ϵͳ�ܿ�����Ϊ��Ϣ������ʱ
		if (PeekMessage(&msg,NULL,0,0,PM_REMOVE)) {
			// ���������⴦��
			if (msg.message == WM_QUIT) {
				break;
			}
			// ת����Ӧ�ó�����ʶ�����Ϣ
			TranslateMessage(&msg);
			// ֱ�ӵ���WindowProc
			DispatchMessage(&msg);
		} else {
			// rendering
		}
	}
	return 0;
}