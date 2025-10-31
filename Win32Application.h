#pragma once

#include <string>

#include "SHRRenderEngine.h"

class SHRRenderEngine;

class Win32Application
{
public:
	static int LaunchWindow(HINSTANCE hInstance, int nCmdShow);
	static HWND GetWindowHandle() { return hwnd; }

protected:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

private:
	static HWND hwnd;
};