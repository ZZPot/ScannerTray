#pragma once
//#define _WIN32_IE 0x0600
#include <windows.h>
#include <tchar.h>

#define DEFAULT_BLOON_TITLE _T("Info")

class TrayIcon
{
public:
	TrayIcon(HWND hWnd, HWND hWndOwner, UINT uMessage, UINT uID, HINSTANCE hInst = NULL, UINT uIcon = 0, LPCTSTR tTip = nullptr, BOOL bConstant = FALSE, HMENU hMenu = NULL);
	~TrayIcon();
	VOID Minimize();
	VOID Restore();
	VOID Switch();
	VOID ShowPopup(LPCTSTR info, LPCTSTR title = nullptr);
	INT  ShowMenu(int x, int y);
protected:
	HWND _hWnd;
	NOTIFYICONDATA _nid;
	LPCTSTR _tTip;
	HMENU _hMenu;
	BOOL _bConstant;
	BOOL _bMinimized;
};