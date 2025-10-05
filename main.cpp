#include <windows.h>


LPCTSTR gWindowsClassName = L"BattleFire";//ASCII
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
	ShowWindow(hwnd, inShowCmd);
	UpdateWindow(hwnd);

	MSG msg;
	while (true) {
		// 可以吧内存置于0
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
		}
	}
	return 0;
}