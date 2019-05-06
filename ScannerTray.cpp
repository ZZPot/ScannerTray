#include <windows.h>
#include <tchar.h>
#include "defs.h"
#include "resource.h"
#include "TrayIcon\TrayIcon.h"
#include "common.h"
#include <algorithm>

LRESULT CALLBACK MainWndProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT OnCreateMain(HWND hDlg, WPARAM wParam, LPARAM lParam);
LRESULT OnTrayCommandMain(HWND hDlg, UINT uID, DWORD uMsg);
LRESULT OnCloseMain(HWND hDlg);
unsigned __stdcall ReadThreadFunc(PVOID arg);
void SendString(std::tstring str);
std::vector<HICON> LoadIcons(UINT initial, unsigned count); // All resources should be "packed"
struct rtp
{
	HANDLE hEndEvent;
	TrayIcon *ti;
};

CmdLine cmline;

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
	cmline.AddOption(_T("-pref"), true, _T("prefix"));
	cmline.AddOption(_T("-post"), true, _T("postfix"));
	cmline.SetCmd(ptCmdLine);
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

	TrayIcon* ti = new TrayIcon(hWnd, TRAY_MSG, TRAY_OBJ, GetModuleHandle(NULL), IDI_TRAY, _T("Barcode scanner"), TRUE, hPopupMenu);
	std::vector<HICON> icon_anim = LoadIcons(IDI_TRAY1, 8);
	ti->Animate(icon_anim);
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
	static bool menu_visible = false;
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
			ti->StartAnim();
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
#pragma region com handle
	// Нужен FILE_FLAG_OVERLAPPED для реального порта
	// Через переходник FILE_FLAG_OVERLAPPED не работает, считыавение происходит постоянно.
	HANDLE com_port = CreateFile(_T("COM5"), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	if (com_port == INVALID_HANDLE_VALUE)
	{
		param->ti->ShowBaloon(_T("Can't open 'COM5'"));
		return 1;
	}
	DCB serial_params = { 0 };
	serial_params.DCBlength = sizeof(DCB);
	if (!GetCommState(com_port, &serial_params))
	{
		param->ti->ShowBaloon(_T("Getting state error"));
		CloseHandle(com_port);
		return 1;
	}
	serial_params.BaudRate = CBR_9600;
	serial_params.ByteSize = 8;
	serial_params.StopBits = ONESTOPBIT;
	serial_params.Parity = NOPARITY;
	if (!SetCommState(com_port, &serial_params))
	{
		param->ti->ShowBaloon(_T("Setting state error"));
		return 1;
	}
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 300;
	SetCommTimeouts(com_port, &timeouts);
#pragma endregion	
	OVERLAPPED ovr;
	ZeroMemory(&ovr, sizeof(ovr));
	ovr.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	HANDLE wait_duo[2] = { param->hEndEvent, ovr.hEvent };
	
	DWORD bytes_read = 0;
	char data[BYTES_TO_READ_COM1 + 1];
	data[BYTES_TO_READ_COM1] = '\0';
	memset(data, 0, BYTES_TO_READ_COM1 + 1);
	ReadFile(com_port, &data, BYTES_TO_READ_COM1, &bytes_read, &ovr);
	while (WaitForMultipleObjects(2, wait_duo, false, INFINITE) != WAIT_OBJECT_0)
	{
		static std::tstring str = _T("");
		if (bytes_read && data[0] != '\0xa')
		{
			if(str.length() == 16)
			{
				GetLastError();
			}
			bool more_data = (data[bytes_read-1] != '\n');
			data[bytes_read] = '\0';
			str += CharToTchar(data, CP_ACP);
			if(!more_data)
			{
				str.erase(std::remove(str.begin(), str.end(), _T('\r')), str.end());
				str.erase(std::remove(str.begin(), str.end(), _T('\n')), str.end());
				if(str.length() != 0)
				{
					param->ti->ShowBaloon(str.c_str());
					str = cmline.GetString(_T("-pref")) + str + cmline.GetString(_T("-post"));
					SendString(str);
					str = _T("");
				}
			}
		}
		ReadFile(com_port, &data, BYTES_TO_READ_COM1, &bytes_read, &ovr);
	}
	CloseHandle(com_port);
	delete param;
	return 0;
}

void SendString(std::tstring str)
{
	HKL layout = LoadKeyboardLayout(_T("00000409"), KLF_ACTIVATE | KLF_NOTELLSHELL);
	size_t len = str.length();
	std::vector<INPUT> inp_v;
	for(unsigned i = 0; i < len; i++)
	{
		MakeInputSeq(str[i], &inp_v);
	}
	SendInput(inp_v.size(), inp_v.data(), sizeof(INPUT));
}
std::vector<HICON> LoadIcons(UINT initial, unsigned count)
{
	std::vector<HICON> res;
	HINSTANCE inst = GetModuleHandle(NULL);
	for (unsigned i = 0; i < count; i++)
	{
		res.push_back((HICON)LoadImage(inst, MAKEINTRESOURCE(initial + i), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR | LR_DEFAULTSIZE));
	}
	return res;
}