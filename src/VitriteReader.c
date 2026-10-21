#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shellapi.h>
#include <strsafe.h>
#include "resource.h"
#include "VitriteReader.h"

#define READER_CLASS_NAME L"VitriteTextReaderWindow"
#define READER_TITLE L"Vitrite TXT 阅读器"
#define READER_SETTINGS_NAME L"reader.ini"
#define READER_TOOLBAR_HEIGHT 54
#define READER_MAX_FILE_BYTES (128ULL * 1024ULL * 1024ULL)

#define IDC_READER_OPEN 600
#define IDC_READER_FONT_MINUS 601
#define IDC_READER_FONT_PLUS 602
#define IDC_READER_OPACITY 603
#define IDC_READER_STATUS 604
#define IDC_READER_EDIT 605
#define IDC_READER_RECENT 606

#define IDM_READER_OPEN 700
#define IDM_READER_FONT_MINUS 701
#define IDM_READER_FONT_PLUS 702
#define IDM_READER_OPACITY_BASE 710
#define IDM_READER_EXIT 799

#define READER_BG_COLOR RGB(250, 248, 241)
#define READER_TOOLBAR_COLOR RGB(242, 238, 229)
#define READER_TEXT_COLOR RGB(42, 40, 37)
#define READER_SUBTLE_COLOR RGB(107, 101, 92)

static HINSTANCE g_readerInstance;
static HMODULE g_richEditLibrary;
static HWND g_readerWindow;
static HWND g_readerEdit;
static HWND g_readerOpenButton;
static HWND g_readerFontMinusButton;
static HWND g_readerFontPlusButton;
static HWND g_readerOpacityCombo;
static HWND g_readerStatus;
static HFONT g_readerTextFont;
static HFONT g_readerUiFont;
static HBRUSH g_readerBackgroundBrush;
static HBRUSH g_readerToolbarBrush;
static WCHAR g_readerCurrentPath[MAX_PATH];
static RECT g_readerSavedRect;
static int g_readerFontPoints = 16;
static int g_readerOpacityPercent = 100;
static UINT g_readerDpi = 96;
static BOOL g_readerClassRegistered;

typedef enum READER_ENCODING {
	READER_ENCODING_UTF8,
	READER_ENCODING_UTF8_BOM,
	READER_ENCODING_UTF16_LE,
	READER_ENCODING_UTF16_BE,
	READER_ENCODING_SYSTEM
} READER_ENCODING;

static UINT GetReaderDpi(HWND hwnd) {
	typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);
	HMODULE user32;
	PFN_GetDpiForWindow getDpiForWindow;
	UINT dpi;
	HDC screen;

	user32 = GetModuleHandleW(L"user32.dll");
	getDpiForWindow = user32 != NULL
		? (PFN_GetDpiForWindow)GetProcAddress(user32, "GetDpiForWindow")
		: NULL;
	if (getDpiForWindow != NULL && hwnd != NULL) {
		dpi = getDpiForWindow(hwnd);
		if (dpi >= 72) {
			return dpi;
		}
	}

	screen = GetDC(NULL);
	dpi = screen != NULL ? (UINT)GetDeviceCaps(screen, LOGPIXELSX) : 96;
	if (screen != NULL) {
		ReleaseDC(NULL, screen);
	}
	return dpi >= 72 ? dpi : 96;
}

static int ReaderScale(HWND hwnd, int value) {
	return MulDiv(value, (int)GetReaderDpi(hwnd), 96);
}

static void GetReaderSettingsPath(LPWSTR path, int pathCount) {
	DWORD length;
	LPWSTR slash;

	if (path == NULL || pathCount <= 0) {
		return;
	}
	path[0] = L'\0';
	length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, (DWORD)pathCount);
	if (length == 0 || length >= (DWORD)pathCount) {
		GetModuleFileNameW(NULL, path, (DWORD)pathCount);
		slash = wcsrchr(path, L'\\');
		if (slash != NULL) {
			slash[1] = L'\0';
		}
		StringCchCatW(path, (size_t)pathCount, READER_SETTINGS_NAME);
		return;
	}
	StringCchCatW(path, (size_t)pathCount, L"\\Vitrite");
	CreateDirectoryW(path, NULL);
	StringCchCatW(path, (size_t)pathCount, L"\\");
	StringCchCatW(path, (size_t)pathCount, READER_SETTINGS_NAME);
}

static void LoadReaderSettings(void) {
	WCHAR path[MAX_PATH];
	int value;

	GetReaderSettingsPath(path, MAX_PATH);
	g_readerFontPoints = GetPrivateProfileIntW(L"Reader", L"FontPoints", 16, path);
	if (g_readerFontPoints < 9) {
		g_readerFontPoints = 9;
	} else if (g_readerFontPoints > 42) {
		g_readerFontPoints = 42;
	}
	g_readerOpacityPercent = GetPrivateProfileIntW(L"Reader", L"OpacityPercent", 100, path);
	if (g_readerOpacityPercent < 35) {
		g_readerOpacityPercent = 35;
	} else if (g_readerOpacityPercent > 100) {
		g_readerOpacityPercent = 100;
	}

	memset(&g_readerSavedRect, 0, sizeof(g_readerSavedRect));
	g_readerSavedRect.left = GetPrivateProfileIntW(L"Window", L"Left", 0, path);
	g_readerSavedRect.top = GetPrivateProfileIntW(L"Window", L"Top", 0, path);
	g_readerSavedRect.right = GetPrivateProfileIntW(L"Window", L"Width", 0, path);
	g_readerSavedRect.bottom = GetPrivateProfileIntW(L"Window", L"Height", 0, path);
	value = g_readerSavedRect.right;
	if (value < 560 || g_readerSavedRect.bottom < 420) {
		memset(&g_readerSavedRect, 0, sizeof(g_readerSavedRect));
	}
}

