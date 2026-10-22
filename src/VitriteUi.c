#include <windows.h>
#include "VitriteUi.h"

#define DWMWA_WINDOW_CORNER_PREFERENCE_VITRITE 33
#define DWMWCP_ROUND_VITRITE 2

typedef HRESULT (WINAPI *PFN_DwmSetWindowAttribute)(HWND, DWORD, LPCVOID, DWORD);
typedef LONG (WINAPI *PFN_RtlGetVersion)(void*);

typedef struct VITRITE_OSVERSIONINFO {
	ULONG size;
	ULONG major;
	ULONG minor;
	ULONG build;
	ULONG platform;
	WCHAR servicePack[128];
} VITRITE_OSVERSIONINFO;

static BOOL IsWindows11OrGreater(void) {
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	PFN_RtlGetVersion getVersion = ntdll != NULL
		? (PFN_RtlGetVersion)GetProcAddress(ntdll, "RtlGetVersion")
		: NULL;
	VITRITE_OSVERSIONINFO version;
	if (getVersion == NULL) {
		return FALSE;
	}
	memset(&version, 0, sizeof(version));
	version.size = sizeof(version);
	if (getVersion(&version) != 0) {
		return FALSE;
	}
	return version.major > 10 ||
		(version.major == 10 && version.build >= 22000);
}

static BOOL IsHighContrastEnabled(void) {
	HIGHCONTRASTW contrast;
	memset(&contrast, 0, sizeof(contrast));
	contrast.cbSize = sizeof(contrast);
	return SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(contrast),
		&contrast, 0) &&
		(contrast.dwFlags & HCF_HIGHCONTRASTON) != 0;
}

void GetVitriteUiTheme(HWND window, VITRITE_UI_THEME* theme) {
	WCHAR override[32];
	(void)window;
	if (theme == NULL) {
		return;
	}
	memset(theme, 0, sizeof(*theme));
	override[0] = L'\0';
	GetEnvironmentVariableW(L"VITRITE_UI_THEME_OVERRIDE", override, 32);
	theme->highContrast = IsHighContrastEnabled() ||
		lstrcmpiW(override, L"contrast") == 0;
	if (theme->highContrast) {
		theme->dark = FALSE;
		theme->windowBackground = GetSysColor(COLOR_WINDOW);
		theme->headerBackground = GetSysColor(COLOR_BTNFACE);
		theme->surfaceBackground = GetSysColor(COLOR_WINDOW);
		theme->text = GetSysColor(COLOR_WINDOWTEXT);
		theme->mutedText = GetSysColor(COLOR_WINDOWTEXT);
		theme->accent = GetSysColor(COLOR_HIGHLIGHT);
		theme->accentHover = GetSysColor(COLOR_HIGHLIGHT);
		theme->accentText = GetSysColor(COLOR_HIGHLIGHTTEXT);
		theme->button = GetSysColor(COLOR_BTNFACE);
		theme->buttonHover = GetSysColor(COLOR_HIGHLIGHT);
		theme->buttonPressed = GetSysColor(COLOR_HIGHLIGHT);
		theme->border = GetSysColor(COLOR_WINDOWTEXT);
		theme->separator = GetSysColor(COLOR_WINDOWTEXT);
		theme->selectionBackground = GetSysColor(COLOR_HIGHLIGHT);
		theme->selectionText = GetSysColor(COLOR_HIGHLIGHTTEXT);
		theme->focus = GetSysColor(COLOR_HIGHLIGHT);
		return;
	}
	theme->dark = lstrcmpiW(override, L"dark") == 0;
	if (theme->dark) {
		theme->windowBackground = RGB(30, 30, 30);
		theme->headerBackground = RGB(36, 36, 36);
		theme->surfaceBackground = RGB(26, 26, 26);
		theme->text = RGB(242, 242, 242);
		theme->mutedText = RGB(174, 174, 174);
		theme->accent = RGB(236, 236, 236);
		theme->accentHover = RGB(255, 255, 255);
		theme->accentText = RGB(20, 20, 20);
		theme->button = RGB(45, 45, 45);
		theme->buttonHover = RGB(58, 58, 58);
		theme->buttonPressed = RGB(69, 69, 69);
		theme->border = RGB(73, 73, 73);
		theme->separator = RGB(62, 62, 62);
		theme->selectionBackground = GetSysColor(COLOR_HIGHLIGHT);
		theme->selectionText = RGB(255, 255, 255);
		theme->focus = RGB(255, 255, 255);
	} else {
		theme->windowBackground = RGB(255, 255, 255);
		theme->headerBackground = RGB(255, 255, 255);
		theme->surfaceBackground = RGB(255, 255, 255);
		theme->text = RGB(29, 29, 29);
		theme->mutedText = RGB(109, 109, 109);
		theme->accent = RGB(242, 242, 242);
		theme->accentHover = RGB(233, 233, 233);
		theme->accentText = RGB(22, 22, 22);
		theme->button = RGB(255, 255, 255);
		theme->buttonHover = RGB(243, 243, 243);
		theme->buttonPressed = RGB(232, 232, 232);
		theme->border = RGB(224, 224, 224);
		theme->separator = RGB(236, 236, 236);
		theme->selectionBackground = GetSysColor(COLOR_HIGHLIGHT);
		theme->selectionText = GetSysColor(COLOR_HIGHLIGHTTEXT);
		theme->focus = GetSysColor(COLOR_HIGHLIGHT);
	}
}

