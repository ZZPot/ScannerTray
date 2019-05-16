#pragma once
//#define _WIN32_IE 0x0600
#include <windows.h>
#include <tchar.h>
#include <vector>
#include <map>

#define DEFAULT_BLOON_TITLE _T("Info")
#define DEFAULT_ANIM_RATE	16

class TrayIcon
{
public:
	TrayIcon(HWND hWnd, UINT uMessage, UINT uID, HINSTANCE hInst = NULL, UINT uIcon = 0, LPCTSTR tTip = nullptr, BOOL bConstant = FALSE, HMENU hMenu = NULL);
	~TrayIcon();
	VOID Minimize();
	VOID Restore();
	VOID Switch();
	VOID ShowBaloon(LPCTSTR info, LPCTSTR title = nullptr);
	INT  ShowMenu(int x, int y);
	void Animate(std::vector<HICON> icons, bool loop = false);
	bool StartAnim();
	bool StopAnim();
	bool Check(unsigned id);
	bool Uncheck(unsigned id);
protected:
	HWND _hWnd;
	NOTIFYICONDATA _nid;
	LPCTSTR _tTip;
	HMENU _hMenu;
	BOOL _bConstant;
	BOOL _bMinimized;
	int _anim_id;
};

class IconAnimator
{
public:
	IconAnimator();
	virtual ~IconAnimator();
public:
	static bool SetRate(unsigned new_rate);
	static int Animate(std::vector<HICON> icons, HICON default_icon, UINT icon_id, HWND owner, bool loop = false);
	static bool Start(int anim_id);
	static bool Pause(int anim_id);
	static bool Stop(int anim_id);
protected:
	static unsigned __stdcall AnimThreadFunc(PVOID arg);
	static void UpdateAnimState();
	struct icon_anim
	{
		std::vector<HICON> icons;
		HICON default_icon;
		UINT icon_id;
		HWND owner;
		unsigned stage;
		bool active;
		bool loop;
	};
protected:
	static std::map<int, icon_anim> _anims;
	static unsigned _rate;
	static HANDLE _stop_event;
	static HANDLE _timer;
	static int _next_id;
private:
	static IconAnimator* _inst;
};