static void WriteReaderProfileInt(LPCWSTR section, LPCWSTR key, int value, LPCWSTR path) {
	WCHAR text[32];
	StringCchPrintfW(text, 32, L"%d", value);
	WritePrivateProfileStringW(section, key, text, path);
}

static void SaveReaderSettings(void) {
	WCHAR path[MAX_PATH];
	RECT rect;

	GetReaderSettingsPath(path, MAX_PATH);
	if (g_readerWindow != NULL && IsWindow(g_readerWindow)) {
		GetWindowRect(g_readerWindow, &rect);
		WriteReaderProfileInt(L"Window", L"Left", rect.left, path);
		WriteReaderProfileInt(L"Window", L"Top", rect.top, path);
		WriteReaderProfileInt(L"Window", L"Width", rect.right - rect.left, path);
		WriteReaderProfileInt(L"Window", L"Height", rect.bottom - rect.top, path);
	}
	WriteReaderProfileInt(L"Reader", L"FontPoints", g_readerFontPoints, path);
	WriteReaderProfileInt(L"Reader", L"OpacityPercent", g_readerOpacityPercent, path);
}

static const WCHAR* EncodingName(READER_ENCODING encoding) {
	switch (encoding) {
	case READER_ENCODING_UTF8_BOM:
		return L"UTF-8 BOM";
	case READER_ENCODING_UTF16_LE:
		return L"UTF-16 LE";
	case READER_ENCODING_UTF16_BE:
		return L"UTF-16 BE";
	case READER_ENCODING_SYSTEM:
		return L"系统编码";
	default:
		return L"UTF-8";
	}
}

static WCHAR* NormalizeReaderText(const WCHAR* source, SIZE_T sourceLength, SIZE_T* outputLength) {
	SIZE_T i;
	SIZE_T j;
	WCHAR* output;

	if (outputLength == NULL) {
		return NULL;
	}
	*outputLength = 0;
	if (source == NULL) {
		sourceLength = 0;
	}
	if (sourceLength > (SIZE_T)(((SIZE_T)-1) / (2 * sizeof(WCHAR))) - 2) {
		return NULL;
	}
	output = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
		(sourceLength * 2 + 1) * sizeof(WCHAR));
	if (output == NULL) {
		return NULL;
	}

	j = 0;
	for (i = 0; i < sourceLength; i++) {
		WCHAR ch = source[i];
		if (ch == L'\r') {
			output[j++] = L'\r';
			output[j++] = L'\n';
			if (i + 1 < sourceLength && source[i + 1] == L'\n') {
				i++;
			}
		} else if (ch == L'\n') {
			output[j++] = L'\r';
			output[j++] = L'\n';
		} else {
			output[j++] = ch == L'\0' ? L'\uFFFD' : ch;
		}
	}
	output[j] = L'\0';
	*outputLength = j;
	return output;
}

static BOOL ReaderBytesLookLikeUtf16(const BYTE* bytes, SIZE_T size, int* littleEndian) {
	SIZE_T sample;
	SIZE_T i;
	SIZE_T leMatches;
	SIZE_T beMatches;

	if (littleEndian == NULL || bytes == NULL || size < 4) {
		return FALSE;
	}
	sample = size < 200 ? size : 200;
	leMatches = 0;
	beMatches = 0;
	for (i = 0; i + 1 < sample; i += 2) {
		if (bytes[i] != 0 && bytes[i + 1] == 0) {
			leMatches++;
		}
		if (bytes[i] == 0 && bytes[i + 1] != 0) {
			beMatches++;
		}
	}
	if (leMatches >= sample / 8 && leMatches > beMatches * 2) {
		*littleEndian = TRUE;
		return TRUE;
	}
	if (beMatches >= sample / 8 && beMatches > leMatches * 2) {
		*littleEndian = FALSE;
		return TRUE;
	}
	return FALSE;
}

