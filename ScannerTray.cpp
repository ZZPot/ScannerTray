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
std::vector<unsigned> GetCOMdevices();
HANDLE OpenComDevice(unsigned dev_id);
std::tstring MakeCOMName(unsigned dev_id);
//Read thread param
struct rtp
{
	~rtp() // just for handle this handles, lol
	{
		CloseHandle(hEndEvent);
		CloseHandle(hDevSelectEvent);
		if (ti != nullptr)
			delete ti;
	}
	HANDLE hEndEvent;
	HANDLE hDevSelectEvent;
	unsigned cur_dev;
	TrayIcon *ti;
};
bool SelectCOMdevice(unsigned dev_id, rtp* param);


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
	cmline.AddOption(_T("-dev"), true, _T("device"));
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
	/* - Добавить пункты меню с COM устройствами*/
	auto devices = GetCOMdevices();
	for(auto dev: devices)
	{
		AppendMenu(hPopupMenu, MF_STRING, IDC_COM + dev, MakeCOMName(dev).c_str());
	}
	AppendMenu(hPopupMenu, MF_STRING, IDC_EXIT, _T("Exit")); 

	//Будет удалено в ~rtp()
	TrayIcon* ti = new TrayIcon(hWnd, TRAY_MSG, TRAY_OBJ, GetModuleHandle(NULL), IDI_TRAY, _T("Barcode scanner"), TRUE, hPopupMenu);
	std::vector<HICON> icon_anim = LoadIcons(IDI_TRAY1, 8);
	ti->Animate(icon_anim);
	//Будет удалено в OnCloseMain()
	rtp* param = new rtp;
	param->hEndEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	param->hDevSelectEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	param->ti = ti;
	SetProp(hWnd, TRAY_PROP_NAME, (HANDLE)param);
	unsigned tid;
	BeginThreadEx(NULL, 0, ReadThreadFunc, param, 0, &tid);
	if (cmline.IsSet(_T("-dev")))
	{
		//Эмуляция выбора устройства
		unsigned selected_dev = cmline.GetInt(_T("-dev"));
		SelectCOMdevice(selected_dev, param);
	}
	return 0;
}
LRESULT OnTrayCommandMain(HWND hWnd, UINT uID, DWORD uMsg)
{
	if (uMsg == WM_LBUTTONUP)
	{
		rtp* rp = (rtp*)GetProp(hWnd, TRAY_PROP_NAME);
		if (rp != nullptr)
		{
			POINT cur;
			GetCursorPos(&cur);
			INT menu_sel = rp->ti->ShowMenu(cur.x, cur.y);
			if(menu_sel == IDC_EXIT)
				SendMessage(hWnd, WM_CLOSE, 0, 0);
			SelectCOMdevice(unsigned(menu_sel - IDC_COM), rp);
		}
	}
	// Тест анимации
	/*if (uMsg == WM_RBUTTONUP)
	{
	TrayIcon* ti = (TrayIcon*)GetProp(hWnd, TRAY_PROP_NAME);
	if (ti != nullptr)
	{
	ti->StartAnim();
	}
	}*/	
	return 0;
}
LRESULT OnCloseMain(HWND hWnd)
{
	rtp* rp = (rtp*)GetProp(hWnd, TRAY_PROP_NAME);
	if (rp != nullptr)
	{
		delete rp;
	}
	PostQuitMessage(0);
	return 0;
}
unsigned __stdcall ReadThreadFunc(PVOID arg)
{
	rtp* param = (rtp*)arg;
	HANDLE wait_pack[3] = { param->hEndEvent, param->hDevSelectEvent, NULL };
	DWORD wait_res;
	HANDLE com_port = INVALID_HANDLE_VALUE;
	while (com_port == INVALID_HANDLE_VALUE)
	{
		wait_res = WaitForMultipleObjects(2, wait_pack, false, INFINITE);
		if(wait_res == WAIT_OBJECT_0) // closed without making any action
		{
			return 0;
		}
		com_port = OpenComDevice(param->cur_dev);
	}
	DWORD bytes_read = 0;
	char data[BYTES_TO_READ_COM1 + 1];
	memset(data, 0, BYTES_TO_READ_COM1 + 1);
	OVERLAPPED ovr;
	ZeroMemory(&ovr, sizeof(ovr));
	ovr.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
	wait_pack[2] = ovr.hEvent; // full pack is ready
	ReadFile(com_port, &data, BYTES_TO_READ_COM1, &bytes_read, &ovr);
	while (1)
	{
		wait_res = WaitForMultipleObjects(3, wait_pack, false, INFINITE);
		if (wait_res == WAIT_OBJECT_0)
		{
			break;
		}
		if (wait_res == WAIT_OBJECT_0 + 2)
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
						param->ti->StartAnim();
						param->ti->ShowBaloon(str.c_str());
						str = cmline.GetString(_T("-pref")) + str + cmline.GetString(_T("-post"));
						SendString(str);
						str = _T("");
					}
				}
			}
		}
		if (wait_res == WAIT_OBJECT_0 + 1)
		{
			HANDLE com_port_tmp = OpenComDevice(param->cur_dev);
			if (com_port_tmp == INVALID_HANDLE_VALUE)
			{
				param->ti->ShowBaloon(_T("Can't select device"));
			}
			else
			{
				CloseHandle(ovr.hEvent);
				CloseHandle(com_port);
				com_port = com_port_tmp;
				ovr.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
				wait_pack[2] = ovr.hEvent;
			}
		}
		ReadFile(com_port, &data, BYTES_TO_READ_COM1, &bytes_read, &ovr);
	}
	CloseHandle(ovr.hEvent);
	CloseHandle(com_port);
	return 0;
}

