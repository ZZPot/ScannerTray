#include "TrayIcon.h"
#include <tchar.h>
#include "common.h"
#pragma warning(disable: 4996 4267)

TrayIcon::TrayIcon(HWND hWnd, UINT uMessage, UINT uID, HINSTANCE hInst, UINT uIcon, LPCTSTR tTip, BOOL bConstant, HMENU hMenu):
	_hWnd(hWnd), _bConstant(bConstant), _hMenu(hMenu), _anim_id(-1)
{
	memset(&_nid, 0, sizeof(_nid));
	_nid.cbSize = sizeof(_nid);
	_nid.uFlags = NIF_MESSAGE;
	_nid.hWnd = _hWnd;
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
VOID TrayIcon::ShowBaloon(LPCTSTR info, LPCTSTR title)
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
void TrayIcon::Animate(std::vector<HICON> icons, bool loop)
{
	_anim_id = IconAnimator::Animate(icons, _nid.hIcon, _nid.uID, _hWnd);

}
bool TrayIcon::StartAnim()
{
	if (_anim_id == -1)
		return false;
	return IconAnimator::Start(_anim_id);
}
bool TrayIcon::StopAnim()
{
	if (_anim_id == -1)
		return false;
	return IconAnimator::Stop(_anim_id);
}
bool  TrayIcon::Check(unsigned id)
{
	MENUITEMINFO mii;
	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STATE;
	mii.fType = MIIM_STRING;
	mii.fState = MFS_CHECKED;
	return SetMenuItemInfo(_hMenu, id, false, &mii);
}
bool TrayIcon::Uncheck(unsigned id)
{
	MENUITEMINFO mii;
	memset(&mii, 0, sizeof(mii));
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STATE;
	mii.fType = MIIM_STRING;
	mii.fState = MFS_UNCHECKED;
	return SetMenuItemInfo(_hMenu, id, false, &mii);
}
IconAnimator::IconAnimator()
{
	_stop_event = CreateEvent(nullptr, TRUE, FALSE, nullptr);
	_timer = CreateWaitableTimer(nullptr, FALSE, nullptr);
	SetRate(_rate);
	unsigned tid;
	BeginThreadEx(NULL, 0, IconAnimator::AnimThreadFunc, nullptr, 0, &tid);
}
IconAnimator::~IconAnimator()
{
	SetEvent(_stop_event);
	CloseHandle(_stop_event);
	CloseHandle(_timer);
}
bool IconAnimator::SetRate(unsigned new_rate)
{
	if (_timer == NULL)
		return false;
	if (new_rate <= 0)
		return false;
	_rate = new_rate;
	LARGE_INTEGER li;
	li.QuadPart = 0;
	SetWaitableTimer(_timer, &li, 1000 / new_rate, nullptr, nullptr, false);
	return true;
}
int IconAnimator::Animate(std::vector<HICON> icons, HICON default_icon, UINT icon_id, HWND owner, bool loop)
{
	if (_inst == nullptr)
		_inst = new IconAnimator;
	int res = _next_id++;
	_anims[res].icons = icons;
	_anims[res].icon_id = icon_id;
	_anims[res].owner = owner;
	_anims[res].default_icon = default_icon;
	_anims[res].active = false;
	_anims[res].loop = loop;
	_anims[res].stage = 0;
	return res;
}
bool IconAnimator::Start(int anim_id)
{
	if (_inst == nullptr)
		return false;
	auto anim = _anims.find(anim_id);
	if (anim != _anims.end())
	{
		anim->second.active = true;
		return true;
	}
	return false;
}
bool IconAnimator::Pause(int anim_id)
{
	if (_inst == nullptr)
		return false;
	auto anim = _anims.find(anim_id);
	if (anim != _anims.end())
	{
		anim->second.active = false;
		return true;
	}
	return false;
}
bool IconAnimator::Stop(int anim_id)
{
	if (_inst == nullptr)
		return false;
	auto anim = _anims.find(anim_id);
	if (anim != _anims.end())
	{
		anim->second.active = false;
		anim->second.stage = 0;
		return true;
	}
	return false;
}
void IconAnimator::UpdateAnimState()
{
	if (_inst == nullptr)
		return;
	NOTIFYICONDATA nid;
	memset(&nid, 0, sizeof(nid));
	nid.cbSize = sizeof(nid);
	nid.uFlags = NIF_ICON;
	if (!_anims.size()) // some crap error on for-loop
		return;
	for (auto& anim : _anims)
	{
		if (!anim.second.active)
			continue;
		unsigned anim_size = anim.second.icons.size();
		nid.hWnd = anim.second.owner;
		nid.uID = anim.second.icon_id;
		bool reset = false;
		if (anim.second.stage >= anim_size)
		{
			anim.second.stage = 0;
			if (!anim.second.loop)
				reset = true;
		}
		if (reset)
		{
			nid.hIcon = anim.second.default_icon;
			anim.second.active = false;
		}
		else
		{
			unsigned icon_num = anim.second.stage++;
			nid.hIcon = anim.second.icons[icon_num];
		}
		Shell_NotifyIcon(NIM_MODIFY, &nid);
	}
}
unsigned __stdcall IconAnimator::AnimThreadFunc(PVOID arg)
{
	HANDLE wait_duo[2] = { _stop_event, _timer};
	while (1)
	{
		DWORD wait = WaitForMultipleObjects(2, wait_duo, FALSE, INFINITE);
		if (wait == 0)
			break;
		UpdateAnimState();
	}
	return 0;
}

std::map<int, IconAnimator::icon_anim> IconAnimator::_anims;
HANDLE IconAnimator::_stop_event = NULL;
HANDLE IconAnimator::_timer = NULL;
unsigned IconAnimator::_rate = DEFAULT_ANIM_RATE;
int IconAnimator::_next_id = 1;

IconAnimator* IconAnimator::_inst = nullptr;