static WCHAR* DecodeReaderText(const BYTE* bytes, SIZE_T size, READER_ENCODING* encoding,
	SIZE_T* characterCount) {
	const BYTE* data;
	SIZE_T dataSize;
	int count;
	WCHAR* text;
	int wideCount;
	SIZE_T i;
	int littleEndian;

	if (encoding == NULL || characterCount == NULL) {
		return NULL;
	}
	*characterCount = 0;
	if (bytes == NULL) {
		return NULL;
	}
	if (size == 0) {
		text = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WCHAR));
		if (text != NULL) {
			*encoding = READER_ENCODING_UTF8;
		}
		return text;
	}

	data = bytes;
	dataSize = size;
	*encoding = READER_ENCODING_UTF8;

	if (dataSize >= 3 && data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) {
		data += 3;
		dataSize -= 3;
		*encoding = READER_ENCODING_UTF8_BOM;
	} else if (dataSize >= 2 && data[0] == 0xFF && data[1] == 0xFE) {
		data += 2;
		dataSize -= 2;
		*encoding = READER_ENCODING_UTF16_LE;
	} else if (dataSize >= 2 && data[0] == 0xFE && data[1] == 0xFF) {
		data += 2;
		dataSize -= 2;
		*encoding = READER_ENCODING_UTF16_BE;
	} else if (ReaderBytesLookLikeUtf16(data, dataSize, &littleEndian)) {
		*encoding = littleEndian ? READER_ENCODING_UTF16_LE : READER_ENCODING_UTF16_BE;
	}

	if (*encoding == READER_ENCODING_UTF16_LE || *encoding == READER_ENCODING_UTF16_BE) {
		if (dataSize % sizeof(WCHAR) != 0) {
			dataSize--;
		}
		wideCount = (int)(dataSize / sizeof(WCHAR));
		text = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
			((SIZE_T)wideCount + 1) * sizeof(WCHAR));
		if (text == NULL) {
			return NULL;
		}
		memcpy(text, data, (SIZE_T)wideCount * sizeof(WCHAR));
		if (*encoding == READER_ENCODING_UTF16_BE) {
			for (i = 0; i < (SIZE_T)wideCount; i++) {
				text[i] = (WCHAR)(((text[i] & 0x00FF) << 8) | ((text[i] & 0xFF00) >> 8));
			}
		}
		*characterCount = (SIZE_T)wideCount;
		return text;
	}

	if (dataSize == 0) {
		text = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(WCHAR));
		if (text != NULL) {
			*characterCount = 0;
		}
		return text;
	}

	if (dataSize > (SIZE_T)INT_MAX) {
		return NULL;
	}
	count = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, (LPCCH)data,
		(int)dataSize, NULL, 0);
	if (count <= 0) {
		*encoding = READER_ENCODING_SYSTEM;
		count = MultiByteToWideChar(CP_ACP, 0, (LPCCH)data, (int)dataSize, NULL, 0);
	}
	if (count <= 0) {
		return NULL;
	}
	text = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
		((SIZE_T)count + 1) * sizeof(WCHAR));
	if (text == NULL) {
		return NULL;
	}
	if (MultiByteToWideChar(*encoding == READER_ENCODING_SYSTEM ? CP_ACP : CP_UTF8,
		0, (LPCCH)data, (int)dataSize, text, count) <= 0) {
		HeapFree(GetProcessHeap(), 0, text);
		return NULL;
	}
	*characterCount = (SIZE_T)count;
	return text;
}

static BOOL ReadTextFile(const WCHAR* path, WCHAR** normalizedText, READER_ENCODING* encoding,
	SIZE_T* outputLength, DWORD* errorCode) {
	HANDLE file;
	LARGE_INTEGER fileSize;
	BYTE* bytes;
	DWORD bytesRead;
	DWORD totalRead;
	WCHAR* decoded;
	SIZE_T decodedLength;

	if (errorCode != NULL) {
		*errorCode = ERROR_SUCCESS;
	}
	if (path == NULL || normalizedText == NULL || encoding == NULL || outputLength == NULL) {
		if (errorCode != NULL) {
			*errorCode = ERROR_INVALID_PARAMETER;
		}
		return FALSE;
	}
	*normalizedText = NULL;
	*outputLength = 0;

	file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
		NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		if (errorCode != NULL) {
			*errorCode = GetLastError();
		}
		return FALSE;
	}
	if (!GetFileSizeEx(file, &fileSize) || fileSize.QuadPart < 0 ||
		(ULONGLONG)fileSize.QuadPart > READER_MAX_FILE_BYTES) {
		if (errorCode != NULL) {
			*errorCode = ERROR_FILE_TOO_LARGE;
		}
		CloseHandle(file);
		return FALSE;
	}
	bytes = (BYTE*)HeapAlloc(GetProcessHeap(), 0, (SIZE_T)(fileSize.QuadPart + 1));
	if (bytes == NULL) {
		if (errorCode != NULL) {
			*errorCode = ERROR_NOT_ENOUGH_MEMORY;
		}
		CloseHandle(file);
		return FALSE;
	}

	totalRead = 0;
	while (totalRead < (DWORD)fileSize.QuadPart) {
		if (!ReadFile(file, bytes + totalRead, (DWORD)fileSize.QuadPart - totalRead,
				&bytesRead, NULL)) {
			if (errorCode != NULL) {
				*errorCode = GetLastError();
			}
			HeapFree(GetProcessHeap(), 0, bytes);
			CloseHandle(file);
			return FALSE;
		}
		if (bytesRead == 0) {
			break;
		}
		totalRead += bytesRead;
	}
	CloseHandle(file);

	decoded = DecodeReaderText(bytes, totalRead, encoding, &decodedLength);
	HeapFree(GetProcessHeap(), 0, bytes);
	if (decoded == NULL) {
		if (errorCode != NULL) {
			*errorCode = ERROR_NO_UNICODE_TRANSLATION;
		}
		return FALSE;
	}
	*normalizedText = NormalizeReaderText(decoded, decodedLength, outputLength);
	HeapFree(GetProcessHeap(), 0, decoded);
	if (*normalizedText == NULL) {
		if (errorCode != NULL) {
			*errorCode = ERROR_NOT_ENOUGH_MEMORY;
		}
		return FALSE;
	}
	return TRUE;
}