void SendString(std::tstring str)
{
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
std::vector<unsigned> GetCOMdevices()
{
	std::vector<unsigned> res;
	for (unsigned i = 1; i<256; i++)
	{
		HANDLE dev = CreateFile(MakeCOMName(i).c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
		if (dev == INVALID_HANDLE_VALUE)
		{
			DWORD dwError = GetLastError();
			if ((dwError != ERROR_ACCESS_DENIED) &&
				(dwError != ERROR_GEN_FAILURE) && 
				(dwError != ERROR_SHARING_VIOLATION) && 
				(dwError != ERROR_SEM_TIMEOUT))
			{
				continue;
			}
		}
		res.push_back(i);
		CloseHandle(dev);
	}
	return res;
}
HANDLE OpenComDevice(unsigned dev_id)
{
#pragma region com handle
	// Нужен FILE_FLAG_OVERLAPPED для реального порта
	// Через переходник FILE_FLAG_OVERLAPPED не работает, считыавение происходит постоянно.
	// Проблема в определении типа устройства
	// Определяю тип устройства по его номеру (нет информации)
	DWORD flags;
	if(dev_id < 3)
		flags = FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED;
	else
		flags = FILE_ATTRIBUTE_NORMAL;
	HANDLE com_port = CreateFile(MakeCOMName(dev_id).c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, flags, nullptr);
	if (com_port == INVALID_HANDLE_VALUE)
	{
		return com_port;
	}
	DCB serial_params = { 0 };
	serial_params.DCBlength = sizeof(DCB);
	if (!GetCommState(com_port, &serial_params))
	{
		CloseHandle(com_port);
		return INVALID_HANDLE_VALUE;
	}
	serial_params.BaudRate = CBR_9600;
	serial_params.ByteSize = 8;
	serial_params.StopBits = ONESTOPBIT;
	serial_params.Parity = NOPARITY;
	if (!SetCommState(com_port, &serial_params))
	{
		CloseHandle(com_port);
		return INVALID_HANDLE_VALUE;
	}
	COMMTIMEOUTS timeouts = { 0 };
	timeouts.ReadIntervalTimeout = 300;
	SetCommTimeouts(com_port, &timeouts);
#pragma endregion	
	return com_port;
}
std::tstring MakeCOMName(unsigned dev_id)
{
	TCHAR dev_name[DEV_NAME_SIZE];
	_stprintf_s(dev_name, DEV_NAME_SIZE, _T("COM%d"), dev_id);
	return dev_name;
}
bool SelectCOMdevice(unsigned dev_id, rtp* param)
{
	bool res = false;
	if (dev_id <= 255 && dev_id >= 1)
	{
		param->ti->Uncheck(param->cur_dev + IDC_COM);
		param->cur_dev = dev_id;
		param->ti->Check(dev_id + IDC_COM);
		SetEvent(param->hDevSelectEvent);
		res = true;
	}
	return res;
}