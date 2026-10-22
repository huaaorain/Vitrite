#ifndef VITRITE_UI_H
#define VITRITE_UI_H

#include <windows.h>

typedef struct VITRITE_UI_THEME {
	BOOL dark;
	BOOL highContrast;
	COLORREF windowBackground;
	COLORREF headerBackground;
	COLORREF surfaceBackground;
	COLORREF text;
	COLORREF mutedText;
	COLORREF accent;
	COLORREF accentHover;
	COLORREF accentText;
	COLORREF button;
	COLORREF buttonHover;
	COLORREF buttonPressed;
	COLORREF border;
	COLORREF separator;
	COLORREF selectionBackground;
	COLORREF selectionText;
	COLORREF focus;
} VITRITE_UI_THEME;

void ApplyRoundedWindowCorners(HWND window);
void GetVitriteUiTheme(HWND window, VITRITE_UI_THEME* theme);
void ApplyWindowDarkTitleBar(HWND window, BOOL dark);
BOOL IsWindowPinnedTopmost(HWND window);
void SetWindowPinnedTopmost(HWND window, BOOL topmost);
void DrawPinButton(const DRAWITEMSTRUCT* item, BOOL topmost);

#endif
