#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <richedit.h>
#include <shellapi.h>
#include <strsafe.h>
#include "resource.h"
#include "VitriteReader.h"
#include "VitriteLibrary.h"
#include "VitriteUi.h"

#define READER_CLASS_NAME L"VitriteTextReaderWindow"
#define READER_TITLE L"Vitrite TXT 阅读器"
#define READER_SETTINGS_NAME L"reader.ini"
#define READER_TOOLBAR_HEIGHT 108
#define READER_MAX_FILE_BYTES (128ULL * 1024ULL * 1024ULL)

#define IDC_READER_OPEN 600
#define IDC_READER_FONT_MINUS 601
#define IDC_READER_FONT_PLUS 602
#define IDC_READER_OPACITY 603
#define IDC_READER_STATUS 604
#define IDC_READER_EDIT 605
#define IDC_READER_RECENT 606
#define IDC_READER_LIBRARY 607
#define IDC_READER_WORD_COUNT 608
#define IDC_READER_PROGRESS 609
#define IDC_READER_PIN 610
#define IDC_READER_FONT_SIZE 611

#define IDM_READER_OPEN 700
#define IDM_READER_FONT_MINUS 701
#define IDM_READER_FONT_PLUS 702
#define IDM_READER_LIBRARY 703
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
static HWND g_readerLibraryButton;
static HWND g_readerFontMinusButton;
static HWND g_readerFontPlusButton;
static HWND g_readerFontSizeLabel;
static HWND g_readerOpacityCombo;
static HWND g_readerStatus;
static HWND g_readerWordInfo;
static HWND g_readerProgressInfo;
static HWND g_readerPinButton;
static HFONT g_readerTextFont;
static HFONT g_readerUiFont;
static HFONT g_readerTitleFont;
static HBRUSH g_readerBackgroundBrush;
static HBRUSH g_readerToolbarBrush;
static VITRITE_UI_THEME g_readerTheme;
static BOOL g_readerThemeReady;
static int g_readerSeparator1;
static int g_readerSeparator2;
static WCHAR g_readerCurrentPath[MAX_PATH];
static WCHAR g_readerBookTitle[192];
static RECT g_readerSavedRect;
static SIZE_T g_readerCharacterCount;
static int g_readerFontPoints = 16;
static int g_readerOpacityPercent = 100;
static BOOL g_readerTopmost;
static UINT g_readerDpi = 96;
static BOOL g_readerClassRegistered;

typedef enum READER_BUTTON_STYLE {
	READER_BUTTON_PRIMARY,
	READER_BUTTON_SECONDARY,
	READER_BUTTON_SEGMENT
} READER_BUTTON_STYLE;

