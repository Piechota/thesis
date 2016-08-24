#include "pch.h"
#include "input.h"
#include <Windows.h>

HWND GHWnd;
UINT const GWidth = 960;
UINT const GHeight = 540;
CInputManager GInputManager;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, INT nCmdShow)
{
	WNDCLASS windowClass = { 0 };
	/*
	CS_HREDRAW - Redraws the entire window if a movement or size adjustment changes the width of the client area.
	CS_VREDRAW - Redraws the entire window if a movement or size adjustment changes the height of the client area.
	*/
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = CInputManager::WindowProc;
	windowClass.hInstance = hInstance;
	windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	windowClass.lpszClassName = L"WindowClass";
	windowClass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);

	if (!RegisterClass(&windowClass))
	{
		MessageBox(0, L"RegisterClass FAILED", 0, 0);
	}

	GHWnd = CreateWindow(
		L"WindowClass",
		L"Engine",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		GWidth, GHeight,
		NULL, NULL,
		hInstance,
		NULL);

	if (GHWnd == 0)
	{
		MessageBox(0, L"CreateWindow FAILED", 0, 0);
	}

	ShowWindow(GHWnd, nCmdShow);

	GInputManager.SetHWND(GHWnd);

	GInputManager.Init();

	return 0;
}