static const WCHAR* ReaderFileName(const WCHAR* path) {
	const WCHAR* slash;
	const WCHAR* forward;
	const WCHAR* name;

	name = path;
	slash = wcsrchr(path, L'\\');
	forward = wcsrchr(path, L'/');
	if (slash != NULL && slash + 1 > name) {
		name = slash + 1;
	}
	if (forward != NULL && forward + 1 > name) {
		name = forward + 1;
	}
	return name;
}

static void RecreateReaderFonts(HWND hwnd) {
	LOGFONTW logFont;
	int uiHeight;
	UINT dpi;

	dpi = GetReaderDpi(hwnd);
	g_readerDpi = dpi;
	if (g_readerTextFont != NULL) {
		DeleteObject(g_readerTextFont);
		g_readerTextFont = NULL;
	}
	if (g_readerUiFont != NULL) {
		DeleteObject(g_readerUiFont);
		g_readerUiFont = NULL;
	}

	memset(&logFont, 0, sizeof(logFont));
	logFont.lfHeight = -MulDiv(g_readerFontPoints, (int)dpi, 72);
	logFont.lfWeight = FW_NORMAL;
	logFont.lfCharSet = DEFAULT_CHARSET;
	logFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
	logFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	logFont.lfQuality = CLEARTYPE_QUALITY;
	logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	StringCchCopyW(logFont.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
	g_readerTextFont = CreateFontIndirectW(&logFont);
	if (g_readerTextFont == NULL) {
		g_readerTextFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	}

	memset(&logFont, 0, sizeof(logFont));
	uiHeight = -MulDiv(10, (int)dpi, 72);
	logFont.lfHeight = uiHeight;
	logFont.lfWeight = FW_NORMAL;
	logFont.lfCharSet = DEFAULT_CHARSET;
	logFont.lfQuality = CLEARTYPE_QUALITY;
	logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	StringCchCopyW(logFont.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
	g_readerUiFont = CreateFontIndirectW(&logFont);
	if (g_readerUiFont == NULL) {
		g_readerUiFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
	}
}

static void ApplyReaderFontAndColors(void) {
	CHARFORMAT2W format;

	if (g_readerEdit == NULL) {
		return;
	}
	SendMessageW(g_readerEdit, WM_SETFONT, (WPARAM)g_readerTextFont, TRUE);
	memset(&format, 0, sizeof(format));
	format.cbSize = sizeof(format);
	format.dwMask = CFM_FACE | CFM_SIZE | CFM_WEIGHT | CFM_COLOR | CFM_CHARSET;
	format.yHeight = g_readerFontPoints * 20;
	format.wWeight = FW_NORMAL;
	format.bCharSet = DEFAULT_CHARSET;
	format.crTextColor = READER_TEXT_COLOR;
	StringCchCopyW(format.szFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
	SendMessageW(g_readerEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);
}

static void UpdateReaderFontLabels(void) {
	WCHAR text[48];

	if (g_readerFontMinusButton != NULL) {
		StringCchPrintfW(text, 48, L"A−  %d pt", g_readerFontPoints);
		SetWindowTextW(g_readerFontMinusButton, text);
	}
	if (g_readerFontPlusButton != NULL) {
		StringCchPrintfW(text, 48, L"A+  %d pt", g_readerFontPoints);
		SetWindowTextW(g_readerFontPlusButton, text);
	}
}

static void ApplyReaderFontsToControls(void) {
	ApplyReaderFontAndColors();
	SendMessageW(g_readerOpenButton, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerFontMinusButton, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerFontPlusButton, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerOpacityCombo, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerStatus, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	UpdateReaderFontLabels();
}

static void LayoutReaderWindow(void) {
	RECT client;
	int width;
	int height;
	int padding;
	int toolbarHeight;
	int buttonHeight;
	int openWidth;
	int fontWidth;
	int opacityWidth;
	int gap;
	int x;
	int y;

	if (g_readerWindow == NULL || !GetClientRect(g_readerWindow, &client)) {
		return;
	}
	width = client.right - client.left;
	height = client.bottom - client.top;
	padding = ReaderScale(g_readerWindow, 12);
	toolbarHeight = ReaderScale(g_readerWindow, READER_TOOLBAR_HEIGHT);
	buttonHeight = ReaderScale(g_readerWindow, 32);
	openWidth = ReaderScale(g_readerWindow, 132);
	fontWidth = ReaderScale(g_readerWindow, 92);
	opacityWidth = ReaderScale(g_readerWindow, 112);
	gap = ReaderScale(g_readerWindow, 8);
	y = ReaderScale(g_readerWindow, 10);
	x = padding;

	MoveWindow(g_readerOpenButton, x, y, openWidth, buttonHeight, TRUE);
	x += openWidth + gap;
	MoveWindow(g_readerFontMinusButton, x, y, fontWidth, buttonHeight, TRUE);
	x += fontWidth + gap;
	MoveWindow(g_readerFontPlusButton, x, y, fontWidth, buttonHeight, TRUE);
	x += fontWidth + gap;
	MoveWindow(g_readerOpacityCombo, x, y + ReaderScale(g_readerWindow, 2),
		opacityWidth, ReaderScale(g_readerWindow, 280), TRUE);
	x += opacityWidth + gap * 2;
	MoveWindow(g_readerStatus, x, y,
		width - x - padding > 0 ? width - x - padding : 0,
		buttonHeight, TRUE);

	MoveWindow(g_readerEdit, padding, toolbarHeight,
		width - padding * 2 > 0 ? width - padding * 2 : 0,
		height - toolbarHeight - padding > 0 ? height - toolbarHeight - padding : 0,
		TRUE);
	InvalidateRect(g_readerWindow, NULL, TRUE);
}

static int ReaderOpacityFromCombo(void) {
	int index;
	LRESULT data;

	index = (int)SendMessageW(g_readerOpacityCombo, CB_GETCURSEL, 0, 0);
	if (index < 0) {
		return g_readerOpacityPercent;
	}
	data = SendMessageW(g_readerOpacityCombo, CB_GETITEMDATA, index, 0);
	return data >= 35 && data <= 100 ? (int)data : 100;
}

static void SelectReaderOpacityCombo(int percent) {
	int count;
	int i;

	count = (int)SendMessageW(g_readerOpacityCombo, CB_GETCOUNT, 0, 0);
	for (i = 0; i < count; i++) {
		if ((int)SendMessageW(g_readerOpacityCombo, CB_GETITEMDATA, i, 0) == percent) {
			SendMessageW(g_readerOpacityCombo, CB_SETCURSEL, i, 0);
			return;
		}
	}
}

static void ApplyReaderOpacity(int percent) {
	LONG_PTR style;

	if (percent < 35) {
		percent = 35;
	} else if (percent > 100) {
		percent = 100;
	}
	g_readerOpacityPercent = percent;
	if (g_readerWindow != NULL) {
		style = GetWindowLongPtrW(g_readerWindow, GWL_EXSTYLE);
		SetWindowLongPtrW(g_readerWindow, GWL_EXSTYLE, style | WS_EX_LAYERED);
		SetLayeredWindowAttributes(g_readerWindow, 0,
			(BYTE)MulDiv(percent, 255, 100), LWA_ALPHA);
	}
}

static void PopulateReaderOpacityCombo(void) {
	WCHAR text[32];
	int values[] = {100, 90, 80, 70, 60, 50, 40};
	int i;
	int index;

	for (i = 0; i < (int)(sizeof(values) / sizeof(values[0])); i++) {
		StringCchPrintfW(text, 32, L"不透明度 %d%%", values[i]);
		index = (int)SendMessageW(g_readerOpacityCombo, CB_ADDSTRING, 0, (LPARAM)text);
		if (index >= 0) {
			SendMessageW(g_readerOpacityCombo, CB_SETITEMDATA, (WPARAM)index,
				(LPARAM)values[i]);
		}
	}
	SelectReaderOpacityCombo(g_readerOpacityPercent);
}

static void ShowReaderLoadError(DWORD errorCode) {
	WCHAR message[256];
	WCHAR* systemMessage;

	systemMessage = NULL;
	if (errorCode == ERROR_FILE_TOO_LARGE) {
		StringCchCopyW(message, 256, L"这个文件超过 128 MB，暂时无法打开。");
	} else if (FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
			FORMAT_MESSAGE_IGNORE_INSERTS, NULL, errorCode, 0,
			(LPWSTR)&systemMessage, 0, NULL) != 0 && systemMessage != NULL) {
		StringCchPrintfW(message, 256, L"无法打开文件：\r\n%s", systemMessage);
		LocalFree(systemMessage);
	} else {
		StringCchPrintfW(message, 256, L"无法打开文件（错误代码 %lu）。", errorCode);
	}
	MessageBoxW(g_readerWindow, message, READER_TITLE, MB_OK | MB_ICONWARNING);
}

static BOOL LoadReaderFile(const WCHAR* path) {
	WCHAR* text;
	READER_ENCODING encoding;
	SIZE_T textLength;
	DWORD errorCode;
	SETTEXTEX setText;
	CHARRANGE range;
	WCHAR title[MAX_PATH + 64];
	WCHAR status[MAX_PATH + 96];
	const WCHAR* fileName;

	if (path == NULL || path[0] == L'\0') {
		return FALSE;
	}
	if (!ReadTextFile(path, &text, &encoding, &textLength, &errorCode)) {
		ShowReaderLoadError(errorCode);
		return FALSE;
	}

	StringCchCopyW(g_readerCurrentPath, MAX_PATH, path);
	memset(&setText, 0, sizeof(setText));
	setText.flags = ST_DEFAULT;
	setText.codepage = 1200;
	SendMessageW(g_readerEdit, EM_SETTEXTEX, (WPARAM)&setText, (LPARAM)text);
	HeapFree(GetProcessHeap(), 0, text);
	ApplyReaderFontAndColors();

	range.cpMin = 0;
	range.cpMax = 0;
	SendMessageW(g_readerEdit, EM_EXSETSEL, 0, (LPARAM)&range);
	SendMessageW(g_readerEdit, EM_SCROLLCARET, 0, 0);
	InvalidateRect(g_readerEdit, NULL, TRUE);

	fileName = ReaderFileName(path);
	StringCchPrintfW(title, MAX_PATH + 64, L"%s — " READER_TITLE, fileName);
	SetWindowTextW(g_readerWindow, title);
	StringCchPrintfW(status, MAX_PATH + 96,
		L"%s  ·  %s  ·  %Iu 字符  ·  自动换行", fileName, EncodingName(encoding),
		(size_t)textLength);
	SetWindowTextW(g_readerStatus, status);
	SetFocus(g_readerEdit);
	return TRUE;
}

static BOOL ShowReaderOpenDialog(void) {
	OPENFILENAMEW dialog;
	WCHAR file[MAX_PATH];

	file[0] = L'\0';
	if (g_readerCurrentPath[0] != L'\0') {
		StringCchCopyW(file, MAX_PATH, g_readerCurrentPath);
	}
	memset(&dialog, 0, sizeof(dialog));
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = g_readerWindow;
	dialog.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0所有文件 (*.*)\0*.*\0\0";
	dialog.lpstrFile = file;
	dialog.nMaxFile = MAX_PATH;
	dialog.lpstrDefExt = L"txt";
	dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY |
		OFN_EXPLORER;
	dialog.lpstrTitle = L"打开 TXT 文件";
	if (!GetOpenFileNameW(&dialog)) {
		return FALSE;
	}
	return LoadReaderFile(file);
}

static void ChangeReaderFont(int delta) {
	int newPoints;

	newPoints = g_readerFontPoints + delta;
	if (newPoints < 9) {
		newPoints = 9;
	} else if (newPoints > 42) {
		newPoints = 42;
	}
	if (newPoints == g_readerFontPoints) {
		return;
	}
	g_readerFontPoints = newPoints;
	RecreateReaderFonts(g_readerWindow);
	ApplyReaderFontsToControls();
	LayoutReaderWindow();
	SetFocus(g_readerEdit);
}

static LRESULT CALLBACK ReaderEditSubclassProc(HWND hwnd, UINT message, WPARAM wParam,
	LPARAM lParam, UINT_PTR subclassId, DWORD_PTR referenceData) {
	(void)subclassId;
	(void)referenceData;

	if (message == WM_KEYDOWN && GetKeyState(VK_CONTROL) < 0) {
		switch (wParam) {
		case 'O':
			ShowReaderOpenDialog();
			return 0;
		case VK_OEM_PLUS:
		case VK_ADD:
			ChangeReaderFont(1);
			return 0;
		case VK_OEM_MINUS:
		case VK_SUBTRACT:
			ChangeReaderFont(-1);
			return 0;
		}
	} else if (message == WM_MOUSEWHEEL && GetKeyState(VK_CONTROL) < 0) {
		ChangeReaderFont(GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1);
		return 0;
	}
	return DefSubclassProc(hwnd, message, wParam, lParam);
}

static void CreateReaderControls(HWND hwnd) {
	DWORD editStyle;

	g_readerBackgroundBrush = CreateSolidBrush(READER_BG_COLOR);
	g_readerToolbarBrush = CreateSolidBrush(READER_TOOLBAR_COLOR);
	RecreateReaderFonts(hwnd);

	g_readerOpenButton = CreateWindowExW(0, L"BUTTON", L"打开 TXT...",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_OPEN, g_readerInstance, NULL);
	g_readerFontMinusButton = CreateWindowExW(0, L"BUTTON", L"A−",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_FONT_MINUS, g_readerInstance, NULL);
	g_readerFontPlusButton = CreateWindowExW(0, L"BUTTON", L"A+",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_FONT_PLUS, g_readerInstance, NULL);
	g_readerOpacityCombo = CreateWindowExW(0, WC_COMBOBOXW, L"",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_OPACITY, g_readerInstance, NULL);
	g_readerStatus = CreateWindowExW(0, L"STATIC", L"打开一个 TXT 文件开始阅读",
		WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_STATUS, g_readerInstance, NULL);

	editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE |
		ES_READONLY | ES_NOHIDESEL | ES_AUTOVSCROLL;
	g_readerEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
		editStyle, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_EDIT,
		g_readerInstance, NULL);

	PopulateReaderOpacityCombo();
	ApplyReaderFontsToControls();
	ApplyReaderOpacity(g_readerOpacityPercent);
	SendMessageW(g_readerEdit, EM_SETBKGNDCOLOR, 0, (LPARAM)READER_BG_COLOR);
	SendMessageW(g_readerEdit, EM_SETLIMITTEXT, 0x7FFFFFF0, 0);
	SetWindowSubclass(g_readerEdit, ReaderEditSubclassProc, 1, 0);
	LayoutReaderWindow();
}

static void BuildReaderMenu(HWND hwnd) {
	HMENU menu;
	HMENU fileMenu;
	HMENU viewMenu;
	HMENU opacityMenu;
	WCHAR text[32];
	int percent;

	menu = CreateMenu();
	fileMenu = CreatePopupMenu();
	viewMenu = CreatePopupMenu();
	opacityMenu = CreatePopupMenu();
	if (menu == NULL || fileMenu == NULL || viewMenu == NULL || opacityMenu == NULL) {
		return;
	}
	AppendMenuW(fileMenu, MF_STRING, IDM_READER_OPEN, L"打开 TXT...\tCtrl+O");
	AppendMenuW(fileMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(fileMenu, MF_STRING, IDM_READER_EXIT, L"关闭阅读器");
	for (percent = 100; percent >= 40; percent -= 10) {
		StringCchPrintfW(text, 32, L"%d%%", percent);
		AppendMenuW(opacityMenu, MF_STRING, IDM_READER_OPACITY_BASE + percent, text);
	}
	AppendMenuW(viewMenu, MF_STRING, IDM_READER_FONT_PLUS, L"增大字体\tCtrl++");
	AppendMenuW(viewMenu, MF_STRING, IDM_READER_FONT_MINUS, L"减小字体\tCtrl+-");
	AppendMenuW(viewMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(viewMenu, MF_POPUP, (UINT_PTR)opacityMenu, L"不透明度");
	AppendMenuW(menu, MF_POPUP, (UINT_PTR)fileMenu, L"文件");
	AppendMenuW(menu, MF_POPUP, (UINT_PTR)viewMenu, L"视图");
	SetMenu(hwnd, menu);
	DrawMenuBar(hwnd);
}

static LRESULT CALLBACK TextReaderWndProc(HWND hwnd, UINT message, WPARAM wParam,
	LPARAM lParam) {
	switch (message) {
	case WM_CREATE:
		g_readerWindow = hwnd;
		BuildReaderMenu(hwnd);
		CreateReaderControls(hwnd);
		return 0;

	case WM_SIZE:
		LayoutReaderWindow();
		return 0;

	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = ReaderScale(hwnd, 560);
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = ReaderScale(hwnd, 420);
		return 0;

	case WM_DPICHANGED:
	{
		RECT* suggested = (RECT*)lParam;
		SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
			suggested->right - suggested->left, suggested->bottom - suggested->top,
			SWP_NOZORDER | SWP_NOACTIVATE);
		RecreateReaderFonts(hwnd);
		ApplyReaderFontsToControls();
		LayoutReaderWindow();
		return 0;
	}

	case WM_ERASEBKGND:
	{
		RECT rect;
		GetClientRect(hwnd, &rect);
		FillRect((HDC)wParam, &rect, g_readerBackgroundBrush);
		return 1;
	}

	case WM_CTLCOLORSTATIC:
		SetBkMode((HDC)wParam, TRANSPARENT);
		SetTextColor((HDC)wParam, READER_SUBTLE_COLOR);
		return (LRESULT)g_readerToolbarBrush;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_READER_OPEN:
		case IDM_READER_OPEN:
			ShowReaderOpenDialog();
			return 0;
		case IDC_READER_FONT_MINUS:
		case IDM_READER_FONT_MINUS:
			ChangeReaderFont(-1);
			return 0;
		case IDC_READER_FONT_PLUS:
		case IDM_READER_FONT_PLUS:
			ChangeReaderFont(1);
			return 0;
		case IDM_READER_EXIT:
			SendMessageW(hwnd, WM_CLOSE, 0, 0);
			return 0;
		case IDC_READER_OPACITY:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				ApplyReaderOpacity(ReaderOpacityFromCombo());
			}
			return 0;
		}
		if (LOWORD(wParam) >= IDM_READER_OPACITY_BASE + 40 &&
			LOWORD(wParam) <= IDM_READER_OPACITY_BASE + 100) {
			ApplyReaderOpacity((int)(LOWORD(wParam) - IDM_READER_OPACITY_BASE));
			SelectReaderOpacityCombo(g_readerOpacityPercent);
			return 0;
		}
		break;

	case WM_DROPFILES:
	{
		HDROP drop = (HDROP)wParam;
		WCHAR path[MAX_PATH];
		if (DragQueryFileW(drop, 0, path, MAX_PATH) > 0) {
			LoadReaderFile(path);
		}
		DragFinish(drop);
		return 0;
	}

	case WM_CLOSE:
		SaveReaderSettings();
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		if (g_readerEdit != NULL) {
			RemoveWindowSubclass(g_readerEdit, ReaderEditSubclassProc, 1);
		}
		g_readerWindow = NULL;
		g_readerEdit = NULL;
		g_readerOpenButton = NULL;
		g_readerFontMinusButton = NULL;
		g_readerFontPlusButton = NULL;
		g_readerOpacityCombo = NULL;
		g_readerStatus = NULL;
		if (g_readerTextFont != NULL && g_readerTextFont != GetStockObject(DEFAULT_GUI_FONT)) {
			DeleteObject(g_readerTextFont);
		}
		g_readerTextFont = NULL;
		if (g_readerUiFont != NULL && g_readerUiFont != GetStockObject(DEFAULT_GUI_FONT)) {
			DeleteObject(g_readerUiFont);
		}
		g_readerUiFont = NULL;
		if (g_readerBackgroundBrush != NULL) {
			DeleteObject(g_readerBackgroundBrush);
			g_readerBackgroundBrush = NULL;
		}
		if (g_readerToolbarBrush != NULL) {
			DeleteObject(g_readerToolbarBrush);
			g_readerToolbarBrush = NULL;
		}
		return 0;
	}
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

BOOL InitializeTextReader(HINSTANCE instance) {
	WNDCLASSEXW windowClass;

	if (g_readerClassRegistered) {
		return TRUE;
	}
	g_richEditLibrary = LoadLibraryW(L"Msftedit.dll");
	if (g_richEditLibrary == NULL) {
		MessageBoxW(NULL, L"无法加载系统文本阅读组件 Msftedit.dll。",
			READER_TITLE, MB_OK | MB_ICONERROR);
		return FALSE;
	}
	g_readerInstance = instance;
	memset(&windowClass, 0, sizeof(windowClass));
	windowClass.cbSize = sizeof(windowClass);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = TextReaderWndProc;
	windowClass.hInstance = instance;
	windowClass.hIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(IDI_ICON1),
		IMAGE_ICON, 32, 32, LR_SHARED);
	windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
	windowClass.lpszClassName = READER_CLASS_NAME;
	if (!RegisterClassExW(&windowClass)) {
		FreeLibrary(g_richEditLibrary);
		g_richEditLibrary = NULL;
		return FALSE;
	}
	LoadReaderSettings();
	g_readerClassRegistered = TRUE;
	return TRUE;
}

void CleanupTextReader(void) {
	DestroyTextReaderWindow();
	if (g_richEditLibrary != NULL) {
		FreeLibrary(g_richEditLibrary);
		g_richEditLibrary = NULL;
	}
	if (g_readerClassRegistered) {
		UnregisterClassW(READER_CLASS_NAME, g_readerInstance);
		g_readerClassRegistered = FALSE;
	}
}

BOOL OpenTextReaderWindow(HWND owner, LPCTSTR filePath) {
	HMONITOR monitor;
	MONITORINFO monitorInfo;
	POINT savedPoint;
	int width;
	int height;
	int x;
	int y;

	(void)owner;
	if (!g_readerClassRegistered) {
		return FALSE;
	}
	if (g_readerWindow != NULL && IsWindow(g_readerWindow)) {
		ShowWindow(g_readerWindow, SW_RESTORE);
		SetForegroundWindow(g_readerWindow);
		if (filePath != NULL && filePath[0] != L'\0') {
			return LoadReaderFile(filePath);
		}
		return TRUE;
	}

	width = ReaderScale(NULL, 960);
	height = ReaderScale(NULL, 720);
	if (g_readerSavedRect.right >= 560 && g_readerSavedRect.bottom >= 420) {
		width = g_readerSavedRect.right;
		height = g_readerSavedRect.bottom;
		x = g_readerSavedRect.left;
		y = g_readerSavedRect.top;
		savedPoint.x = x;
		savedPoint.y = y;
		monitor = MonitorFromPoint(savedPoint, MONITOR_DEFAULTTONEAREST);
		memset(&monitorInfo, 0, sizeof(monitorInfo));
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfoW(monitor, &monitorInfo) &&
			(x + width > monitorInfo.rcWork.right ||
			 y + height > monitorInfo.rcWork.bottom ||
			 x < monitorInfo.rcWork.left || y < monitorInfo.rcWork.top)) {
			x = monitorInfo.rcWork.left + (monitorInfo.rcWork.right - monitorInfo.rcWork.left - width) / 2;
			y = monitorInfo.rcWork.top + (monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - height) / 2;
		}
	} else {
		x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
		y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	}

	g_readerWindow = CreateWindowExW(WS_EX_APPWINDOW, READER_CLASS_NAME, READER_TITLE,
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, x, y, width, height,
		NULL, NULL, g_readerInstance, NULL);
	if (g_readerWindow == NULL) {
		return FALSE;
	}
	DragAcceptFiles(g_readerWindow, TRUE);
	ShowWindow(g_readerWindow, SW_SHOW);
	UpdateWindow(g_readerWindow);
	SetForegroundWindow(g_readerWindow);
	if (filePath != NULL && filePath[0] != L'\0') {
		return LoadReaderFile(filePath);
	}
	SetFocus(g_readerEdit);
	return TRUE;
}

