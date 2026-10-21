/*
Copyright 2002 Ryan VanMiddlesworth

This file is part of Vitrite.

Vitrite is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

Vitrite is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Vitrite; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

// This app requires Windows 2000 or newer
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <tchar.h>
#include <strsafe.h>
#include <stdio.h>
#include <stdlib.h>
#include "resource.h"
#include "vitrite.h"
#include "VitriteReader.h"

// Globals
HINSTANCE  g_hInstance;
BOOL  g_bWindowShown;
HANDLE  ghMutex;
HWND  g_hMenuTargetWnd;
HWND  g_hMainWnd;
HWND  g_hSettingsWnd;

#define BRAND_COLOR RGB(170, 153, 255)
#define SUBTITLE_COLOR RGB(110, 110, 120)
#define SETTINGS_INI_NAME TEXT("vitrite.ini")
#define HOTKEY_MODIFIERS_DEFAULT (MOD_CONTROL | MOD_SHIFT)
#define INSTANCE_PROPERTY TEXT("Vitrite.Instance.v2")
#define COPYDATA_OPEN_TEXT_FILE 0x56545231

typedef struct APPSETTINGS {
	UINT opacityModifiers;
	UINT topmostModifiers;
	UINT topmostVk;
	BOOL runAtStartup;
} APPSETTINGS;

static APPSETTINGS g_settings;
static HBRUSH g_brandBrush;
static HBRUSH g_settingsBackgroundBrush;
static HFONT g_hSettingsTitleFont;

#define TIMER_OPACITY_ANIM 1
#define OPACITY_ANIM_INTERVAL_MS 15
#define OPACITY_ANIM_STEPS 12

static BOOL g_hotkeyAlphaRegistered[10];
static BOOL g_hotkeyTopmostRegistered;
static BOOL g_hotkeyTopmostNumpadRegistered;
static UINT g_uTaskbarRestart; // Message ID for "TaskbarCreated"

// NOTIFYICON_VERSION_4 handling
#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif
#ifndef NIN_KEYSELECT
#define NIN_KEYSELECT (WM_USER + 1)
#endif

typedef struct WINDOWSTATE {
	HWND hwnd;
	BOOL hadLayered;
	BOOL captured;
	COLORREF prevColorKey;
	BYTE prevAlpha;
	DWORD prevFlags;
} WINDOWSTATE;

#define WINDOWSTATE_MAX 64
static WINDOWSTATE g_windowStates[WINDOWSTATE_MAX];
static int g_windowStateEvictIndex;

static BOOL g_animActive;
static HWND g_animOwnerWnd;
static HWND g_animTargetWnd;
static BYTE g_animFromAlpha;
static BYTE g_animToAlpha;
static int g_animStep;
static int g_animSteps;
static BOOL g_animClearStateAtEnd;
static int g_animEndAction; // 0=none, 1=remove-layered, 2=restore-prev
static WINDOWSTATE g_animRestore;

static void LoadResString(UINT id, LPTSTR buf, int cchBuf, LPCTSTR fallback) {
	int n;

	if (buf == NULL || cchBuf <= 0) {
		return;
	}
	buf[0] = TEXT('\0');

	n = LoadString(g_hInstance, id, buf, cchBuf);
	if (n <= 0 && fallback != NULL) {
		StringCchCopy(buf, cchBuf, fallback);
	}
}

static void GetSettingsPath(LPTSTR path, int cchPath) {
	DWORD length;

	if (path == NULL || cchPath <= 0) {
		return;
	}
	path[0] = TEXT('\0');
	length = GetEnvironmentVariable(TEXT("LOCALAPPDATA"), path, (DWORD)cchPath);
	if (length == 0 || length >= (DWORD)cchPath) {
		LPTSTR slash;
		GetModuleFileName(NULL, path, cchPath);
		slash = _tcsrchr(path, TEXT('\\'));
		if (slash != NULL) {
			*(slash + 1) = TEXT('\0');
		}
		StringCchCat(path, cchPath, SETTINGS_INI_NAME);
		return;
	}
	StringCchCat(path, cchPath, TEXT("\\Vitrite"));
	CreateDirectory(path, NULL);
	StringCchCat(path, cchPath, TEXT("\\") SETTINGS_INI_NAME);
}

static void SetDefaultSettings(APPSETTINGS* settings) {
	if (settings == NULL) {
		return;
	}
	settings->opacityModifiers = HOTKEY_MODIFIERS_DEFAULT;
	settings->topmostModifiers = HOTKEY_MODIFIERS_DEFAULT;
	settings->topmostVk = VK_OEM_PLUS;
	settings->runAtStartup = FALSE;
}

static void LoadSettings(void) {
	TCHAR path[MAX_PATH];
	SetDefaultSettings(&g_settings);
	GetSettingsPath(path, MAX_PATH);
	g_settings.opacityModifiers = (UINT)GetPrivateProfileInt(TEXT("Hotkeys"), TEXT("OpacityModifiers"),
		HOTKEY_MODIFIERS_DEFAULT, path);
	g_settings.topmostModifiers = (UINT)GetPrivateProfileInt(TEXT("Hotkeys"), TEXT("TopmostModifiers"),
		HOTKEY_MODIFIERS_DEFAULT, path);
	g_settings.topmostVk = (UINT)GetPrivateProfileInt(TEXT("Hotkeys"), TEXT("TopmostVk"), VK_OEM_PLUS, path);
	g_settings.runAtStartup = GetPrivateProfileInt(TEXT("General"), TEXT("RunAtStartup"), 0, path) != 0;
}

static void SaveSettings(void) {
	TCHAR path[MAX_PATH];
	TCHAR value[32];
	GetSettingsPath(path, MAX_PATH);
	StringCchPrintf(value, 32, TEXT("%u"), g_settings.opacityModifiers);
	WritePrivateProfileString(TEXT("Hotkeys"), TEXT("OpacityModifiers"), value, path);
	StringCchPrintf(value, 32, TEXT("%u"), g_settings.topmostModifiers);
	WritePrivateProfileString(TEXT("Hotkeys"), TEXT("TopmostModifiers"), value, path);
	StringCchPrintf(value, 32, TEXT("%u"), g_settings.topmostVk);
	WritePrivateProfileString(TEXT("Hotkeys"), TEXT("TopmostVk"), value, path);
	WritePrivateProfileString(TEXT("General"), TEXT("RunAtStartup"),
		g_settings.runAtStartup ? TEXT("1") : TEXT("0"), path);
}

static BOOL UpdateRunAtStartup(BOOL enabled) {
	HKEY key;
	TCHAR executable[MAX_PATH];
	TCHAR command[MAX_PATH + 32];
	LONG result;

	result = RegCreateKeyEx(HKEY_CURRENT_USER,
		TEXT("Software\\Microsoft\\Windows\\CurrentVersion\\Run"), 0, NULL, 0,
		KEY_SET_VALUE, NULL, &key, NULL);
	if (result != ERROR_SUCCESS) {
		return FALSE;
	}
	if (enabled) {
		GetModuleFileName(NULL, executable, MAX_PATH);
		StringCchPrintf(command, MAX_PATH + 32, TEXT("\"%s\" --startup"), executable);
		result = RegSetValueEx(key, TEXT("Vitrite"), 0, REG_SZ,
			(const BYTE*)command, (DWORD)((lstrlen(command) + 1) * sizeof(TCHAR)));
	} else {
		result = RegDeleteValue(key, TEXT("Vitrite"));
		if (result == ERROR_FILE_NOT_FOUND) {
			result = ERROR_SUCCESS;
		}
	}
	RegCloseKey(key);
	return result == ERROR_SUCCESS;
}

static BOOL CALLBACK FindVitriteInstanceProc(HWND hWnd, LPARAM lParam) {
	HWND* result = (HWND*)lParam;
	if (GetProp(hWnd, INSTANCE_PROPERTY) != NULL) {
		*result = hWnd;
		return FALSE;
	}
	return TRUE;
}

static HWND FindExistingVitriteInstance(void) {
	HWND result = NULL;
	EnumWindows(FindVitriteInstanceProc, (LPARAM)&result);
	return result;
}

static BOOL IsStartupLaunch(LPCTSTR commandLine) {
	if (commandLine == NULL) {
		return FALSE;
	}
	while (*commandLine == TEXT(' ') || *commandLine == TEXT('\t')) {
		commandLine++;
	}
	return lstrcmpi(commandLine, TEXT("--startup")) == 0 ||
		lstrcmpi(commandLine, TEXT("/startup")) == 0;
}

static void NotifyExistingVitriteInstance(HWND existing, LPCTSTR commandLine) {
	TCHAR path[MAX_PATH];
	COPYDATASTRUCT data;

	if (ExtractTextReaderPathFromCommandLine(commandLine, path, MAX_PATH)) {
		memset(&data, 0, sizeof(data));
		data.dwData = COPYDATA_OPEN_TEXT_FILE;
		data.cbData = (DWORD)((lstrlen(path) + 1) * sizeof(TCHAR));
		data.lpData = path;
		SendMessage(existing, WM_COPYDATA, 0, (LPARAM)&data);
	} else {
		PostMessage(existing, WM_COMMAND, MAKEWPARAM(IDM_READER, 0), 0);
	}
}

static void InitUiLanguage(void) {
	HMODULE hKernel32;
	LANGID lang;
	typedef LANGID (WINAPI *PFN_SetThreadUILanguage)(LANGID);
	PFN_SetThreadUILanguage pSetThreadUILanguage;

	hKernel32 = GetModuleHandle(TEXT("kernel32.dll"));
	if (hKernel32 == NULL) {
		return;
	}

	// Best-effort: set thread UI language to the user's default UI language so the
	// resource loader can pick the correct language (e.g., zh-CN vs en-US).
	lang = GetUserDefaultUILanguage();
	pSetThreadUILanguage = (PFN_SetThreadUILanguage)GetProcAddress(hKernel32, "SetThreadUILanguage");
	if (pSetThreadUILanguage != NULL) {
		pSetThreadUILanguage(lang);
	}
}

static void UnregisterAllHotkeys(HWND hWnd) {
	int i;
	for (i = 0; i <= 9; i++) {
		UnregisterHotKey(hWnd, HOTKEY_ALPHA_0 + i);
		g_hotkeyAlphaRegistered[i] = FALSE;
	}
	UnregisterHotKey(hWnd, HOTKEY_TOPMOST);
	UnregisterHotKey(hWnd, HOTKEY_TOPMOST_NUMPAD);
	g_hotkeyTopmostRegistered = FALSE;
	g_hotkeyTopmostNumpadRegistered = FALSE;
}

static BOOL RegisterConfiguredHotkeys(HWND hWnd, const APPSETTINGS* settings, BOOL showError) {
	int i;
	BOOL success;
	APPSETTINGS previous;
	TCHAR message[256];

	if (settings == NULL) {
		return FALSE;
	}
	previous = g_settings;
	UnregisterAllHotkeys(hWnd);
	success = TRUE;
	for (i = 0; i <= 9; i++) {
		if (RegisterHotKey(hWnd, HOTKEY_ALPHA_0 + i, settings->opacityModifiers, 0x30 + i)) {
			g_hotkeyAlphaRegistered[i] = TRUE;
		} else {
			success = FALSE;
			break;
		}
	}
	if (success && RegisterHotKey(hWnd, HOTKEY_TOPMOST,
		settings->topmostModifiers, settings->topmostVk)) {
		g_hotkeyTopmostRegistered = TRUE;
	} else {
		success = FALSE;
	}
	if (success && settings->topmostVk != VK_ADD &&
		RegisterHotKey(hWnd, HOTKEY_TOPMOST_NUMPAD, settings->topmostModifiers, VK_ADD)) {
		g_hotkeyTopmostNumpadRegistered = TRUE;
	}

	if (!success) {
		UnregisterAllHotkeys(hWnd);
		for (i = 0; i <= 9; i++) {
			if (RegisterHotKey(hWnd, HOTKEY_ALPHA_0 + i, previous.opacityModifiers, 0x30 + i)) {
				g_hotkeyAlphaRegistered[i] = TRUE;
			}
		}
		g_hotkeyTopmostRegistered = RegisterHotKey(hWnd, HOTKEY_TOPMOST,
			previous.topmostModifiers, previous.topmostVk);
		if (previous.topmostVk != VK_ADD) {
			g_hotkeyTopmostNumpadRegistered = RegisterHotKey(hWnd, HOTKEY_TOPMOST_NUMPAD,
				previous.topmostModifiers, VK_ADD);
		}
		if (showError) {
			StringCchCopy(message, 256,
				TEXT("The hotkey is already in use. Your previous settings were restored."));
			MessageBox(g_hSettingsWnd, message, TEXT("Vitrite"), MB_OK | MB_ICONWARNING);
		}
		return FALSE;
	}
	return TRUE;
}

static void StopOpacityAnimation(void) {
	if (g_animActive && g_animOwnerWnd != NULL) {
		KillTimer(g_animOwnerWnd, TIMER_OPACITY_ANIM);
	}
	g_animActive = FALSE;
	g_animOwnerWnd = NULL;
	g_animTargetWnd = NULL;
	g_animStep = 0;
	g_animSteps = 0;
	g_animClearStateAtEnd = FALSE;
	g_animEndAction = 0;
	memset(&g_animRestore, 0, sizeof(g_animRestore));
}

static void RemoveWindowState(HWND hWnd) {
	int i;
	for (i = 0; i < WINDOWSTATE_MAX; i++) {
		if (g_windowStates[i].hwnd == hWnd) {
			g_windowStates[i].hwnd = NULL;
			return;
		}
	}
}

static BYTE GetCurrentWindowAlpha(HWND hWnd) {
	COLORREF key;
	BYTE alpha;
	DWORD flags;

	key = 0;
	alpha = 255;
	flags = 0;
	if (GetLayeredWindowAttributes(hWnd, &key, &alpha, &flags) != 0) {
		if (flags & LWA_ALPHA) {
			return alpha;
		}
	}
	return 255;
}

static void TickOpacityAnimation(void) {
	LONG_PTR exStyle;
	BYTE alpha;
	int alphaInt;
	int step;
	int steps;

	if (!g_animActive || g_animOwnerWnd == NULL || g_animTargetWnd == NULL) {
		StopOpacityAnimation();
		return;
	}
	if (!IsWindow(g_animTargetWnd)) {
		if (g_animClearStateAtEnd) {
			RemoveWindowState(g_animTargetWnd);
		}
		StopOpacityAnimation();
		return;
	}

	step = g_animStep;
	steps = g_animSteps;
	if (steps <= 0) {
		steps = 1;
	}
	if (step > steps) {
		step = steps;
	}

	alphaInt = (int)g_animFromAlpha + ((int)g_animToAlpha - (int)g_animFromAlpha) * step / steps;
	if (alphaInt < 0) {
		alphaInt = 0;
	} else if (alphaInt > 255) {
		alphaInt = 255;
	}
	alpha = (BYTE)alphaInt;
	exStyle = GetWindowLongPtr(g_animTargetWnd, GWL_EXSTYLE);
	SetWindowLongPtr(g_animTargetWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
	SetLayeredWindowAttributes(g_animTargetWnd, 0, alpha, LWA_ALPHA);

	g_animStep++;
	if (g_animStep > g_animSteps) {
		if (g_animEndAction == 2) {
			exStyle = GetWindowLongPtr(g_animTargetWnd, GWL_EXSTYLE);
			SetWindowLongPtr(g_animTargetWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
			if (g_animRestore.captured) {
				SetLayeredWindowAttributes(g_animTargetWnd,
					g_animRestore.prevColorKey, g_animRestore.prevAlpha, g_animRestore.prevFlags);
			}
		} else if (g_animEndAction == 1) {
			exStyle = GetWindowLongPtr(g_animTargetWnd, GWL_EXSTYLE);
			SetWindowLongPtr(g_animTargetWnd, GWL_EXSTYLE, exStyle & ~WS_EX_LAYERED);
			RedrawWindow(g_animTargetWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE |
				RDW_FRAME | RDW_ALLCHILDREN);
		}

		if (g_animClearStateAtEnd) {
			RemoveWindowState(g_animTargetWnd);
		}
		StopOpacityAnimation();
	}
}

static void PruneWindowStates(void) {
	int i;
	for (i = 0; i < WINDOWSTATE_MAX; i++) {
		if (g_windowStates[i].hwnd != NULL && !IsWindow(g_windowStates[i].hwnd)) {
			g_windowStates[i].hwnd = NULL;
		}
	}
}

static WINDOWSTATE* FindWindowState(HWND hWnd) {
	int i;
	for (i = 0; i < WINDOWSTATE_MAX; i++) {
		if (g_windowStates[i].hwnd == hWnd) {
			return &g_windowStates[i];
		}
	}
	return NULL;
}

static WINDOWSTATE* GetOrCaptureWindowState(HWND hWnd) {
	WINDOWSTATE* state;
	int i;
	LONG_PTR exStyle;
	COLORREF key;
	BYTE alpha;
	DWORD flags;

	state = FindWindowState(hWnd);
	if (state != NULL) {
		return state;
	}

	for (i = 0; i < WINDOWSTATE_MAX; i++) {
		if (g_windowStates[i].hwnd == NULL) {
			state = &g_windowStates[i];
			break;
		}
	}
	if (state == NULL) {
		if (g_windowStateEvictIndex < 0 || g_windowStateEvictIndex >= WINDOWSTATE_MAX) {
			g_windowStateEvictIndex = 0;
		}
		state = &g_windowStates[g_windowStateEvictIndex++];
		if (g_windowStateEvictIndex >= WINDOWSTATE_MAX) {
			g_windowStateEvictIndex = 0;
		}
	}

	memset(state, 0, sizeof(*state));
	state->hwnd = hWnd;

	exStyle = GetWindowLongPtr(hWnd, GWL_EXSTYLE);
	state->hadLayered = ((exStyle & WS_EX_LAYERED) != 0);
	state->captured = FALSE;
	if (state->hadLayered) {
		key = 0;
		alpha = 255;
		flags = 0;
		if (GetLayeredWindowAttributes(hWnd, &key, &alpha, &flags) != 0) {
			state->prevColorKey = key;
			state->prevAlpha = alpha;
			state->prevFlags = flags;
			state->captured = TRUE;
		}
	}
	return state;
}

static void BeginOpacityAnimation(HWND hOwnerWnd, HWND hTargetWnd, BYTE toAlpha) {
	LONG_PTR exStyle;

	if (hOwnerWnd == NULL || hTargetWnd == NULL) {
		return;
	}
	StopOpacityAnimation();

	exStyle = GetWindowLongPtr(hTargetWnd, GWL_EXSTYLE);
	SetWindowLongPtr(hTargetWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

	g_animActive = TRUE;
	g_animOwnerWnd = hOwnerWnd;
	g_animTargetWnd = hTargetWnd;
	g_animFromAlpha = GetCurrentWindowAlpha(hTargetWnd);
	g_animToAlpha = toAlpha;
	g_animStep = 0;
	g_animSteps = OPACITY_ANIM_STEPS;
	g_animClearStateAtEnd = FALSE;
	g_animEndAction = 0;
	memset(&g_animRestore, 0, sizeof(g_animRestore));
	SetTimer(hOwnerWnd, TIMER_OPACITY_ANIM, OPACITY_ANIM_INTERVAL_MS, NULL);
}

static void BeginResetAnimation(HWND hOwnerWnd, HWND hTargetWnd) {
	WINDOWSTATE* state;
	LONG_PTR exStyle;

	if (hOwnerWnd == NULL || hTargetWnd == NULL) {
		return;
	}

	state = FindWindowState(hTargetWnd);
	if (state == NULL) {
		return;
	}

	StopOpacityAnimation();

	// If we modified a window that was already layered, restore its original attributes.
	if (state->hadLayered && state->captured) {
		// If the original state didn't use alpha, restore immediately to avoid changing flags mid-animation.
		if ((state->prevFlags & LWA_ALPHA) == 0) {
			exStyle = GetWindowLongPtr(hTargetWnd, GWL_EXSTYLE);
			SetWindowLongPtr(hTargetWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);
			SetLayeredWindowAttributes(hTargetWnd, state->prevColorKey, state->prevAlpha, state->prevFlags);
			RemoveWindowState(hTargetWnd);
			return;
		}

		g_animEndAction = 2;
		g_animRestore = *state;
		g_animToAlpha = state->prevAlpha;
	} else if (!state->hadLayered) {
		// We added WS_EX_LAYERED: fade back to opaque then remove WS_EX_LAYERED.
		g_animEndAction = 1;
		g_animToAlpha = 255;
	} else {
		// Best-effort: fade back to opaque and keep WS_EX_LAYERED (we couldn't capture the original state).
		g_animEndAction = 0;
		g_animToAlpha = 255;
	}

	exStyle = GetWindowLongPtr(hTargetWnd, GWL_EXSTYLE);
	SetWindowLongPtr(hTargetWnd, GWL_EXSTYLE, exStyle | WS_EX_LAYERED);

	g_animActive = TRUE;
	g_animOwnerWnd = hOwnerWnd;
	g_animTargetWnd = hTargetWnd;
	g_animFromAlpha = GetCurrentWindowAlpha(hTargetWnd);
	g_animStep = 0;
	g_animSteps = OPACITY_ANIM_STEPS;
	g_animClearStateAtEnd = TRUE;
	SetTimer(hOwnerWnd, TIMER_OPACITY_ANIM, OPACITY_ANIM_INTERVAL_MS, NULL);
}

static void TryEnableDarkModeTitleBar(HWND hWnd) {
	typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
	HMODULE hDwm;
	PFN_DwmSetWindowAttribute pDwmSetWindowAttribute;
	BOOL useDark;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1
#define DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 19
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

	hDwm = LoadLibrary(TEXT("dwmapi.dll"));
	if (hDwm == NULL) {
		return;
	}
	pDwmSetWindowAttribute = (PFN_DwmSetWindowAttribute)GetProcAddress(hDwm, "DwmSetWindowAttribute");
	if (pDwmSetWindowAttribute == NULL) {
		FreeLibrary(hDwm);
		return;
	}

	useDark = TRUE;
	// Best-effort: try both attribute IDs used by different Windows 10/11 builds.
	pDwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &useDark, sizeof(useDark));
	pDwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1, &useDark, sizeof(useDark));
	FreeLibrary(hDwm);
}

static BOOL IsBlacklistedWindowClass(HWND hWnd) {
	TCHAR className[128];
	if (GetClassName(hWnd, className, (int)(sizeof(className) / sizeof(className[0]))) == 0) {
		return FALSE;
	}
	// Avoid special/system UI windows where changing transparency is undesirable.
	return (lstrcmpi(className, TEXT("Shell_TrayWnd")) == 0) ||
		(lstrcmpi(className, TEXT("Shell_SecondaryTrayWnd")) == 0) ||
		(lstrcmpi(className, TEXT("Progman")) == 0) ||
		(lstrcmpi(className, TEXT("WorkerW")) == 0);
}

static BOOL IsUsableTargetWindow(HWND hWnd, HWND hOurWnd) {
	return hWnd != NULL && hWnd != hOurWnd && IsWindow(hWnd) &&
		IsWindowVisible(hWnd) && !IsBlacklistedWindowClass(hWnd);
}

static void ApplyTransparencyToWindow(HWND hOurWnd, HWND hTargetWindow, int iTenths) {
	WINDOWSTATE* state;
	BYTE targetAlpha;

	if (iTenths < 0 || iTenths > 9 || !IsUsableTargetWindow(hTargetWindow, hOurWnd)) {
		return;
	}

	PruneWindowStates();
	if (iTenths > 0) {
		state = GetOrCaptureWindowState(hTargetWindow);
		if (state == NULL) {
			return;
		}
		targetAlpha = (iTenths == 9) ? 255 : (BYTE)((iTenths * 255) / 10);
		BeginOpacityAnimation(hOurWnd, hTargetWindow, targetAlpha);
	} else {
		BeginResetAnimation(hOurWnd, hTargetWindow);
	}
}

static void ApplyTransparencyTenths(HWND hOurWnd, int iTenths) {
	ApplyTransparencyToWindow(hOurWnd, GetForegroundWindow(), iTenths);
}

static void ToggleTopmostWindow(HWND hOurWnd, HWND hTargetWindow) {
	LONG_PTR exStyle;

	if (!IsUsableTargetWindow(hTargetWindow, hOurWnd)) {
		return;
	}

	exStyle = GetWindowLongPtr(hTargetWindow, GWL_EXSTYLE);
	if (exStyle & WS_EX_TOPMOST) {
		SetWindowPos(hTargetWindow, HWND_NOTOPMOST, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE);
	} else {
		SetWindowPos(hTargetWindow, HWND_TOPMOST, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE);
	}
}

static void ToggleTopmost(HWND hOurWnd) {
	ToggleTopmostWindow(hOurWnd, GetForegroundWindow());
}

BOOL AddIconToSystemTray(HWND hWnd, UINT uID, LPCTSTR lpszTip) {
	NOTIFYICONDATA  tnid;
	int cx, cy;
	int cchTip;

	memset(&tnid, 0, sizeof(tnid));
	tnid.cbSize = sizeof(tnid);
	tnid.hWnd = hWnd;
	tnid.uID = uID;
	tnid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
	tnid.uCallbackMessage = TRAY_CALLBACK;

	cx = GetSystemMetrics(SM_CXSMICON);
	cy = GetSystemMetrics(SM_CYSMICON);
	tnid.hIcon = (HICON) LoadImage(g_hInstance, MAKEINTRESOURCE(IDI_ICON1),
		IMAGE_ICON, cx, cy, LR_SHARED);

	cchTip = (int)(sizeof(tnid.szTip) / sizeof(tnid.szTip[0]));
	if (lpszTip)
		lstrcpyn(tnid.szTip, lpszTip, cchTip);
	else 
		tnid.szTip[0] = '\0'; 

	if (!Shell_NotifyIcon(NIM_ADD, &tnid)) {
		return FALSE;
	}

	tnid.uVersion = NOTIFYICON_VERSION_4;
	Shell_NotifyIcon(NIM_SETVERSION, &tnid);
	return TRUE;
}

BOOL RemoveIconFromSystemTray(HWND hWnd, UINT uID) { 
	NOTIFYICONDATA tnid; 

	memset(&tnid, 0, sizeof(tnid));
	tnid.cbSize = sizeof(tnid); 
	tnid.hWnd = hWnd; 
	tnid.uID = uID; 
	return(Shell_NotifyIcon(NIM_DELETE, &tnid)); 
} 

BOOL ShowSettingsMenu(HWND hWnd, POINT pOint) {
	HMENU hPopup;
	UINT selectedCommand;
	TCHAR itemText[64];
	TCHAR resetText[64];
	TCHAR levelFormat[64];
	TCHAR topmostText[64];
	int i;

	hPopup = CreatePopupMenu();
	if (hPopup == NULL) {
		return FALSE;
	}

	LoadResString(IDS_MENU_OPACITY_RESET, resetText,
		(int)(sizeof(resetText) / sizeof(resetText[0])), TEXT("100% (Reset)\tCtrl+Shift+0"));
	LoadResString(IDS_MENU_OPACITY_LEVEL, levelFormat,
		(int)(sizeof(levelFormat) / sizeof(levelFormat[0])), TEXT("%d%% opacity\tCtrl+Shift+%d"));
	LoadResString(IDS_MENU_TOPMOST, topmostText,
		(int)(sizeof(topmostText) / sizeof(topmostText[0])), TEXT("Toggle always on top\tCtrl+Shift++"));

	AppendMenu(hPopup, MF_STRING, IDM_OPACITY_0, resetText);
	AppendMenu(hPopup, MF_SEPARATOR, 0, NULL);
	for (i = 9; i >= 1; --i) {
		StringCchPrintf(itemText, (int)(sizeof(itemText) / sizeof(itemText[0])),
			levelFormat, i * 10, i);
		AppendMenu(hPopup, MF_STRING, IDM_OPACITY_0 + i, itemText);
	}
	AppendMenu(hPopup, MF_SEPARATOR, 0, NULL);
	AppendMenu(hPopup, MF_STRING, IDM_TOPMOST, topmostText);

	SetForegroundWindow(hWnd);
	selectedCommand = TrackPopupMenuEx(hPopup,
		TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
		pOint.x, pOint.y, hWnd, NULL);
	if (selectedCommand != 0) {
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(selectedCommand, 0), 0);
	}
	PostMessage(hWnd, WM_NULL, 0, 0);
	DestroyMenu(hPopup);
	return TRUE;
}

BOOL ShowPopupMenu(HWND hWnd, POINT pOint) {
	HMENU  hPopup;
	UINT selectedCommand;
	TCHAR titleError[64];
	TCHAR msgCreatePopup[128];
	TCHAR menuSettings[64];
	TCHAR menuReader[64];
	TCHAR menuAbout[64];
	TCHAR menuExit[64];

	//	Create popup menu on the fly. We'll use InsertMenu or AppendMenu 
	//  to add items to it.
	hPopup = CreatePopupMenu();
	if (!hPopup) {
		LoadResString(IDS_TITLE_ERROR, titleError, (int)(sizeof(titleError) / sizeof(titleError[0])), TEXT("Error"));
		LoadResString(IDS_ERR_CREATEPOPUP, msgCreatePopup, (int)(sizeof(msgCreatePopup) / sizeof(msgCreatePopup[0])),
			TEXT("CreatePopupMenu failed"));
		MessageBox(NULL, msgCreatePopup, titleError, MB_OK);
		return FALSE;
	}

	LoadResString(IDS_MENU_SETTINGS, menuSettings, (int)(sizeof(menuSettings) / sizeof(menuSettings[0])), TEXT("&Settings..."));
	LoadResString(IDS_MENU_READER, menuReader, (int)(sizeof(menuReader) / sizeof(menuReader[0])), TEXT("Open &TXT Reader..."));
	LoadResString(IDS_MENU_ABOUT, menuAbout, (int)(sizeof(menuAbout) / sizeof(menuAbout[0])), TEXT("&About"));
	LoadResString(IDS_MENU_EXIT, menuExit, (int)(sizeof(menuExit) / sizeof(menuExit[0])), TEXT("E&xit"));
	AppendMenu(hPopup, MF_STRING, IDM_READER, menuReader);
	AppendMenu(hPopup, MF_SEPARATOR, 0, NULL);
	AppendMenu(hPopup, MF_STRING, IDM_SETTINGS, menuSettings);
	AppendMenu(hPopup, MF_SEPARATOR, 0, NULL);
	AppendMenu(hPopup,
		MF_STRING | ((g_bWindowShown)? MF_GRAYED : 0),
		IDM_MMAIN,
		menuAbout);
	AppendMenu(hPopup, MF_STRING, IDM_MEXIT, menuExit);

	// We have to do some MS voodoo to make the taskbar 
	// notification menus work right. It's not my fault - 
	// Microsoft is to blame on this one.
	// See MS Knowledgebase article Q135788 for more info.
	SetForegroundWindow(hWnd);		// [MS voodoo]
	selectedCommand = TrackPopupMenuEx(hPopup,
		TPM_RIGHTALIGN | TPM_BOTTOMALIGN | TPM_RETURNCMD | TPM_NONOTIFY,
		pOint.x, pOint.y, hWnd, NULL);
	if (selectedCommand != 0) {
		PostMessage(hWnd, WM_COMMAND, MAKEWPARAM(selectedCommand, 0), 0);
	}
	PostMessage(hWnd, WM_NULL, 0, 0);	// [More MS voodoo]
	DestroyMenu(hPopup);

	return TRUE;
}

static UINT ModifiersFromComboSelection(int selection) {
	switch (selection) {
	case 1: return MOD_ALT | MOD_SHIFT;
	case 2: return MOD_CONTROL | MOD_ALT;
	case 3: return MOD_CONTROL | MOD_ALT | MOD_SHIFT;
	default: return MOD_CONTROL | MOD_SHIFT;
	}
}

static int ComboSelectionFromModifiers(UINT modifiers) {
	if (modifiers == (MOD_ALT | MOD_SHIFT)) return 1;
	if (modifiers == (MOD_CONTROL | MOD_ALT)) return 2;
	if (modifiers == (MOD_CONTROL | MOD_ALT | MOD_SHIFT)) return 3;
	return 0;
}

static void PopulateModifierCombo(HWND combo) {
	SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)TEXT("Ctrl + Shift"));
	SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)TEXT("Alt + Shift"));
	SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)TEXT("Ctrl + Alt"));
	SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)TEXT("Ctrl + Alt + Shift"));
}

static void ShowSettingsWindow(HWND owner) {
	if (g_hSettingsWnd != NULL && IsWindow(g_hSettingsWnd)) {
		ShowWindow(g_hSettingsWnd, SW_RESTORE);
		SetForegroundWindow(g_hSettingsWnd);
		return;
	}
	g_hSettingsWnd = CreateDialog(g_hInstance, MAKEINTRESOURCE(IDD_SETTINGS), owner, SettingsDlgProc);
	if (g_hSettingsWnd != NULL) {
		ShowWindow(g_hSettingsWnd, SW_SHOW);
		SetForegroundWindow(g_hSettingsWnd);
	}
}

INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam) {
	APPSETTINGS proposed;
	HWND control;
	RECT rect;
	int selection;
	(void)lParam;

	switch (Msg) {
	case WM_INITDIALOG:
		g_hSettingsWnd = hDlg;
		TryEnableDarkModeTitleBar(hDlg);
		GetWindowRect(hDlg, &rect);
		SetWindowPos(hDlg, NULL,
			(GetSystemMetrics(SM_CXSCREEN) - (rect.right - rect.left)) / 2,
			(GetSystemMetrics(SM_CYSCREEN) - (rect.bottom - rect.top)) / 2,
			0, 0, SWP_NOSIZE | SWP_NOZORDER);
		control = GetDlgItem(hDlg, IDC_OPACITY_MODIFIERS);
		PopulateModifierCombo(control);
		SendMessage(control, CB_SETCURSEL, ComboSelectionFromModifiers(g_settings.opacityModifiers), 0);
		control = GetDlgItem(hDlg, IDC_TOPMOST_HOTKEY);
		PopulateModifierCombo(control);
		SendMessage(control, CB_SETCURSEL, ComboSelectionFromModifiers(g_settings.topmostModifiers), 0);
		CheckDlgButton(hDlg, IDC_RUN_AT_STARTUP,
			g_settings.runAtStartup ? BST_CHECKED : BST_UNCHECKED);

		// Create and apply the brand title font (14pt Segoe UI Semibold).
		{
			HDC hdcScreen = GetDC(NULL);
			int lfHeight = -MulDiv(14, GetDeviceCaps(hdcScreen, LOGPIXELSY), 72);
			ReleaseDC(NULL, hdcScreen);
			g_hSettingsTitleFont = CreateFontW(lfHeight, 0, 0, 0, FW_SEMIBOLD,
				FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
			if (g_hSettingsTitleFont != NULL) {
				SendMessage(GetDlgItem(hDlg, IDC_SETTINGS_HEADER), WM_SETFONT,
					(WPARAM)g_hSettingsTitleFont, TRUE);
			}
		}
		return TRUE;

	case WM_CTLCOLORDLG:
		return (INT_PTR)g_settingsBackgroundBrush;
	case WM_CTLCOLORSTATIC:
		SetBkMode((HDC)wParam, TRANSPARENT);
		if ((HWND)lParam == GetDlgItem(hDlg, IDC_SETTINGS_SUBTITLE)) {
			SetTextColor((HDC)wParam, SUBTITLE_COLOR);
		}
		return (INT_PTR)g_settingsBackgroundBrush;

	case WM_DRAWITEM:
	{
		DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
		if (dis != NULL && dis->CtlID == IDC_SETTINGS_HEADER) {
			TCHAR text[128];
			HFONT hOldFont;
			RECT rc;
			text[0] = TEXT('\0');
			if (dis->hwndItem != NULL) {
				GetWindowText(dis->hwndItem, text, 128);
			}
			SetBkMode(dis->hDC, TRANSPARENT);
			SetTextColor(dis->hDC, BRAND_COLOR);
			hOldFont = (HFONT)SelectObject(dis->hDC,
				g_hSettingsTitleFont ? g_hSettingsTitleFont : GetStockObject(DEFAULT_GUI_FONT));
			rc = dis->rcItem;
			DrawText(dis->hDC, text, -1, &rc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
			SelectObject(dis->hDC, hOldFont);
			return TRUE;
		}
		break;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_RESTORE_DEFAULTS:
			SetDefaultSettings(&proposed);
			SendMessage(GetDlgItem(hDlg, IDC_OPACITY_MODIFIERS), CB_SETCURSEL,
				ComboSelectionFromModifiers(proposed.opacityModifiers), 0);
			SendMessage(GetDlgItem(hDlg, IDC_TOPMOST_HOTKEY), CB_SETCURSEL,
				ComboSelectionFromModifiers(proposed.topmostModifiers), 0);
			CheckDlgButton(hDlg, IDC_RUN_AT_STARTUP, BST_UNCHECKED);
			SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, TEXT("Defaults restored. Select Apply to save."));
			return TRUE;
		case IDC_OPEN_ABOUT:
			ShowWindow(g_hMainWnd, SW_SHOW);
			SetForegroundWindow(g_hMainWnd);
			g_bWindowShown = TRUE;
			return TRUE;
		case IDOK:
			selection = (int)SendMessage(GetDlgItem(hDlg, IDC_OPACITY_MODIFIERS), CB_GETCURSEL, 0, 0);
			proposed.opacityModifiers = ModifiersFromComboSelection(selection);
			selection = (int)SendMessage(GetDlgItem(hDlg, IDC_TOPMOST_HOTKEY), CB_GETCURSEL, 0, 0);
			proposed.topmostModifiers = ModifiersFromComboSelection(selection);
			proposed.topmostVk = VK_OEM_PLUS;
			proposed.runAtStartup = IsDlgButtonChecked(hDlg, IDC_RUN_AT_STARTUP) == BST_CHECKED;
			if (!RegisterConfiguredHotkeys(g_hMainWnd, &proposed, TRUE)) {
				return TRUE;
			}
			if (!UpdateRunAtStartup(proposed.runAtStartup)) {
				MessageBox(hDlg, TEXT("Unable to update the startup setting."), TEXT("Vitrite"), MB_OK | MB_ICONWARNING);
				RegisterConfiguredHotkeys(g_hMainWnd, &g_settings, FALSE);
				return TRUE;
			}
			g_settings = proposed;
			SaveSettings();
			SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, TEXT("Settings saved and applied."));
			return TRUE;
		case IDCANCEL:
			DestroyWindow(hDlg);
			return TRUE;
		}
		break;

	case WM_CLOSE:
		DestroyWindow(hDlg);
		return TRUE;
	case WM_DESTROY:
		if (g_hSettingsTitleFont != NULL) {
			DeleteObject(g_hSettingsTitleFont);
			g_hSettingsTitleFont = NULL;
		}
		g_hSettingsWnd = NULL;
		return TRUE;
	}
	return FALSE;
}

INT_PTR CALLBACK MainDlgProc(HWND hDlg,
							 UINT Msg,
							 WPARAM wParam,
							 LPARAM lParam) {
	RECT  DlgRect;
	POINT  pMenuPoint;
	int i;

	switch(Msg) {
	case WM_INITDIALOG :
		g_hMainWnd = hDlg;
		GetWindowRect(hDlg, &DlgRect);
		SetWindowPos(hDlg, HWND_TOP,
			(GetSystemMetrics(SM_CXSCREEN)-(DlgRect.right-DlgRect.left))/2, 
			(GetSystemMetrics(SM_CYSCREEN)-(DlgRect.bottom-DlgRect.top))/2,
			0, 0, SWP_NOSIZE);
		TryEnableDarkModeTitleBar(hDlg);
		return(TRUE);

	case WM_DESTROY :
		StopOpacityAnimation();
		CleanupTextReader();
		// Unregister hotkeys
		for (i = 0; i <= 9; i++) {
			if (g_hotkeyAlphaRegistered[i]) {
				UnregisterHotKey(hDlg, HOTKEY_ALPHA_0 + i);
			}
		}
		if (g_hotkeyTopmostRegistered) {
			UnregisterHotKey(hDlg, HOTKEY_TOPMOST);
		}
		if (g_hotkeyTopmostNumpadRegistered) {
			UnregisterHotKey(hDlg, HOTKEY_TOPMOST_NUMPAD);
		}
		RemoveIconFromSystemTray(hDlg, IDI_ICON1);
		if (ghMutex != NULL) {
			CloseHandle(ghMutex);
			ghMutex = NULL;
		}
		PostQuitMessage(0);
		return(TRUE);

	case TRAY_CALLBACK :
		// With NOTIFYICON_VERSION_4, the notification code is stored in
		// LOWORD(lParam), while HIWORD(lParam) contains the icon ID.
		switch(LOWORD(lParam)) {
		case WM_CONTEXTMENU :
		case WM_RBUTTONUP :
			g_hMenuTargetWnd = NULL;
			// Version 4 supplies an anchor point in wParam. Fall back to the
			// current cursor position for legacy notifications.
			if (LOWORD(lParam) == WM_CONTEXTMENU) {
				pMenuPoint.x = GET_X_LPARAM(wParam);
				pMenuPoint.y = GET_Y_LPARAM(wParam);
			} else {
				GetCursorPos(&pMenuPoint);
			}
			ShowPopupMenu(hDlg, pMenuPoint);
			return(TRUE);

		case NIN_SELECT :
		case WM_LBUTTONUP :
			g_hMenuTargetWnd = GetForegroundWindow();
			GetCursorPos(&pMenuPoint);
			ShowSettingsMenu(hDlg, pMenuPoint);
			return(TRUE);

		case NIN_KEYSELECT :
		case WM_LBUTTONDBLCLK :
			BringTextReaderToFront();
			return(TRUE);
		}
		return(TRUE);

	default:
		if (Msg == g_uTaskbarRestart && g_uTaskbarRestart != 0) {
			// Explorer crashed or restarted - re-add our icon
			TCHAR cTrayTip[128];
			TCHAR trayTipFormat[64];
			LoadResString(IDS_TRAYTIP_FORMAT, trayTipFormat, (int)(sizeof(trayTipFormat) / sizeof(trayTipFormat[0])),
				TEXT("Vitrite - %s"));
			StringCchPrintf(cTrayTip, (int)(sizeof(cTrayTip) / sizeof(cTrayTip[0])),
				trayTipFormat, VITRITE_VERSION);
			AddIconToSystemTray(hDlg, IDI_ICON1, cTrayTip);
			return(TRUE);
		}
		break;

	case WM_COMMAND :
		if (LOWORD(wParam) >= IDM_OPACITY_0 && LOWORD(wParam) <= IDM_OPACITY_9) {
			ApplyTransparencyToWindow(hDlg, g_hMenuTargetWnd,
				(int)(LOWORD(wParam) - IDM_OPACITY_0));
			g_hMenuTargetWnd = NULL;
			return(TRUE);
		}
		if (LOWORD(wParam) == IDM_TOPMOST) {
			ToggleTopmostWindow(hDlg, g_hMenuTargetWnd);
			g_hMenuTargetWnd = NULL;
			return(TRUE);
		}
		switch(LOWORD(wParam)) {
		case IDOK :
			ShowWindow(hDlg, SW_HIDE);
			g_bWindowShown = FALSE;
			return(TRUE);

		case IDM_MEXIT :
			PostQuitMessage(0);
			return(TRUE);

		case IDM_SETTINGS :
			ShowSettingsWindow(hDlg);
			return(TRUE);

		case IDM_READER :
			BringTextReaderToFront();
			return(TRUE);

		case IDM_MMAIN :
			ShowWindow(hDlg, SW_SHOW);
			UpdateWindow(hDlg);
			g_bWindowShown = TRUE;
			return(TRUE);
		}		
		return(TRUE);

	case WM_COPYDATA :
	{
		COPYDATASTRUCT* data = (COPYDATASTRUCT*)lParam;
		TCHAR path[MAX_PATH];
		size_t chars;
		if (data != NULL && data->dwData == COPYDATA_OPEN_TEXT_FILE &&
			data->lpData != NULL && data->cbData >= sizeof(TCHAR)) {
			chars = data->cbData / sizeof(TCHAR);
			if (chars >= MAX_PATH) {
				chars = MAX_PATH - 1;
			}
			memcpy(path, data->lpData, chars * sizeof(TCHAR));
			path[chars] = TEXT('\0');
			OpenTextReaderWindow(hDlg, path);
		} else {
			BringTextReaderToFront();
		}
		return TRUE;
	}

	case WM_HOTKEY :
		if (wParam >= HOTKEY_ALPHA_0 && wParam <= HOTKEY_ALPHA_9) {
			ApplyTransparencyTenths(hDlg, (int)(wParam - HOTKEY_ALPHA_0));
			return(TRUE);
		} else if (wParam == HOTKEY_TOPMOST || wParam == HOTKEY_TOPMOST_NUMPAD) {
			ToggleTopmost(hDlg);
			return(TRUE);
		}
		return(TRUE);

	case WM_TIMER :
		if (wParam == TIMER_OPACITY_ANIM) {
			TickOpacityAnimation();
			return(TRUE);
		}
		return(TRUE);
	}	
	return(FALSE);
}

int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
					   LPTSTR lpCmdLine, int nCmdShow) {
	HWND  hWnd;
	MSG  Msg;
	TCHAR  cTrayTip[128];
	TCHAR titleError[64];
	TCHAR titleWarning[64];
	TCHAR msgMutexCreate[128];
	TCHAR msgAlreadyRunning[128];
	TCHAR msgTrayIcon[128];
	TCHAR msgNoHotkeys[256];
	TCHAR trayTipFormat[64];

	(void)hPrevInstance;
	(void)nCmdShow;

	g_hInstance = hInstance;
	InitUiLanguage();
	InitializeTextReader(hInstance);
	LoadSettings();
	g_brandBrush = CreateSolidBrush(BRAND_COLOR);
	g_settingsBackgroundBrush = CreateSolidBrush(RGB(248, 247, 252));

	g_uTaskbarRestart = RegisterWindowMessage(TEXT("TaskbarCreated"));

	// Create/check for a mutex to determine if another copy of Vitrite is already running.
	if ((ghMutex = CreateMutex(NULL, FALSE, TEXT("_vitrite_mutex"))) == NULL) {
		LoadResString(IDS_TITLE_ERROR, titleError, (int)(sizeof(titleError) / sizeof(titleError[0])), TEXT("Critical Error"));
		LoadResString(IDS_ERR_MUTEX_CREATE, msgMutexCreate, (int)(sizeof(msgMutexCreate) / sizeof(msgMutexCreate[0])),
			TEXT("Unable to create mutex"));
		MessageBox(NULL, msgMutexCreate, titleError, MB_OK | MB_ICONSTOP);
		return 1;
	} else if (GetLastError() == ERROR_ALREADY_EXISTS) {
		HWND existing = FindExistingVitriteInstance();
		if (existing != NULL) {
			if (!IsStartupLaunch(lpCmdLine)) {
				NotifyExistingVitriteInstance(existing, lpCmdLine);
			}
			CloseHandle(ghMutex);
			ghMutex = NULL;
			CleanupTextReader();
			return 0;
		}
		LoadResString(IDS_TITLE_WARNING, titleWarning, (int)(sizeof(titleWarning) / sizeof(titleWarning[0])), TEXT("Warning"));
		LoadResString(IDS_WARN_ALREADY_RUNNING, msgAlreadyRunning, (int)(sizeof(msgAlreadyRunning) / sizeof(msgAlreadyRunning[0])),
			TEXT("Another copy of Vitrite is already running."));
		MessageBox(NULL, msgAlreadyRunning, titleWarning, MB_OK | MB_ICONEXCLAMATION);
		CloseHandle(ghMutex);
		ghMutex = NULL;
		return 0;
	}

	memset(&Msg, 0, sizeof(Msg));
	// Build/show main dialog
	hWnd = CreateDialog(hInstance,
		MAKEINTRESOURCE(IDD_MAIN),
		NULL,
		MainDlgProc);
	SetProp(hWnd, INSTANCE_PROPERTY, (HANDLE)1);
	SendMessage(hWnd, WM_SETICON, 
		(WPARAM) ICON_BIG,
		(LPARAM) (HICON) LoadImage(hInstance,
		MAKEINTRESOURCE(IDI_ICON1),
		IMAGE_ICON, 32, 32, 0));

	ShowWindow(hWnd, SW_HIDE);
	g_bWindowShown = FALSE;
	UpdateWindow(hWnd);

	LoadResString(IDS_TRAYTIP_FORMAT, trayTipFormat, (int)(sizeof(trayTipFormat) / sizeof(trayTipFormat[0])),
		TEXT("Vitrite - %s"));
	StringCchPrintf(cTrayTip, (int)(sizeof(cTrayTip) / sizeof(cTrayTip[0])),
		trayTipFormat, VITRITE_VERSION);
	if (!AddIconToSystemTray(hWnd, IDI_ICON1, cTrayTip)) {
		LoadResString(IDS_TITLE_ERROR, titleError, (int)(sizeof(titleError) / sizeof(titleError[0])), TEXT("Critical Error"));
		LoadResString(IDS_ERR_TRAYICON, msgTrayIcon, (int)(sizeof(msgTrayIcon) / sizeof(msgTrayIcon[0])),
			TEXT("Unable to put icon on taskbar."));
		MessageBox(NULL, msgTrayIcon, titleError, MB_ICONSTOP | MB_OK);
		DestroyWindow(hWnd);
		return (int)(INT_PTR)Msg.wParam;
	}

	// Register configurable global hotkeys.
	memset(g_hotkeyAlphaRegistered, 0, sizeof(g_hotkeyAlphaRegistered));
	g_hotkeyTopmostRegistered = FALSE;
	g_hotkeyTopmostNumpadRegistered = FALSE;
	if (!RegisterConfiguredHotkeys(hWnd, &g_settings, FALSE)) {
		APPSETTINGS defaults;
		SetDefaultSettings(&defaults);
		if (!RegisterConfiguredHotkeys(hWnd, &defaults, FALSE)) {
			LoadResString(IDS_TITLE_ERROR, titleError, (int)(sizeof(titleError) / sizeof(titleError[0])), TEXT("Critical Error"));
			LoadResString(IDS_ERR_NO_HOTKEYS, msgNoHotkeys, (int)(sizeof(msgNoHotkeys) / sizeof(msgNoHotkeys[0])),
				TEXT("Unable to register any hotkeys.\r\nAnother program may be using them."));
			MessageBox(NULL, msgNoHotkeys, titleError, MB_OK | MB_ICONSTOP);
			RemoveIconFromSystemTray(hWnd, IDI_ICON1);
			DestroyWindow(hWnd);
			return (int)(INT_PTR)Msg.wParam;
		}
		g_settings = defaults;
		SaveSettings();
	}

	if (lpCmdLine != NULL && lpCmdLine[0] != TEXT('\0')) {
		if (IsStartupLaunch(lpCmdLine)) {
			/* Keep the tray utility silent when launched by Windows sign-in. */
		} else {
			OpenTextReaderFromCommandLine(hWnd, lpCmdLine);
		}
	} else {
		OpenTextReaderWindow(hWnd, NULL);
	}

	while (GetMessage(&Msg, NULL, 0, 0)) {
		if (Msg.message == WM_HOTKEY) {
			DispatchMessage(&Msg);
			continue;
		}
		if (!IsDialogMessage(hWnd, &Msg)) {
			TranslateMessage(&Msg);
			DispatchMessage(&Msg);
		}
	}
	DestroyWindow(hWnd);
	if (g_brandBrush != NULL) {
		DeleteObject(g_brandBrush);
		g_brandBrush = NULL;
	}
	if (g_settingsBackgroundBrush != NULL) {
		DeleteObject(g_settingsBackgroundBrush);
		g_settingsBackgroundBrush = NULL;
	}

	return (int)(INT_PTR)Msg.wParam;
}
