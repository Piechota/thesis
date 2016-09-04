#include "pch.h"
#include "input.h"
#include <Windows.h>
#include "render.h"
#include "camera.h"
#include "timer.h"

HWND GHWnd;
UINT const GWidth = 960;
UINT const GHeight = 540;
CInputManager GInputManager;
CCamera GCamera;
CTimer GTimer;
CSystemInput GSystemInput;

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

	GCamera.SetPerspective(45.f * MathConsts::DegToRad, (float)GWidth / (float)GHeight, 0.01f, 400.f);

	CRender render;
	render.Init();

	GInputManager.SetHWND(GHWnd);

	GInputManager.Init();
	GInputManager.AddObserver(&GSystemInput);

	float timeToRender = 0.f;
	MSG msg = { 0 };
	bool run = true;
	while (run)
	{
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
			if (msg.message == WM_QUIT)
			{
				run = false;
				break;
			}
		}
		GTimer.Tick();
		GCamera.Update();

		timeToRender -= GTimer.Delta();
		if (timeToRender < 0.f)
		{
			render.DrawFrame();
			timeToRender = 1.f / 60.f;
		}
	}

	render.Release();
	return 0;
}