#define READER_BUTTON_HOVER_PROP L"VitriteControlHover"

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
	if (g_readerOpacityPercent < 10) {
		g_readerOpacityPercent = 10;
	} else if (g_readerOpacityPercent > 100) {
		g_readerOpacityPercent = 100;
	}
	g_readerOpacityPercent = ((g_readerOpacityPercent + 5) / 10) * 10;
	if (g_readerOpacityPercent > 100) {
		g_readerOpacityPercent = 100;
	}
	g_readerTopmost = GetPrivateProfileIntW(L"Reader", L"AlwaysOnTop", 0, path) != 0;

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
	if (g_readerWindow != NULL && IsWindow(g_readerWindow)) {
		g_readerTopmost = IsWindowPinnedTopmost(g_readerWindow);
	}
	WriteReaderProfileInt(L"Reader", L"AlwaysOnTop", g_readerTopmost ? 1 : 0, path);
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
	if (g_readerTitleFont != NULL) {
		DeleteObject(g_readerTitleFont);
		g_readerTitleFont = NULL;
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

	memset(&logFont, 0, sizeof(logFont));
	logFont.lfHeight = -MulDiv(15, (int)dpi, 72);
	logFont.lfWeight = FW_SEMIBOLD;
	logFont.lfCharSet = DEFAULT_CHARSET;
	logFont.lfQuality = CLEARTYPE_QUALITY;
	logFont.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
	StringCchCopyW(logFont.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
	g_readerTitleFont = CreateFontIndirectW(&logFont);
	if (g_readerTitleFont == NULL) {
		g_readerTitleFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
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
	if (!g_readerThemeReady) {
		GetVitriteUiTheme(g_readerWindow, &g_readerTheme);
		g_readerThemeReady = TRUE;
	}
	format.crTextColor = g_readerTheme.text;
	StringCchCopyW(format.szFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
	SendMessageW(g_readerEdit, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);
}

static void UpdateReaderFontLabels(void) {
	WCHAR text[48];

	if (g_readerFontMinusButton != NULL) {
		SetWindowTextW(g_readerFontMinusButton, L"A−");
	}
	if (g_readerFontPlusButton != NULL) {
		SetWindowTextW(g_readerFontPlusButton, L"A+");
	}
	if (g_readerFontSizeLabel != NULL) {
		StringCchPrintfW(text, 48, L"%d pt", g_readerFontPoints);
		SetWindowTextW(g_readerFontSizeLabel, text);
	}
}

static void ReaderDisplayTitle(const WCHAR* fileName, LPWSTR title, int titleCount) {
	LPWSTR extension;
	StringCchCopyW(title, (size_t)titleCount, fileName);
	extension = wcsrchr(title, L'.');
	if (extension != NULL && _wcsicmp(extension, L".txt") == 0) {
		*extension = L'\0';
	}
}

static SIZE_T CountReaderCharacters(const WCHAR* text, SIZE_T length) {
	SIZE_T count = 0;
	SIZE_T i;
	for (i = 0; i < length; i++) {
		WCHAR ch = text[i];
		if (ch != L' ' && ch != L'\t' && ch != L'\r' && ch != L'\n' &&
			ch != L'\x3000' && ch != L'\0') {
			count++;
		}
	}
	return count;
}

static void UpdateReaderProgress(void) {
	WCHAR progressText[64];
	int firstLine;
	int lineCount;
	int visibleLines;
	int maxFirstLine;
	int progress;
	RECT rect;
	HDC dc;
	HFONT oldFont;
	TEXTMETRICW metrics;

	if (g_readerProgressInfo == NULL) {
		return;
	}
	if (g_readerEdit == NULL || g_readerCurrentPath[0] == L'\0') {
		SetWindowTextW(g_readerProgressInfo, L"进度：0%");
		return;
	}
	GetClientRect(g_readerEdit, &rect);
	dc = GetDC(g_readerEdit);
	oldFont = (HFONT)SelectObject(dc, g_readerTextFont);
	GetTextMetricsW(dc, &metrics);
	SelectObject(dc, oldFont);
	ReleaseDC(g_readerEdit, dc);
	visibleLines = max(1, (rect.bottom - rect.top - 4) /
		max(1, metrics.tmHeight + metrics.tmExternalLeading));
	lineCount = (int)SendMessageW(g_readerEdit, EM_GETLINECOUNT, 0, 0);
	firstLine = (int)SendMessageW(g_readerEdit, EM_GETFIRSTVISIBLELINE, 0, 0);
	maxFirstLine = max(0, lineCount - visibleLines);
	progress = maxFirstLine > 0
		? (firstLine * 100 + maxFirstLine / 2) / maxFirstLine
		: 100;
	progress = max(0, min(100, progress));
	StringCchPrintfW(progressText, 64, L"进度：%d%%", progress);
	SetWindowTextW(g_readerProgressInfo, progressText);
}

static void UpdateReaderInformation(void) {
	WCHAR wordText[64];
	if (g_readerStatus != NULL) {
		SetWindowTextW(g_readerStatus,
			g_readerBookTitle[0] ? g_readerBookTitle : L"未打开书籍");
	}
	if (g_readerWordInfo != NULL) {
		StringCchPrintfW(wordText, 64, L"字数：%Iu",
			(size_t)g_readerCharacterCount);
		SetWindowTextW(g_readerWordInfo, wordText);
	}
	UpdateReaderProgress();
}

static void ToggleReaderTopmost(void) {
	g_readerTopmost = !IsWindowPinnedTopmost(g_readerWindow);
	SetWindowPinnedTopmost(g_readerWindow, g_readerTopmost);
	if (g_readerPinButton != NULL) {
		InvalidateRect(g_readerPinButton, NULL, TRUE);
	}
}

static void ApplyReaderFontsToControls(void) {
	ApplyReaderFontAndColors();
	SendMessageW(g_readerOpenButton, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerLibraryButton, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerFontMinusButton, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerFontPlusButton, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerFontSizeLabel, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerOpacityCombo, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerStatus, WM_SETFONT, (WPARAM)g_readerTitleFont, TRUE);
	SendMessageW(g_readerWordInfo, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	SendMessageW(g_readerProgressInfo, WM_SETFONT, (WPARAM)g_readerUiFont, TRUE);
	UpdateReaderFontLabels();
}

static READER_BUTTON_STYLE ReaderButtonStyleForId(UINT id) {
	switch (id) {
	case IDC_READER_OPEN:
		return READER_BUTTON_PRIMARY;
	case IDC_READER_FONT_MINUS:
	case IDC_READER_FONT_PLUS:
		return READER_BUTTON_SEGMENT;
	default:
		return READER_BUTTON_SECONDARY;
	}
}

static LRESULT CALLBACK ReaderButtonSubclassProc(HWND hwnd, UINT message,
	WPARAM wParam, LPARAM lParam, UINT_PTR subclassId, DWORD_PTR referenceData) {
	(void)subclassId;
	(void)referenceData;
	if (message == WM_MOUSEMOVE) {
		TRACKMOUSEEVENT tracking;
		if (GetPropW(hwnd, READER_BUTTON_HOVER_PROP) == NULL) {
			SetPropW(hwnd, READER_BUTTON_HOVER_PROP, (HANDLE)(ULONG_PTR)1);
			InvalidateRect(hwnd, NULL, FALSE);
		}
		memset(&tracking, 0, sizeof(tracking));
		tracking.cbSize = sizeof(tracking);
		tracking.dwFlags = TME_LEAVE;
		tracking.hwndTrack = hwnd;
		TrackMouseEvent(&tracking);
	} else if (message == WM_MOUSELEAVE) {
		RemovePropW(hwnd, READER_BUTTON_HOVER_PROP);
		InvalidateRect(hwnd, NULL, FALSE);
	} else if (message == WM_SETFOCUS || message == WM_KILLFOCUS ||
		message == WM_ENABLE) {
		InvalidateRect(hwnd, NULL, FALSE);
	}
	return DefSubclassProc(hwnd, message, wParam, lParam);
}

static void DrawReaderToolbarButton(const DRAWITEMSTRUCT* item) {
	VITRITE_UI_THEME theme;
	READER_BUTTON_STYLE style;
	HBRUSH fillBrush;
	HPEN borderPen;
	HGDIOBJ oldBrush;
	HGDIOBJ oldPen;
	HFONT oldFont;
	WCHAR text[128];
	RECT rect;
	COLORREF fill;
	COLORREF border;
	COLORREF textColor;
	BOOL hovered;
	BOOL pressed;
	BOOL disabled;

	if (item == NULL || item->hDC == NULL) {
		return;
	}
	GetVitriteUiTheme(item->hwndItem, &theme);
	style = ReaderButtonStyleForId(item->CtlID);
	hovered = GetPropW(item->hwndItem, READER_BUTTON_HOVER_PROP) != NULL;
	pressed = (item->itemState & ODS_SELECTED) != 0;
	disabled = (item->itemState & ODS_DISABLED) != 0;
	fill = style == READER_BUTTON_PRIMARY ? theme.accent : theme.button;
	border = style == READER_BUTTON_PRIMARY ? theme.accent : theme.border;
	textColor = style == READER_BUTTON_PRIMARY ? theme.accentText : theme.text;
	if (hovered) {
		fill = style == READER_BUTTON_PRIMARY ? theme.accentHover : theme.buttonHover;
		if (theme.highContrast && style != READER_BUTTON_PRIMARY) {
			fill = theme.accent;
			border = theme.accent;
			textColor = theme.accentText;
		}
	}
	if (pressed) {
		fill = theme.buttonPressed;
		if (style == READER_BUTTON_PRIMARY || theme.highContrast) {
			fill = theme.accentHover;
			textColor = theme.accentText;
		}
	}
	if (disabled) {
		fill = theme.headerBackground;
		textColor = theme.mutedText;
		border = theme.separator;
	}
	rect = item->rcItem;
	fillBrush = CreateSolidBrush(fill);
	borderPen = CreatePen(PS_SOLID, 1, border);
	oldBrush = SelectObject(item->hDC, fillBrush);
	oldPen = SelectObject(item->hDC, borderPen);
	RoundRect(item->hDC, rect.left, rect.top, rect.right, rect.bottom, 8, 8);
	SelectObject(item->hDC, oldPen);
	SelectObject(item->hDC, oldBrush);
	DeleteObject(borderPen);
	DeleteObject(fillBrush);

	GetWindowTextW(item->hwndItem, text, 128);
	SetBkMode(item->hDC, TRANSPARENT);
	SetTextColor(item->hDC, textColor);
	oldFont = (HFONT)SelectObject(item->hDC, g_readerUiFont);
	InflateRect(&rect, -6, -2);
	DrawTextW(item->hDC, text, -1, &rect,
		DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
	SelectObject(item->hDC, oldFont);
	if (item->itemState & ODS_FOCUS) {
		HPEN focusPen = CreatePen(PS_SOLID, 2, theme.focus);
		HGDIOBJ previous = SelectObject(item->hDC, focusPen);
		HGDIOBJ previousBrush = SelectObject(item->hDC,
			GetStockObject(NULL_BRUSH));
		RoundRect(item->hDC, rect.left + 1, rect.top + 1,
			rect.right - 1, rect.bottom - 1, 6, 6);
		SelectObject(item->hDC, previousBrush);
		SelectObject(item->hDC, previous);
		DeleteObject(focusPen);
	}
}

static void ApplyReaderSystemTheme(void) {
	typedef HRESULT (WINAPI *PFN_SetWindowTheme)(HWND, LPCWSTR, LPCWSTR);
	HMODULE uxtheme;
	PFN_SetWindowTheme setWindowTheme;

	GetVitriteUiTheme(g_readerWindow, &g_readerTheme);
	g_readerThemeReady = TRUE;
	if (g_readerBackgroundBrush != NULL) {
		DeleteObject(g_readerBackgroundBrush);
	}
	if (g_readerToolbarBrush != NULL) {
		DeleteObject(g_readerToolbarBrush);
	}
	g_readerBackgroundBrush = CreateSolidBrush(g_readerTheme.windowBackground);
	g_readerToolbarBrush = CreateSolidBrush(g_readerTheme.headerBackground);
	if (g_readerEdit != NULL) {
		SendMessageW(g_readerEdit, EM_SETBKGNDCOLOR, 0,
			(LPARAM)g_readerTheme.surfaceBackground);
	}
	ApplyWindowDarkTitleBar(g_readerWindow,
		g_readerTheme.dark && !g_readerTheme.highContrast);
	ApplyReaderFontsToControls();

	uxtheme = LoadLibraryW(L"uxtheme.dll");
	setWindowTheme = uxtheme != NULL
		? (PFN_SetWindowTheme)GetProcAddress(uxtheme, "SetWindowTheme")
		: NULL;
	if (setWindowTheme != NULL && g_readerOpacityCombo != NULL) {
		setWindowTheme(g_readerOpacityCombo,
			g_readerTheme.dark && !g_readerTheme.highContrast
				? L"DarkMode_Explorer" : NULL, NULL);
	}
	RedrawWindow(g_readerWindow, NULL, NULL,
		RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_ALLCHILDREN);
}

static void LayoutReaderWindow(void) {
	RECT client;
	RECT editorRect;
	int width;
	int height;
	int padding;
	int toolbarHeight;
	int buttonHeight;
	int groupGap;
	int openWidth;
	int libraryWidth;
	int fontWidth;
	int fontSizeWidth;
	int opacityWidth;
	int pinSize;
	int wordWidth;
	int progressWidth;
	int infoTop;
	int infoHeight;
	int gap;
	int primaryEnd;
	int typeStart;
	int typeEnd;
	int readingWidth;
	int readingX;
	int x;
	int y;
	BOOL compact;

	if (g_readerWindow == NULL || !GetClientRect(g_readerWindow, &client)) {
		return;
	}
	width = client.right - client.left;
	height = client.bottom - client.top;
	compact = width < ReaderScale(g_readerWindow, 760);
	padding = ReaderScale(g_readerWindow, compact ? 12 : 16);
	toolbarHeight = ReaderScale(g_readerWindow, READER_TOOLBAR_HEIGHT);
	buttonHeight = ReaderScale(g_readerWindow, 34);
	groupGap = ReaderScale(g_readerWindow, compact ? 14 : 18);
	openWidth = ReaderScale(g_readerWindow, compact ? 92 : 112);
	libraryWidth = ReaderScale(g_readerWindow, compact ? 60 : 72);
	fontWidth = ReaderScale(g_readerWindow, compact ? 34 : 36);
	fontSizeWidth = ReaderScale(g_readerWindow, compact ? 48 : 54);
	opacityWidth = ReaderScale(g_readerWindow, compact ? 96 : 112);
	pinSize = ReaderScale(g_readerWindow, 34);
	wordWidth = ReaderScale(g_readerWindow, compact ? 108 : 120);
	progressWidth = ReaderScale(g_readerWindow, compact ? 104 : 116);
	gap = ReaderScale(g_readerWindow, compact ? 6 : 8);
	y = ReaderScale(g_readerWindow, 10);
	x = padding;

	MoveWindow(g_readerOpenButton, x, y, openWidth, buttonHeight, TRUE);
	x += openWidth + gap;
	MoveWindow(g_readerLibraryButton, x, y, libraryWidth, buttonHeight, TRUE);
	primaryEnd = x + libraryWidth;
	g_readerSeparator1 = primaryEnd + groupGap / 2;
	typeStart = primaryEnd + groupGap;
	x = typeStart;
	MoveWindow(g_readerFontMinusButton, x, y, fontWidth, buttonHeight, TRUE);
	x += fontWidth + ReaderScale(g_readerWindow, 4);
	MoveWindow(g_readerFontSizeLabel, x, y, fontSizeWidth, buttonHeight, TRUE);
	x += fontSizeWidth + ReaderScale(g_readerWindow, 4);
	MoveWindow(g_readerFontPlusButton, x, y, fontWidth, buttonHeight, TRUE);
	typeEnd = x + fontWidth;
	g_readerSeparator2 = typeEnd + groupGap / 2;
	x = width - padding - pinSize - gap - opacityWidth;
	MoveWindow(g_readerOpacityCombo, x, y + ReaderScale(g_readerWindow, 2),
		opacityWidth, ReaderScale(g_readerWindow, 280), TRUE);
	MoveWindow(g_readerPinButton, width - padding - pinSize, y,
		pinSize, buttonHeight, TRUE);

	infoTop = y + buttonHeight + ReaderScale(g_readerWindow, 11);
	infoHeight = ReaderScale(g_readerWindow, 30);
	x = padding;
	MoveWindow(g_readerStatus, x + ReaderScale(g_readerWindow, 2), infoTop,
		max(0, width - padding * 2 - wordWidth - progressWidth - gap * 2 -
			ReaderScale(g_readerWindow, 2)),
		infoHeight, TRUE);
	x = width - padding - progressWidth - gap - wordWidth;
	MoveWindow(g_readerWordInfo, x, infoTop, wordWidth, infoHeight, TRUE);
	x = width - padding - progressWidth;
	MoveWindow(g_readerProgressInfo, x, infoTop, progressWidth,
		infoHeight, TRUE);

	readingWidth = min(max(0, width - padding * 2),
		ReaderScale(g_readerWindow, 1040));
	readingX = (width - readingWidth) / 2;
	MoveWindow(g_readerEdit, readingX, toolbarHeight,
		readingWidth,
		max(0, height - toolbarHeight - padding), TRUE);
	if (g_readerEdit != NULL && GetClientRect(g_readerEdit, &editorRect)) {
		RECT formatRect;
		formatRect.left = ReaderScale(g_readerWindow, 20);
		formatRect.top = ReaderScale(g_readerWindow, 16);
		formatRect.right = max(formatRect.left,
			editorRect.right - ReaderScale(g_readerWindow, 20));
		formatRect.bottom = max(formatRect.top,
			editorRect.bottom - ReaderScale(g_readerWindow, 16));
		SendMessageW(g_readerEdit, EM_SETRECT, 0, (LPARAM)&formatRect);
	}
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
	return data >= 10 && data <= 100 ? (int)data : 100;
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

	if (percent < 10) {
		percent = 10;
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
	int values[] = {100, 90, 80, 70, 60, 50, 40, 30, 20, 10};
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
	const WCHAR* fileName;

	if (path == NULL || path[0] == L'\0') {
		return FALSE;
	}
	if (!ReadTextFile(path, &text, &encoding, &textLength, &errorCode)) {
		ShowReaderLoadError(errorCode);
		return FALSE;
	}
	(void)encoding;

	StringCchCopyW(g_readerCurrentPath, MAX_PATH, path);
	g_readerCharacterCount = CountReaderCharacters(text, textLength);
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
	ReaderDisplayTitle(fileName, g_readerBookTitle, 192);
	StringCchPrintfW(title, MAX_PATH + 64, L"%s — " READER_TITLE, fileName);
	SetWindowTextW(g_readerWindow, title);
	UpdateReaderInformation();
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
		case 'B':
			ShowLibraryWindow(g_readerWindow);
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

	RecreateReaderFonts(hwnd);

	g_readerOpenButton = CreateWindowExW(0, L"BUTTON", L"打开 TXT...",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_OPEN, g_readerInstance, NULL);
	g_readerLibraryButton = CreateWindowExW(0, L"BUTTON", L"书架",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_LIBRARY, g_readerInstance, NULL);
	g_readerFontMinusButton = CreateWindowExW(0, L"BUTTON", L"A−",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_FONT_MINUS, g_readerInstance, NULL);
	g_readerFontSizeLabel = CreateWindowExW(0, L"STATIC", L"16 pt",
		WS_CHILD | WS_VISIBLE | SS_CENTER | SS_CENTERIMAGE,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_FONT_SIZE,
		g_readerInstance, NULL);
	g_readerFontPlusButton = CreateWindowExW(0, L"BUTTON", L"A+",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_FONT_PLUS, g_readerInstance, NULL);
	g_readerOpacityCombo = CreateWindowExW(0, WC_COMBOBOXW, L"",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_OPACITY, g_readerInstance, NULL);
	g_readerPinButton = CreateWindowExW(0, L"BUTTON", L"固定在最前",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_PIN,
		g_readerInstance, NULL);
	g_readerStatus = CreateWindowExW(0, L"STATIC", L"未打开书籍",
		WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_STATUS, g_readerInstance, NULL);
	g_readerWordInfo = CreateWindowExW(0, L"STATIC", L"字数：0",
		WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_WORD_COUNT,
		g_readerInstance, NULL);
	g_readerProgressInfo = CreateWindowExW(0, L"STATIC", L"进度：0%",
		WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_PROGRESS,
		g_readerInstance, NULL);

	editStyle = WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_VSCROLL | ES_MULTILINE |
		ES_READONLY | ES_NOHIDESEL | ES_AUTOVSCROLL;
	g_readerEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"RICHEDIT50W", L"",
		editStyle, 0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_READER_EDIT,
		g_readerInstance, NULL);

	PopulateReaderOpacityCombo();
	ApplyReaderSystemTheme();
	ApplyReaderOpacity(g_readerOpacityPercent);
	SendMessageW(g_readerEdit, EM_SETLIMITTEXT, 0x7FFFFFF0, 0);
	SendMessageW(g_readerEdit, EM_SETEVENTMASK, 0,
		ENM_SCROLL | ENM_SCROLLEVENTS | ENM_SELCHANGE);
	SetWindowSubclass(g_readerEdit, ReaderEditSubclassProc, 1, 0);
	SetWindowSubclass(g_readerOpenButton, ReaderButtonSubclassProc, 11, 0);
	SetWindowSubclass(g_readerLibraryButton, ReaderButtonSubclassProc, 12, 0);
	SetWindowSubclass(g_readerFontMinusButton, ReaderButtonSubclassProc, 13, 0);
	SetWindowSubclass(g_readerFontPlusButton, ReaderButtonSubclassProc, 14, 0);
	SetWindowSubclass(g_readerPinButton, ReaderButtonSubclassProc, 15, 0);
	LayoutReaderWindow();
	SetWindowPinnedTopmost(hwnd, g_readerTopmost);
	UpdateReaderInformation();
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
	AppendMenuW(fileMenu, MF_STRING, IDM_READER_LIBRARY, L"打开书架目录...\tCtrl+B");
	AppendMenuW(fileMenu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(fileMenu, MF_STRING, IDM_READER_EXIT, L"关闭阅读器");
	for (percent = 100; percent >= 10; percent -= 10) {
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
		ApplyRoundedWindowCorners(hwnd);
		return 0;

	case WM_SIZE:
		LayoutReaderWindow();
		UpdateReaderProgress();
		ApplyRoundedWindowCorners(hwnd);
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
		UpdateReaderProgress();
		return 0;
	}

	case WM_ERASEBKGND:
	{
		RECT rect;
		RECT header;
		GetClientRect(hwnd, &rect);
		FillRect((HDC)wParam, &rect, g_readerBackgroundBrush);
		header = rect;
		header.bottom = min(header.bottom,
			ReaderScale(hwnd, READER_TOOLBAR_HEIGHT));
		FillRect((HDC)wParam, &header, g_readerToolbarBrush);
		return 1;
	}

	case WM_PAINT:
	{
		PAINTSTRUCT paint;
		HDC dc = BeginPaint(hwnd, &paint);
		HPEN pen = CreatePen(PS_SOLID, 1, g_readerTheme.separator);
		HGDIOBJ oldPen = SelectObject(dc, pen);
		int top = ReaderScale(hwnd, 17);
		int bottom = ReaderScale(hwnd, 41);
		MoveToEx(dc, g_readerSeparator1, top, NULL);
		LineTo(dc, g_readerSeparator1, bottom);
		MoveToEx(dc, g_readerSeparator2, top, NULL);
		LineTo(dc, g_readerSeparator2, bottom);
		MoveToEx(dc, 0, ReaderScale(hwnd, READER_TOOLBAR_HEIGHT) - 1, NULL);
		LineTo(dc, paint.rcPaint.right,
			ReaderScale(hwnd, READER_TOOLBAR_HEIGHT) - 1);
		SelectObject(dc, oldPen);
		DeleteObject(pen);
		EndPaint(hwnd, &paint);
		return 0;
	}

	case WM_CTLCOLORSTATIC:
		SetBkMode((HDC)wParam, TRANSPARENT);
		SetBkColor((HDC)wParam, g_readerTheme.headerBackground);
		SetTextColor((HDC)wParam,
			((HWND)lParam == g_readerStatus ||
			 (HWND)lParam == g_readerFontSizeLabel)
				? g_readerTheme.text : g_readerTheme.mutedText);
		return (LRESULT)g_readerToolbarBrush;

	case WM_CTLCOLORLISTBOX:
		SetBkColor((HDC)wParam, g_readerTheme.headerBackground);
		SetTextColor((HDC)wParam, g_readerTheme.text);
		return (LRESULT)g_readerToolbarBrush;

	case WM_DRAWITEM:
	{
		DRAWITEMSTRUCT* item = (DRAWITEMSTRUCT*)lParam;
		if (item != NULL && item->CtlID == IDC_READER_PIN) {
			DrawPinButton(item, IsWindowPinnedTopmost(hwnd));
			return TRUE;
		}
		if (item != NULL && (item->CtlID == IDC_READER_OPEN ||
			item->CtlID == IDC_READER_LIBRARY ||
			item->CtlID == IDC_READER_FONT_MINUS ||
			item->CtlID == IDC_READER_FONT_PLUS)) {
			DrawReaderToolbarButton(item);
			return TRUE;
		}
		break;
	}

	case WM_SETTINGCHANGE:
	case WM_THEMECHANGED:
	case WM_SYSCOLORCHANGE:
		ApplyReaderSystemTheme();
		UpdateReaderProgress();
		return 0;

	case WM_NOTIFY:
	{
		NMHDR* header = (NMHDR*)lParam;
		if (header != NULL && header->hwndFrom == g_readerEdit &&
			(header->code == EN_VSCROLL || header->code == EN_SELCHANGE)) {
			UpdateReaderProgress();
			return 0;
		}
		break;
	}

	case WM_WINDOWPOSCHANGED:
		g_readerTopmost = IsWindowPinnedTopmost(hwnd);
		if (g_readerPinButton != NULL) {
			InvalidateRect(g_readerPinButton, NULL, FALSE);
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_READER_OPEN:
		case IDM_READER_OPEN:
			ShowReaderOpenDialog();
			return 0;
		case IDC_READER_LIBRARY:
		case IDM_READER_LIBRARY:
			ShowLibraryWindow(hwnd);
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
		case IDC_READER_PIN:
			ToggleReaderTopmost();
			return 0;
		}
		if (LOWORD(wParam) >= IDM_READER_OPACITY_BASE + 10 &&
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
		if (g_readerOpenButton != NULL) {
			RemoveWindowSubclass(g_readerOpenButton, ReaderButtonSubclassProc, 11);
		}
		if (g_readerLibraryButton != NULL) {
			RemoveWindowSubclass(g_readerLibraryButton, ReaderButtonSubclassProc, 12);
		}
		if (g_readerFontMinusButton != NULL) {
			RemoveWindowSubclass(g_readerFontMinusButton, ReaderButtonSubclassProc, 13);
		}
		if (g_readerFontPlusButton != NULL) {
			RemoveWindowSubclass(g_readerFontPlusButton, ReaderButtonSubclassProc, 14);
		}
		if (g_readerPinButton != NULL) {
			RemoveWindowSubclass(g_readerPinButton, ReaderButtonSubclassProc, 15);
		}
		g_readerWindow = NULL;
		g_readerEdit = NULL;
		g_readerOpenButton = NULL;
		g_readerLibraryButton = NULL;
		g_readerFontMinusButton = NULL;
		g_readerFontPlusButton = NULL;
		g_readerFontSizeLabel = NULL;
		g_readerOpacityCombo = NULL;
		g_readerPinButton = NULL;
		g_readerStatus = NULL;
		g_readerWordInfo = NULL;
		g_readerProgressInfo = NULL;
		if (g_readerTextFont != NULL && g_readerTextFont != GetStockObject(DEFAULT_GUI_FONT)) {
			DeleteObject(g_readerTextFont);
		}
		g_readerTextFont = NULL;
		if (g_readerUiFont != NULL && g_readerUiFont != GetStockObject(DEFAULT_GUI_FONT)) {
			DeleteObject(g_readerUiFont);
		}
		g_readerUiFont = NULL;
		if (g_readerTitleFont != NULL &&
			g_readerTitleFont != GetStockObject(DEFAULT_GUI_FONT)) {
			DeleteObject(g_readerTitleFont);
		}
		g_readerTitleFont = NULL;
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
