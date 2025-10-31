#include "SHRUtils.h"
#include "Win32Application.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
    return Win32Application::LaunchWindow(hInstance, nCmdShow);
}