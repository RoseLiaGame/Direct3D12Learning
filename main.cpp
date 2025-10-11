#include <windows.h>
#include "Direct3D.h"
#include "StaticMeshComponent.h"

#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

LPCTSTR gWindowsClassName = L"RoseGame";//ASCII

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
	ID3D12GraphicsCommandList* commandList = GetCommandList();
	ID3D12CommandAllocator* commandAllocator = GetCommandAllocator();
	StaticMeshComponent staticMeshComponent;
	staticMeshComponent.InitFromFile(commandList, "Res/Model/Sphere.lhsm");

	ID3D12RootSignature* rootSignature = InitRootSignature();
	D3D12_SHADER_BYTECODE vs, ps;
	CreateShaderFromFile(L"Res/Shader/ndctriangle.hlsl", "MainVS", "vs_5_0", &vs);
	CreateShaderFromFile(L"Res/Shader/ndctriangle.hlsl", "MainPS", "ps_5_0", &ps);
	ID3D12PipelineState* pso = CreatePSO(rootSignature, vs, ps);
	ID3D12Resource* cb = CreateConstantBufferObject(65536);//1024x64
	DirectX::XMMATRIX projectionMatrix = DirectX::XMMatrixPerspectiveFovLH(
		(45.0f*3.141592f)/180.0f, 1280.0f / 720.0f, 0.1f, 1000.0f);
	DirectX::XMMATRIX viewMatrix = DirectX::XMMatrixIdentity();
	DirectX::XMMATRIX modelMatrix = DirectX::XMMatrixTranslation(0.0f,0.0f,5.0f);
	modelMatrix *= DirectX::XMMatrixRotationZ(90.0f*3.141592f/180.0f);
	DirectX::XMFLOAT4X4 tempMatrix;
	float matrices[64];
	DirectX::XMStoreFloat4x4(&tempMatrix, projectionMatrix);
	memcpy(matrices, &tempMatrix, sizeof(float) * 16);
	DirectX::XMStoreFloat4x4(&tempMatrix, viewMatrix);
	memcpy(matrices + 16, &tempMatrix, sizeof(float) * 16);
	DirectX::XMStoreFloat4x4(&tempMatrix, modelMatrix);
	memcpy(matrices + 32, &tempMatrix, sizeof(float) * 16);
	DirectX::XMVECTOR determinant;
	DirectX::XMMATRIX inverseModelMatrix = DirectX::XMMatrixInverse(&determinant,modelMatrix);
	if (DirectX::XMVectorGetX(determinant) != 0.0f) {
		DirectX::XMMATRIX normalMatrix = DirectX::XMMatrixTranspose(inverseModelMatrix);

		DirectX::XMStoreFloat4x4(&tempMatrix, modelMatrix);
		memcpy(matrices + 48, &tempMatrix, sizeof(float) * 16);
	}

	UpdateConstantBuffer(cb, matrices, sizeof(float) * 64);
	EndCommandList();
	WaitForCompletionOfCommandList();
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
			commandAllocator->Reset();
			commandList->Reset(commandAllocator, nullptr);
			BeginRenderTOSwapChain(commandList);
			//draw
			commandList->SetPipelineState(pso);
			commandList->SetGraphicsRootSignature(rootSignature);
			commandList->SetGraphicsRootConstantBufferView(0, cb->GetGPUVirtualAddress());
			commandList->SetGraphicsRoot32BitConstants(1, 4, color, 0);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			staticMeshComponent.Render(commandList);
			
			EndRenderToSwapChain(commandList);
			EndCommandList();
			SwapD3D12Buffers();
		}
	}
	return 0;
}