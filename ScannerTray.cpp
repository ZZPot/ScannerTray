#include <windows.h>
#include <tchar.h>
#include "defs.h"
#include "resource.h"
#include "TrayIcon\TrayIcon.h"
#include "common.h"


LRESULT CALLBACK MainWndProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT OnCreateMain(HWND hDlg, WPARAM wParam, LPARAM lParam);
LRESULT OnTrayCommandMain(HWND hDlg, UINT uID, DWORD uMsg);
LRESULT OnCloseMain(HWND hDlg);
unsigned __stdcall ReadThreadFunc(PVOID arg);
struct rtp
{
	HANDLE hEndEvent;
	TrayIcon *ti;
};
INT WINAPI _tWinMain(HINSTANCE hInst, HINSTANCE, LPTSTR ptCmdLine, int nCmdShow)
{
	HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, SCAN_PROC_EVENT);
	DWORD err = GetLastError();
	if (hEvent && (err == ERROR_ALREADY_EXISTS)) // one process only
	{
		CloseHandle(hEvent);
		return 1;
	}
#pragma region creating window
	WNDCLASS wnd_class;
	ZeroMemory(&wnd_class, sizeof(wnd_class));
	wnd_class.style = 0;
	wnd_class.lpfnWndProc = MainWndProc;
	wnd_class.hInstance = hInst;
	wnd_class.lpszClassName = MAIN_WINDOW_CNAME;
	RegisterClass(&wnd_class);
	CreateWindow(MAIN_WINDOW_CNAME, _T(""), 0, 0, 0, 50, 50, NULL, NULL, NULL, 0);
#pragma endregion
	
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	CloseHandle(hEvent);
	return msg.lParam;
};

LRESULT CALLBACK MainWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_CREATE:
		return OnCreateMain(hWnd, wParam, lParam);
	case TRAY_MSG:
		return OnTrayCommandMain(hWnd, (UINT)wParam, (DWORD)lParam);
	case WM_CLOSE:
		return OnCloseMain(hWnd);
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}
LRESULT OnCreateMain(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HMENU hPopupMenu = CreatePopupMenu();
	AppendMenu(hPopupMenu, MF_STRING, IDC_EXIT, _T("Exit"));

	TrayIcon* ti = new TrayIcon(hWnd, hWnd, TRAY_MSG, TRAY_OBJ, GetModuleHandle(NULL), IDI_TRAY, _T("Barcode scanner"), TRUE, hPopupMenu);
	SetProp(hWnd, TRAY_PROP_NAME, (HANDLE)ti);
	rtp* param = new rtp;
	param->hEndEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	param->ti = ti;
	unsigned tid;
	BeginThreadEx(NULL, 0, ReadThreadFunc, param, 0, &tid);
	return 0;
}
LRESULT OnTrayCommandMain(HWND hWnd, UINT uID, DWORD uMsg)
{
	if (uMsg == WM_LBUTTONUP)
	{
		TrayIcon* ti = (TrayIcon*)GetProp(hWnd, TRAY_PROP_NAME);
		if (ti != nullptr)
		{
			POINT cur;
			GetCursorPos(&cur);
			if (ti->ShowMenu(cur.x, cur.y) == IDC_EXIT)
				SendMessage(hWnd, WM_CLOSE, 0, 0);
		}
	}
	if (uMsg == WM_RBUTTONUP)
	{
		TrayIcon* ti = (TrayIcon*)GetProp(hWnd, TRAY_PROP_NAME);
		if (ti != nullptr)
		{
			ti->ShowPopup(_T("RMB pressed"));
		}
	}
	return 0;
}
LRESULT OnCloseMain(HWND hWnd)
{
	EndDialog(hWnd, 0);
	TrayIcon* ti = (TrayIcon*)GetProp(hWnd, TRAY_PROP_NAME);
	if (ti != nullptr)
		delete ti;
	PostQuitMessage(0);
	return 0;
}
unsigned __stdcall ReadThreadFunc(PVOID arg)
{
	rtp* param = (rtp*)arg;
	OVERLAPPED ovr;
	ZeroMemory(&ovr, sizeof(ovr));
	ovr.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	HANDLE wait_duo[2] = { param->hEndEvent, ovr.hEvent};
	//CreateFile
	while (WaitForMultipleObjects(2, wait_duo, false, INFINITE) != WAIT_OBJECT_0)
	{
		//ReadFile
	}
	delete param;
	return 0;
}