BOOL ExtractTextReaderPathFromCommandLine(LPCTSTR commandLine, LPTSTR filePath, int filePathCount) {
	size_t length;
	LPCTSTR start;
	LPCTSTR end;

	if (filePath == NULL || filePathCount < 2) {
		return FALSE;
	}
	filePath[0] = L'\0';
	if (commandLine == NULL) {
		return FALSE;
	}
	while (*commandLine == L' ' || *commandLine == L'\t') {
		commandLine++;
	}
	if (*commandLine == L'\0' ||
		lstrcmpiW(commandLine, L"--startup") == 0 ||
		lstrcmpiW(commandLine, L"/startup") == 0) {
		return FALSE;
	}
	start = commandLine;
	if (*start == L'"') {
		start++;
		end = wcschr(start, L'"');
		if (end == NULL) {
			end = start + wcslen(start);
		}
	} else {
		end = start + wcslen(start);
		while (end > start && (end[-1] == L' ' || end[-1] == L'\t')) {
			end--;
		}
	}
	length = (size_t)(end - start);
	if (length == 0 || length >= (size_t)filePathCount) {
		return FALSE;
	}
	memcpy(filePath, start, length * sizeof(WCHAR));
	filePath[length] = L'\0';
	return TRUE;
}

BOOL OpenTextReaderFromCommandLine(HWND owner, LPCTSTR commandLine) {
	WCHAR path[MAX_PATH];

	if (!ExtractTextReaderPathFromCommandLine(commandLine, path, MAX_PATH)) {
		return OpenTextReaderWindow(owner, NULL);
	}
	return OpenTextReaderWindow(owner, path);
}

void BringTextReaderToFront(void) {
	if (g_readerWindow != NULL && IsWindow(g_readerWindow)) {
		if (IsIconic(g_readerWindow)) {
			ShowWindow(g_readerWindow, SW_RESTORE);
		} else {
			ShowWindow(g_readerWindow, SW_SHOW);
		}
		SetForegroundWindow(g_readerWindow);
	} else {
		OpenTextReaderWindow(NULL, NULL);
	}
}

void DestroyTextReaderWindow(void) {
	if (g_readerWindow != NULL && IsWindow(g_readerWindow)) {
		SaveReaderSettings();
		DestroyWindow(g_readerWindow);
	}
}
