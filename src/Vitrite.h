#ifndef VITRITE_H
#define VITRITE_H

#include <windows.h>

// Prototypes
BOOL AddIconToSystemTray(HWND hWnd, UINT uID, LPCTSTR lpszTip);
BOOL RemoveIconFromSystemTray(HWND hWnd, UINT uID);
BOOL ShowPopupMenu(HWND hWnd, POINT pOint);
BOOL ShowSettingsMenu(HWND hWnd, POINT pOint);
INT_PTR CALLBACK MainDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK SettingsDlgProc(HWND hDlg, UINT Msg, WPARAM wParam, LPARAM lParam);

// Constants
#define TRAY_CALLBACK	10001

// Notification icon messages (for NOTIFYICON_VERSION_4)
#ifndef NIN_SELECT
#define NIN_SELECT (WM_USER + 0)
#endif

#define IDM_MEXIT        500
#define IDM_MMAIN        501
#define IDM_SETTINGS     502
#define IDM_OPACITY_0    510
#define IDM_OPACITY_9    (IDM_OPACITY_0 + 9)
#define IDM_TOPMOST      520
#define IDM_READER       503
#define IDM_LIBRARY      504

#define HOTKEY_ALPHA_0  100
#define HOTKEY_ALPHA_9  (HOTKEY_ALPHA_0 + 9)
#define HOTKEY_TOPMOST  110
#define HOTKEY_TOPMOST_NUMPAD 111

#define VITRITE_VERSION	TEXT("0.2.0")


#endif	// VITRITE_H
