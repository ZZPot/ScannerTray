#include "TrayIcon.h"
#include <tchar.h>
#pragma warning(disable: 4996)

TrayIcon::TrayIcon(HWND hWnd, HWND hWndOwner, UINT uMessage, UINT uID, HINSTANCE hInst, UINT uIcon, LPCTSTR tTip, BOOL bConstant, HMENU hMenu):
	_hWnd(hWnd), _bConstant(bConstant), _hMenu(hMenu)
{
	memset(&_nid, 0, sizeof(_nid));
	_nid.cbSize = sizeof(_nid);
	_nid.uFlags = NIF_MESSAGE;
	_nid.hWnd = hWndOwner;
	_nid.uID = uID;
	//_nid.uTimeout = 2000;
	_nid.uCallbackMessage = uMessage;
	_nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
	_tcsncpy_s(_nid.szInfoTitle, 64, DEFAULT_BLOON_TITLE, min(63, _tcslen(DEFAULT_BLOON_TITLE)));
	//nid.uVersion = NOTIFYICON_VERSION;
	if(hInst && uIcon)
	{
		_nid.uFlags |= NIF_ICON;
		_nid.hIcon = (HICON)LoadImage(hInst, MAKEINTRESOURCE(uIcon), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR | LR_DEFAULTSIZE);
	}
	if(tTip != nullptr)
	{
		_nid.uFlags |= NIF_TIP;
		_tcsncpy_s(_nid.szTip, 128, tTip, min(63, _tcslen(tTip)));
	}
	if (_bConstant)
	{
		Shell_NotifyIcon(NIM_ADD, &_nid);
	}
	_bMinimized = FALSE;
}
TrayIcon::~TrayIcon()
{
	Restore();
	if(_bConstant)
		Shell_NotifyIcon(NIM_DELETE, &_nid);
	DestroyIcon(_nid.hIcon);
}
VOID TrayIcon::Minimize()
{
	if(!_bConstant)
		Shell_NotifyIcon(NIM_ADD, &_nid);
	ShowWindow(_hWnd, SW_HIDE);
	_bMinimized = TRUE;
}
VOID TrayIcon::Restore()
{
	if (!_bConstant)
		Shell_NotifyIcon(NIM_DELETE, &_nid);
	ShowWindow(_hWnd, SW_SHOW);
	_bMinimized = FALSE;
}
VOID TrayIcon::Switch()
{
	if (_bMinimized)
		Restore();
	else
		Minimize();
}
VOID TrayIcon::ShowPopup(LPCTSTR info, LPCTSTR title)
{
	//NEED TO FIX THESE HARDCODED STRING LENGTH VALUES VVVVVV
	_nid.uFlags |= NIF_INFO;
	if(title != nullptr)
		_tcsncpy_s(_nid.szInfoTitle, 64, title, min(63, _tcslen(title)));
	else
		_tcsncpy_s(_nid.szInfoTitle, 64, DEFAULT_BLOON_TITLE, min(63, _tcslen(DEFAULT_BLOON_TITLE)));
	_tcsncpy_s(_nid.szInfo, 256, info, min(255, _tcslen(info)));
	Shell_NotifyIcon(NIM_MODIFY, &_nid);
	_nid.uFlags ^= NIF_INFO;
}
INT TrayIcon::ShowMenu(int x, int y)
{
	if (_hMenu == NULL)
		return 0;
	SetForegroundWindow(_nid.hWnd);
	return TrackPopupMenu(_hMenu, TPM_RETURNCMD | TPM_NONOTIFY, x, y, 0, _hWnd, nullptr);
}