void ApplyWindowDarkTitleBar(HWND window, BOOL dark) {
	static PFN_DwmSetWindowAttribute setWindowAttribute;
	static BOOL initialized;
	BOOL value = dark;
	if (window == NULL || !IsWindow(window)) {
		return;
	}
	if (!initialized) {
		HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
		if (dwmapi != NULL) {
			setWindowAttribute = (PFN_DwmSetWindowAttribute)GetProcAddress(
				dwmapi, "DwmSetWindowAttribute");
		}
		initialized = TRUE;
	}
	if (setWindowAttribute == NULL) {
		return;
	}
	setWindowAttribute(window, 20, &value, sizeof(value));
	setWindowAttribute(window, 19, &value, sizeof(value));
}

void ApplyRoundedWindowCorners(HWND window) {
	static PFN_DwmSetWindowAttribute setWindowAttribute;
	static BOOL initialized;
	HRGN region;
	RECT rect;
	int radius;
	UINT dpi;
	HDC screen;

	if (window == NULL || !IsWindow(window)) {
		return;
	}
	if (!initialized) {
		HMODULE dwmapi = LoadLibraryW(L"dwmapi.dll");
		if (dwmapi != NULL) {
			setWindowAttribute = (PFN_DwmSetWindowAttribute)GetProcAddress(
				dwmapi, "DwmSetWindowAttribute");
		}
		initialized = TRUE;
	}

	if (setWindowAttribute != NULL && IsWindows11OrGreater()) {
		int preference = DWMWCP_ROUND_VITRITE;
		COLORREF noBorder = 0xFFFFFFFE;
		setWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE_VITRITE,
			&preference, sizeof(preference));
		setWindowAttribute(window, 34, &noBorder, sizeof(noBorder));
	}

	if (IsZoomed(window)) {
		SetWindowRgn(window, NULL, TRUE);
		return;
	}
	if (!GetWindowRect(window, &rect)) {
		return;
	}
	screen = GetDC(NULL);
	dpi = screen != NULL ? (UINT)GetDeviceCaps(screen, LOGPIXELSX) : 96;
	if (screen != NULL) {
		ReleaseDC(NULL, screen);
	}
	radius = MulDiv(14, (int)dpi, 96);
	region = CreateRoundRectRgn(0, 0, rect.right - rect.left + 1,
		rect.bottom - rect.top + 1, radius, radius);
	if (region != NULL) {
		SetWindowRgn(window, region, TRUE);
	}
}

BOOL IsWindowPinnedTopmost(HWND window) {
	return window != NULL &&
		(GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
}

void SetWindowPinnedTopmost(HWND window, BOOL topmost) {
	if (window == NULL || !IsWindow(window)) {
		return;
	}
	SetWindowPos(window, topmost ? HWND_TOPMOST : HWND_NOTOPMOST,
		0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void DrawPinButton(const DRAWITEMSTRUCT* item, BOOL topmost) {
	RECT rect;
	HBRUSH fill;
	HBRUSH glyph;
	HPEN border;
	HPEN needle;
	HGDIOBJ oldBrush;
	HGDIOBJ oldPen;
	COLORREF fillColor;
	COLORREF borderColor;
	COLORREF glyphColor;
	VITRITE_UI_THEME theme;
	BOOL hovered;
	POINT body[5];
	int centerX;
	int centerY;

	if (item == NULL || item->hDC == NULL) {
		return;
	}
	GetVitriteUiTheme(item->hwndItem, &theme);
	rect = item->rcItem;
	hovered = GetPropW(item->hwndItem, L"VitriteControlHover") != NULL;
	fillColor = topmost ? theme.accent : theme.button;
	borderColor = topmost ? theme.accent : theme.border;
	glyphColor = topmost ? theme.accentText : theme.text;
	if (item->itemState & ODS_SELECTED) {
		fillColor = topmost ? theme.accentHover : theme.buttonPressed;
	}
	if (hovered) {
		fillColor = topmost ? theme.accentHover : theme.buttonHover;
		if (theme.highContrast) {
			fillColor = theme.accent;
			glyphColor = theme.accentText;
		}
	}
	fill = CreateSolidBrush(fillColor);
	glyph = CreateSolidBrush(glyphColor);
	border = CreatePen(PS_SOLID, topmost ? 2 : 1, borderColor);
	needle = CreatePen(PS_SOLID, max(1, (rect.bottom - rect.top) / 16),
		glyphColor);
	oldBrush = SelectObject(item->hDC, fill);
	oldPen = SelectObject(item->hDC, border);
	RoundRect(item->hDC, rect.left, rect.top, rect.right, rect.bottom,
		8, 8);
	SelectObject(item->hDC, oldPen);
	SelectObject(item->hDC, oldBrush);
	DeleteObject(fill);
	DeleteObject(border);

	centerX = (rect.left + rect.right) / 2;
	centerY = (rect.top + rect.bottom) / 2 - 2;
	oldBrush = SelectObject(item->hDC, glyph);
	oldPen = SelectObject(item->hDC, needle);
	Ellipse(item->hDC, centerX - 6, centerY - 10,
		centerX + 6, centerY + 1);
	body[0].x = centerX - 4;
	body[0].y = centerY;
	body[1].x = centerX + 4;
	body[1].y = centerY;
	body[2].x = centerX + 3;
	body[2].y = centerY + 8;
	body[3].x = centerX - 3;
	body[3].y = centerY + 8;
	body[4] = body[0];
	Polygon(item->hDC, body, 4);
	MoveToEx(item->hDC, centerX, centerY + 8, NULL);
	LineTo(item->hDC, centerX, centerY + 13);
	SelectObject(item->hDC, oldPen);
	SelectObject(item->hDC, oldBrush);
	DeleteObject(glyph);
	DeleteObject(needle);
	if (item->itemState & ODS_FOCUS) {
		InflateRect(&rect, -3, -3);
		DrawFocusRect(item->hDC, &rect);
	}
}
