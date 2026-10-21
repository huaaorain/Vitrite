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
		setWindowAttribute(window, DWMWA_WINDOW_CORNER_PREFERENCE_VITRITE,
			&preference, sizeof(preference));
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
