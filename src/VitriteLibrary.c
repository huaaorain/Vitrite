#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <strsafe.h>
#include <stdlib.h>
#include <string.h>
#include "resource.h"
#include "VitriteLibrary.h"
#include "VitriteReader.h"
#include "VitriteUi.h"

#define LIBRARY_CLASS_NAME L"VitriteLibraryWindow"
#define BOOK_VIEW_CLASS_NAME L"VitriteBookCardView"
#define LIBRARY_TITLE L"Vitrite 书架目录"
#define LIBRARY_METADATA_NAME L"library.ini"
#define LIBRARY_SETTINGS_NAME L"reader.ini"
#define LIBRARY_MAX_BOOKS 2048
#define LIBRARY_TOOLBAR_HEIGHT 98

#define IDC_LIBRARY_IMPORT 800
#define IDC_LIBRARY_SET_PATH 801
#define IDC_LIBRARY_EXPORT 802
#define IDC_LIBRARY_SORT 803
#define IDC_LIBRARY_FAVORITE_FILTER 804
#define IDC_LIBRARY_AUTHOR_FILTER 805
#define IDC_LIBRARY_STATUS 806
#define IDC_LIBRARY_VIEW 807
#define IDC_LIBRARY_READER 808
#define IDC_LIBRARY_SORT_LABEL 809
#define IDC_LIBRARY_FILTER_LABEL 810
#define IDC_LIBRARY_AUTHOR_LABEL 811

#define IDM_LIBRARY_OPEN_BOOK 900
#define IDM_LIBRARY_EDIT_AUTHOR 901
#define IDM_LIBRARY_TOGGLE_FAVORITE 902
#define IDM_LIBRARY_REMOVE_BOOK 903

#define LIBRARY_BG_COLOR RGB(246, 243, 237)
#define LIBRARY_TOOLBAR_COLOR RGB(238, 234, 225)
#define LIBRARY_CARD_COLOR RGB(255, 253, 247)
#define LIBRARY_TEXT_COLOR RGB(42, 40, 37)
#define LIBRARY_SUBTLE_COLOR RGB(108, 101, 91)
#define LIBRARY_ACCENT_COLOR RGB(156, 126, 232)
#define LIBRARY_STAR_COLOR RGB(222, 159, 36)
#define LIBRARY_BORDER_COLOR RGB(222, 216, 204)

typedef struct LIBRARY_BOOK {
	WCHAR relativePath[MAX_PATH];
	WCHAR title[192];
	WCHAR author[128];
	BOOL favorite;
} LIBRARY_BOOK;

typedef struct AUTHOR_DIALOG_DATA {
	WCHAR title[192];
	WCHAR author[128];
} AUTHOR_DIALOG_DATA;

static HINSTANCE g_libraryInstance;
static HWND g_libraryWindow;
static HWND g_bookView;
static HWND g_libraryImportButton;
static HWND g_librarySetPathButton;
static HWND g_libraryExportButton;
static HWND g_libraryReaderButton;
static HWND g_librarySortCombo;
static HWND g_libraryFavoriteCombo;
static HWND g_libraryAuthorCombo;
static HWND g_libraryStatus;
static HWND g_librarySortLabel;
static HWND g_libraryFilterLabel;
static HWND g_libraryAuthorLabel;
static HFONT g_libraryTitleFont;
static HFONT g_libraryBodyFont;
static HFONT g_librarySmallFont;
static HFONT g_libraryUiFont;
static HBRUSH g_libraryBackgroundBrush;
static HBRUSH g_libraryToolbarBrush;
static HBRUSH g_libraryCardBrush;
static LIBRARY_BOOK* g_books;
static int g_bookCount;
static int g_visibleBooks[LIBRARY_MAX_BOOKS];
static int g_visibleBookCount;
static RECT g_cardRects[LIBRARY_MAX_BOOKS];
static RECT g_starRects[LIBRARY_MAX_BOOKS];
static int g_librarySortMode;
static int g_libraryFavoriteFilter;
static WCHAR g_libraryAuthorFilter[128];
static WCHAR g_libraryRoot[MAX_PATH];
static RECT g_librarySavedRect;
static int g_libraryScrollY;
static int g_libraryContentHeight;
static int g_libraryColumns;
static int g_libraryCardWidth;
static int g_libraryCardHeight;
static int g_libraryHoverBook;
static int g_libraryContextBook;
static BOOL g_libraryClassRegistered;
static BOOL g_bookViewClassRegistered;
#ifdef VITRITE_LIBRARY_TEST
static int g_libraryTestExportStage;
static BOOL g_libraryTestCompressCreateSuccess;
static DWORD g_libraryTestCompressExitCode;
static WCHAR g_libraryTestStagingPath[MAX_PATH];
static WCHAR g_libraryTestCommand[32768];
static WCHAR g_libraryTestPowerShellLog[MAX_PATH];
#endif

static void LoadLibraryBooks(void);
static void SaveLibraryBooks(void);
static void RefreshLibraryView(void);
static void PopulateAuthorFilter(void);
static void ImportDroppedFiles(HWND owner, HDROP drop);
static void ImportTxtFilesFromDialog(HWND owner);
static BOOL ImportSingleBook(HWND owner, const WCHAR* sourcePath, const WCHAR* author);
static void EditBookAuthor(HWND owner, int bookIndex);
static void ExportPortableLibrary(HWND owner);

static UINT LibraryDpi(HWND hwnd) {
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

static int LibraryScale(HWND hwnd, int value) {
	return MulDiv(value, (int)LibraryDpi(hwnd), 96);
}

static void GetLibrarySettingsPath(LPWSTR path, int pathCount) {
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
		StringCchCatW(path, (size_t)pathCount, LIBRARY_SETTINGS_NAME);
		return;
	}
	StringCchCatW(path, (size_t)pathCount, L"\\Vitrite");
	CreateDirectoryW(path, NULL);
	StringCchCatW(path, (size_t)pathCount, L"\\");
	StringCchCatW(path, (size_t)pathCount, LIBRARY_SETTINGS_NAME);
}

static void GetDefaultLibraryRoot(LPWSTR path, int pathCount) {
	WCHAR documents[MAX_PATH];
	documents[0] = L'\0';
	if (FAILED(SHGetFolderPathW(NULL, CSIDL_MYDOCUMENTS | CSIDL_FLAG_CREATE,
			NULL, SHGFP_TYPE_CURRENT, documents))) {
		GetEnvironmentVariableW(L"USERPROFILE", documents, MAX_PATH);
	}
	StringCchPrintfW(path, (size_t)pathCount, L"%s\\Vitrite Library", documents);
}

static BOOL GetPortableLibraryRoot(LPWSTR path, int pathCount) {
	WCHAR executable[MAX_PATH];
	WCHAR candidate[MAX_PATH];
	WCHAR metadata[MAX_PATH];
	LPWSTR slash;

	if (path == NULL || pathCount < 2) {
		return FALSE;
	}
	if (!GetModuleFileNameW(NULL, executable, MAX_PATH)) {
		return FALSE;
	}
	slash = wcsrchr(executable, L'\\');
	if (slash == NULL) {
		return FALSE;
	}
	slash[1] = L'\0';
	StringCchPrintfW(candidate, MAX_PATH, L"%slibrary", executable);
	StringCchPrintfW(metadata, MAX_PATH, L"%s\\" LIBRARY_METADATA_NAME, candidate);
	if (GetFileAttributesW(metadata) == INVALID_FILE_ATTRIBUTES) {
		return FALSE;
	}
	StringCchCopyW(path, (size_t)pathCount, candidate);
	return TRUE;
}

static void EnsureDirectory(const WCHAR* path) {
	if (path != NULL && path[0] != L'\0') {
		CreateDirectoryW(path, NULL);
	}
}

static void NormalizeLibraryRoot(LPWSTR path) {
	size_t length;

	if (path == NULL) {
		return;
	}
	length = wcslen(path);
	while (length > 3 && (path[length - 1] == L'\\' || path[length - 1] == L'/')) {
		path[--length] = L'\0';
	}
}

static void LoadLibraryRoot(void) {
	WCHAR settings[MAX_PATH];
	WCHAR value[MAX_PATH];

	GetLibrarySettingsPath(settings, MAX_PATH);
	g_librarySortMode = GetPrivateProfileIntW(L"Library", L"SortMode", 0, settings);
	g_libraryFavoriteFilter = GetPrivateProfileIntW(L"Library",
		L"FavoriteFilter", 0, settings);
	if (g_librarySortMode < 0 || g_librarySortMode > 1) g_librarySortMode = 0;
	if (g_libraryFavoriteFilter < 0 || g_libraryFavoriteFilter > 2) {
		g_libraryFavoriteFilter = 0;
	}
	GetPrivateProfileStringW(L"Library", L"AuthorFilter", L"",
		g_libraryAuthorFilter, 128, settings);
	if (GetPortableLibraryRoot(g_libraryRoot, MAX_PATH)) {
		return;
	}
	GetPrivateProfileStringW(L"Library", L"Path", L"", value, MAX_PATH, settings);
	if (value[0] == L'\0') {
		GetDefaultLibraryRoot(g_libraryRoot, MAX_PATH);
	} else {
		StringCchCopyW(g_libraryRoot, MAX_PATH, value);
	}
	NormalizeLibraryRoot(g_libraryRoot);
	EnsureDirectory(g_libraryRoot);
}

static void SaveLibraryRoot(void) {
	WCHAR settings[MAX_PATH];
	WCHAR value[32];
	GetLibrarySettingsPath(settings, MAX_PATH);
	WritePrivateProfileStringW(L"Library", L"Path", g_libraryRoot, settings);
	StringCchPrintfW(value, 32, L"%d", g_librarySortMode);
	WritePrivateProfileStringW(L"Library", L"SortMode", value, settings);
	StringCchPrintfW(value, 32, L"%d", g_libraryFavoriteFilter);
	WritePrivateProfileStringW(L"Library", L"FavoriteFilter", value, settings);
	WritePrivateProfileStringW(L"Library", L"AuthorFilter",
		g_libraryAuthorFilter, settings);
}

static void LoadLibraryWindowRect(void) {
	WCHAR settings[MAX_PATH];
	GetLibrarySettingsPath(settings, MAX_PATH);
	g_librarySavedRect.left = GetPrivateProfileIntW(L"LibraryWindow", L"Left", 0, settings);
	g_librarySavedRect.top = GetPrivateProfileIntW(L"LibraryWindow", L"Top", 0, settings);
	g_librarySavedRect.right = GetPrivateProfileIntW(L"LibraryWindow", L"Width", 0, settings);
	g_librarySavedRect.bottom = GetPrivateProfileIntW(L"LibraryWindow", L"Height", 0, settings);
	if (g_librarySavedRect.right < 720 || g_librarySavedRect.bottom < 480) {
		memset(&g_librarySavedRect, 0, sizeof(g_librarySavedRect));
	}
}

static void SaveLibraryWindowRect(void) {
	WCHAR settings[MAX_PATH];
	WCHAR value[32];
	RECT rect;

	if (g_libraryWindow == NULL || !IsWindow(g_libraryWindow)) {
		return;
	}
	GetWindowRect(g_libraryWindow, &rect);
	GetLibrarySettingsPath(settings, MAX_PATH);
	StringCchPrintfW(value, 32, L"%d", rect.left);
	WritePrivateProfileStringW(L"LibraryWindow", L"Left", value, settings);
	StringCchPrintfW(value, 32, L"%d", rect.top);
	WritePrivateProfileStringW(L"LibraryWindow", L"Top", value, settings);
	StringCchPrintfW(value, 32, L"%d", rect.right - rect.left);
	WritePrivateProfileStringW(L"LibraryWindow", L"Width", value, settings);
	StringCchPrintfW(value, 32, L"%d", rect.bottom - rect.top);
	WritePrivateProfileStringW(L"LibraryWindow", L"Height", value, settings);
}

static void GetLibraryMetadataPath(LPWSTR path, int pathCount) {
	StringCchPrintfW(path, (size_t)pathCount, L"%s\\" LIBRARY_METADATA_NAME, g_libraryRoot);
}

static void TitleFromFileName(const WCHAR* fileName, LPWSTR title, int titleCount) {
	LPWSTR extension;
	StringCchCopyW(title, (size_t)titleCount, fileName);
	extension = wcsrchr(title, L'.');
	if (extension != NULL && _wcsicmp(extension, L".txt") == 0) {
		*extension = L'\0';
	}
}

static BOOL IsRelativeLibraryPathSafe(const WCHAR* relativePath) {
	if (relativePath == NULL || relativePath[0] == L'\0' ||
		relativePath[0] == L'\\' || relativePath[0] == L'/' ||
		(relativePath[0] != L'\0' && relativePath[1] == L':')) {
		return FALSE;
	}
	return wcsstr(relativePath, L"..\\") == NULL &&
		wcsstr(relativePath, L"../") == NULL;
}

static BOOL IsTxtFile(const WCHAR* path) {
	const WCHAR* extension = wcsrchr(path, L'.');
	return extension != NULL && _wcsicmp(extension, L".txt") == 0;
}

static BOOL BookDiskPath(const LIBRARY_BOOK* book, LPWSTR path, int pathCount) {
	if (book == NULL) {
		return FALSE;
	}
	StringCchPrintfW(path, (size_t)pathCount, L"%s\\%s", g_libraryRoot,
		book->relativePath);
	return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void LoadLibraryBooks(void) {
	WCHAR metadata[MAX_PATH];
	WCHAR section[32];
	WCHAR key[32];
	int count;
	int i;

	if (g_books == NULL) {
		return;
	}
	g_bookCount = 0;
	GetLibraryMetadataPath(metadata, MAX_PATH);
	count = GetPrivateProfileIntW(L"Library", L"Count", 0, metadata);
	if (count < 0) {
		count = 0;
	} else if (count > LIBRARY_MAX_BOOKS) {
		count = LIBRARY_MAX_BOOKS;
	}
	for (i = 0; i < count; i++) {
		LIBRARY_BOOK book;
		WCHAR fullPath[MAX_PATH];
		memset(&book, 0, sizeof(book));
		StringCchPrintfW(section, 32, L"Book%04d", i + 1);
		GetPrivateProfileStringW(section, L"Path", L"", book.relativePath,
			MAX_PATH, metadata);
		GetPrivateProfileStringW(section, L"Title", L"", book.title,
			192, metadata);
		GetPrivateProfileStringW(section, L"Author", L"", book.author,
			128, metadata);
		StringCchPrintfW(key, 32, L"Favorite");
		book.favorite = GetPrivateProfileIntW(section, key, 0, metadata) != 0;
		if (!IsRelativeLibraryPathSafe(book.relativePath) ||
			!BookDiskPath(&book, fullPath, MAX_PATH)) {
			continue;
		}
		if (book.title[0] == L'\0') {
			const WCHAR* fileName = wcsrchr(book.relativePath, L'\\');
			if (fileName == NULL) {
				fileName = wcsrchr(book.relativePath, L'/');
			}
			fileName = fileName != NULL ? fileName + 1 : book.relativePath;
			TitleFromFileName(fileName, book.title, 192);
		}
		g_books[g_bookCount++] = book;
	}
}

static void SaveLibraryBooks(void) {
	WCHAR metadata[MAX_PATH];
	WCHAR section[32];
	WCHAR value[32];
	int i;

	if (g_books == NULL) {
		return;
	}
	EnsureDirectory(g_libraryRoot);
	GetLibraryMetadataPath(metadata, MAX_PATH);
	StringCchPrintfW(value, 32, L"%d", g_bookCount);
	WritePrivateProfileStringW(L"Library", L"Version", L"1", metadata);
	WritePrivateProfileStringW(L"Library", L"Count", value, metadata);
	for (i = 0; i < g_bookCount; i++) {
		StringCchPrintfW(section, 32, L"Book%04d", i + 1);
		WritePrivateProfileStringW(section, L"Path", g_books[i].relativePath, metadata);
		WritePrivateProfileStringW(section, L"Title", g_books[i].title, metadata);
		WritePrivateProfileStringW(section, L"Author", g_books[i].author, metadata);
		WritePrivateProfileStringW(section, L"Favorite",
			g_books[i].favorite ? L"1" : L"0", metadata);
	}
}

static int CompareTitles(const WCHAR* left, const WCHAR* right) {
	int result = CompareStringEx(L"zh-CN", LINGUISTIC_IGNORECASE,
		left, -1, right, -1, NULL, NULL, 0);
	if (result == 0) {
		result = lstrcmpiW(left, right);
	}
	return result - CSTR_EQUAL;
}

static int CompareBookIndices(const void* left, const void* right) {
	int leftIndex = *(const int*)left;
	int rightIndex = *(const int*)right;
	const LIBRARY_BOOK* leftBook = &g_books[leftIndex];
	const LIBRARY_BOOK* rightBook = &g_books[rightIndex];
	int result;

	if (g_librarySortMode == 1) {
		const WCHAR* leftAuthor = leftBook->author[0] ? leftBook->author : L"未设置作者";
		const WCHAR* rightAuthor = rightBook->author[0] ? rightBook->author : L"未设置作者";
		result = CompareTitles(leftAuthor, rightAuthor);
		if (result != 0) {
			return result;
		}
	}
	result = CompareTitles(leftBook->title, rightBook->title);
	if (result != 0) {
		return result;
	}
	return lstrcmpiW(leftBook->relativePath, rightBook->relativePath);
}

static void RebuildVisibleBooks(void) {
	int i;
	if (g_books == NULL) {
		g_visibleBookCount = 0;
		return;
	}
	g_visibleBookCount = 0;
	for (i = 0; i < g_bookCount; i++) {
		if (g_libraryFavoriteFilter == 1 && !g_books[i].favorite) {
			continue;
		}
		if (g_libraryFavoriteFilter == 2 && g_books[i].favorite) {
			continue;
		}
		if (g_libraryAuthorFilter[0] != L'\0' &&
			lstrcmpiW(g_libraryAuthorFilter,
				g_books[i].author[0] ? g_books[i].author : L"未设置作者") != 0) {
			continue;
		}
		g_visibleBooks[g_visibleBookCount++] = i;
	}
	qsort(g_visibleBooks, (size_t)g_visibleBookCount, sizeof(g_visibleBooks[0]),
		CompareBookIndices);
}

static void UpdateLibraryStatus(void) {
	WCHAR text[256];
	if (g_libraryStatus == NULL) {
		return;
	}
	StringCchPrintfW(text, 256, L"%d 本显示 / 共 %d 本  ·  书库：%s",
		g_visibleBookCount, g_bookCount, g_libraryRoot);
	SetWindowTextW(g_libraryStatus, text);
}

static void RefreshLibraryView(void) {
	g_libraryScrollY = 0;
	RebuildVisibleBooks();
	if (g_bookView != NULL) {
		SendMessageW(g_bookView, WM_USER + 1, 0, 0);
	}
	UpdateLibraryStatus();
}

static void GetComboText(HWND combo, LPWSTR text, int count) {
	int index = (int)SendMessageW(combo, CB_GETCURSEL, 0, 0);
	text[0] = L'\0';
	if (index >= 0) {
		SendMessageW(combo, CB_GETLBTEXT, index, (LPARAM)text);
	}
}

static BOOL AuthorExistsInCombo(const WCHAR* author) {
	int count = (int)SendMessageW(g_libraryAuthorCombo, CB_GETCOUNT, 0, 0);
	WCHAR text[128];
	int i;
	for (i = 1; i < count; i++) {
		SendMessageW(g_libraryAuthorCombo, CB_GETLBTEXT, i, (LPARAM)text);
		if (lstrcmpiW(text, author) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

static void PopulateAuthorFilter(void) {
	int count;
	int i;
	int j;
	WCHAR text[128];
	BOOL added;
	WCHAR previous[128];

	if (g_libraryAuthorCombo == NULL) {
		return;
	}
	StringCchCopyW(previous, 128, g_libraryAuthorFilter);
	SendMessageW(g_libraryAuthorCombo, CB_RESETCONTENT, 0, 0);
	SendMessageW(g_libraryAuthorCombo, CB_ADDSTRING, 0, (LPARAM)L"全部作者");
	for (i = 0; i < g_bookCount; i++) {
		const WCHAR* author = g_books[i].author[0] ? g_books[i].author : L"未设置作者";
		added = FALSE;
		count = (int)SendMessageW(g_libraryAuthorCombo, CB_GETCOUNT, 0, 0);
		for (j = 1; j < count; j++) {
			SendMessageW(g_libraryAuthorCombo, CB_GETLBTEXT, j, (LPARAM)text);
			if (lstrcmpiW(text, author) == 0) {
				added = TRUE;
				break;
			}
		}
		if (!added) {
			SendMessageW(g_libraryAuthorCombo, CB_ADDSTRING, 0, (LPARAM)author);
		}
	}
	if (previous[0] != L'\0' && AuthorExistsInCombo(previous)) {
		int index = (int)SendMessageW(g_libraryAuthorCombo, CB_FINDSTRINGEXACT,
			(WPARAM)-1, (LPARAM)previous);
		SendMessageW(g_libraryAuthorCombo, CB_SETCURSEL, index, 0);
	} else {
		g_libraryAuthorFilter[0] = L'\0';
		SendMessageW(g_libraryAuthorCombo, CB_SETCURSEL, 0, 0);
	}
}

static int HitTestBook(POINT point) {
	int i;
	point.y += g_libraryScrollY;
	for (i = 0; i < g_visibleBookCount; i++) {
		if (PtInRect(&g_cardRects[i], point)) {
			return i;
		}
	}
	return -1;
}

static BOOL HitTestStar(POINT point, int visibleIndex) {
	point.y += g_libraryScrollY;
	return visibleIndex >= 0 &&
		PtInRect(&g_starRects[visibleIndex], point);
}

static void DrawFavoriteStar(HDC dc, const RECT* rect, BOOL favorite) {
	static const int outerUnit[10][2] = {
		{0, -100}, {59, -81}, {95, -31}, {95, 31}, {59, 81},
		{0, 100}, {-59, 81}, {-95, 31}, {-95, -31}, {-59, -81}
	};
	POINT points[10];
	HBRUSH brush;
	HPEN pen;
	int centerX = (rect->left + rect->right) / 2;
	int centerY = (rect->top + rect->bottom) / 2;
	int radius = min(rect->right - rect->left, rect->bottom - rect->top) / 2;
	int i;

	for (i = 0; i < 10; i++) {
		int scale = (i % 2 == 0) ? radius : (radius * 43 / 100);
		points[i].x = centerX + outerUnit[i][0] * scale / 100;
		points[i].y = centerY + outerUnit[i][1] * scale / 100;
	}
	brush = CreateSolidBrush(favorite ? LIBRARY_STAR_COLOR :
		RGB(255, 253, 247));
	pen = CreatePen(PS_SOLID, 2, favorite ? LIBRARY_STAR_COLOR :
		RGB(128, 119, 104));
	if (brush != NULL && pen != NULL) {
		SelectObject(dc, brush);
		SelectObject(dc, pen);
		Polygon(dc, points, 10);
	}
	if (brush != NULL) {
		DeleteObject(brush);
	}
	if (pen != NULL) {
		DeleteObject(pen);
	}
}

static void RecalculateBookLayout(int clientWidth) {
	int padding;
	int gap;
	int available;
	int rows;

	padding = LibraryScale(g_bookView, 20);
	gap = LibraryScale(g_bookView, 16);
	g_libraryCardHeight = LibraryScale(g_bookView, 148);
	available = clientWidth - padding * 2;
	g_libraryColumns = (available + gap) / (LibraryScale(g_bookView, 220) + gap);
	if (g_libraryColumns < 1) {
		g_libraryColumns = 1;
	}
	g_libraryCardWidth = (available - gap * (g_libraryColumns - 1)) /
		g_libraryColumns;
	if (g_libraryCardWidth < LibraryScale(g_bookView, 180)) {
		g_libraryCardWidth = LibraryScale(g_bookView, 180);
	}
	rows = g_visibleBookCount > 0
		? (g_visibleBookCount + g_libraryColumns - 1) / g_libraryColumns
		: 0;
	g_libraryContentHeight = padding * 2 +
		rows * g_libraryCardHeight + (rows > 0 ? (rows - 1) * gap : 0);
}

static void UpdateBookScrollBar(void) {
	RECT client;
	SCROLLINFO info;
	int page;
	int maximum;
	int oldScroll;

	if (g_bookView == NULL) {
		return;
	}
	GetClientRect(g_bookView, &client);
	page = client.bottom - client.top;
	maximum = max(0, g_libraryContentHeight - 1);
	oldScroll = g_libraryScrollY;
	g_libraryScrollY = max(0, min(g_libraryScrollY,
		max(0, g_libraryContentHeight - page)));
	memset(&info, 0, sizeof(info));
	info.cbSize = sizeof(info);
	info.fMask = SIF_RANGE | SIF_PAGE | SIF_POS;
	info.nMin = 0;
	info.nMax = maximum;
	info.nPage = (UINT)max(1, page);
	info.nPos = g_libraryScrollY;
	SetScrollInfo(g_bookView, SB_VERT, &info, TRUE);
	if (oldScroll != g_libraryScrollY) {
		InvalidateRect(g_bookView, NULL, TRUE);
	}
}

static void ScrollBookView(int delta) {
	RECT client;
	GetClientRect(g_bookView, &client);
	g_libraryScrollY = max(0, min(g_libraryScrollY + delta,
		max(0, g_libraryContentHeight - (client.bottom - client.top))));
	UpdateBookScrollBar();
	InvalidateRect(g_bookView, NULL, FALSE);
}

static void PaintBookView(HWND hwnd) {
	PAINTSTRUCT paint;
	HDC dc;
	HDC bufferDc;
	HBITMAP bufferBitmap;
	HBITMAP oldBitmap;
	RECT client;
	RECT card;
	RECT titleRect;
	RECT authorRect;
	RECT starRect;
	HBRUSH background;
	HBRUSH cardBrush;
	HPEN borderPen;
	HPEN oldPen;
	HBRUSH oldBrush;
	WCHAR authorText[192];
	int padding;
	int gap;
	int row;
	int column;
	int i;
	BOOL favorite;
	COLORREF borderColor;

	GetClientRect(hwnd, &client);
	dc = BeginPaint(hwnd, &paint);
	bufferDc = CreateCompatibleDC(dc);
	bufferBitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
	oldBitmap = (HBITMAP)SelectObject(bufferDc, bufferBitmap);
	background = CreateSolidBrush(LIBRARY_BG_COLOR);
	FillRect(bufferDc, &client, background);
	DeleteObject(background);

	RecalculateBookLayout(client.right);
	padding = LibraryScale(hwnd, 20);
	gap = LibraryScale(hwnd, 16);
	SetBkMode(bufferDc, TRANSPARENT);
	SetTextColor(bufferDc, LIBRARY_TEXT_COLOR);

	for (i = 0; i < g_visibleBookCount; i++) {
		int bookIndex = g_visibleBooks[i];
		row = i / g_libraryColumns;
		column = i % g_libraryColumns;
		card.left = padding + column * (g_libraryCardWidth + gap);
		card.top = padding + row * (g_libraryCardHeight + gap) - g_libraryScrollY;
		card.right = card.left + g_libraryCardWidth;
		card.bottom = card.top + g_libraryCardHeight;
		g_cardRects[i] = card;
		g_cardRects[i].top += g_libraryScrollY;
		g_cardRects[i].bottom += g_libraryScrollY;
		if (card.bottom < 0 || card.top > client.bottom) {
			continue;
		}
		favorite = g_books[bookIndex].favorite;
		cardBrush = CreateSolidBrush(i == g_libraryHoverBook
			? RGB(255, 250, 236) : LIBRARY_CARD_COLOR);
		borderColor = favorite ? LIBRARY_STAR_COLOR : LIBRARY_BORDER_COLOR;
		borderPen = CreatePen(PS_SOLID, favorite ? 2 : 1, borderColor);
		oldBrush = (HBRUSH)SelectObject(bufferDc, cardBrush);
		oldPen = (HPEN)SelectObject(bufferDc, borderPen);
		RoundRect(bufferDc, card.left, card.top, card.right, card.bottom,
			LibraryScale(hwnd, 14), LibraryScale(hwnd, 14));
		SelectObject(bufferDc, oldPen);
		SelectObject(bufferDc, oldBrush);
		DeleteObject(borderPen);
		DeleteObject(cardBrush);

		starRect.left = card.right - LibraryScale(hwnd, 48);
		starRect.top = card.top + LibraryScale(hwnd, 12);
		starRect.right = starRect.left + LibraryScale(hwnd, 30);
		starRect.bottom = starRect.top + LibraryScale(hwnd, 30);
		g_starRects[i] = starRect;
		g_starRects[i].top += g_libraryScrollY;
		g_starRects[i].bottom += g_libraryScrollY;
		DrawFavoriteStar(bufferDc, &starRect, favorite);

		titleRect.left = card.left + LibraryScale(hwnd, 18);
		titleRect.top = card.top + LibraryScale(hwnd, 18);
		titleRect.right = card.right - LibraryScale(hwnd, 56);
		titleRect.bottom = card.top + LibraryScale(hwnd, 83);
		SelectObject(bufferDc, g_libraryTitleFont);
		SetTextColor(bufferDc, LIBRARY_TEXT_COLOR);
		DrawTextW(bufferDc, g_books[bookIndex].title, -1, &titleRect,
			DT_LEFT | DT_TOP | DT_WORDBREAK | DT_END_ELLIPSIS | DT_NOPREFIX);

		StringCchPrintfW(authorText, 192, L"作者：%s",
			g_books[bookIndex].author[0] ? g_books[bookIndex].author : L"未设置作者");
		authorRect.left = card.left + LibraryScale(hwnd, 18);
		authorRect.top = card.bottom - LibraryScale(hwnd, 42);
		authorRect.right = card.right - LibraryScale(hwnd, 18);
		authorRect.bottom = card.bottom - LibraryScale(hwnd, 14);
		SelectObject(bufferDc, g_libraryBodyFont);
		SetTextColor(bufferDc, LIBRARY_SUBTLE_COLOR);
		DrawTextW(bufferDc, authorText, -1, &authorRect,
			DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
	}

	if (g_visibleBookCount == 0) {
		RECT emptyRect = client;
		emptyRect.top += LibraryScale(hwnd, 80);
		SelectObject(bufferDc, g_libraryTitleFont);
		SetTextColor(bufferDc, LIBRARY_SUBTLE_COLOR);
		DrawTextW(bufferDc, L"书架还是空的\n把 TXT 文件拖到这里，或点击“导入 TXT”",
			-1, &emptyRect, DT_CENTER | DT_TOP | DT_WORDBREAK | DT_NOPREFIX);
	}

	BitBlt(dc, 0, 0, client.right, client.bottom, bufferDc, 0, 0, SRCCOPY);
	SelectObject(bufferDc, oldBitmap);
	DeleteObject(bufferBitmap);
	DeleteDC(bufferDc);
	EndPaint(hwnd, &paint);
}

static void OpenBookByIndex(int visibleIndex) {
	WCHAR path[MAX_PATH];
	if (visibleIndex < 0 || visibleIndex >= g_visibleBookCount) {
		return;
	}
	if (!BookDiskPath(&g_books[g_visibleBooks[visibleIndex]], path, MAX_PATH)) {
		MessageBoxW(g_libraryWindow, L"这本书的文件已经不在书库目录中。",
			LIBRARY_TITLE, MB_OK | MB_ICONWARNING);
		return;
	}
	OpenTextReaderWindow(g_libraryWindow, path);
	if (g_libraryWindow != NULL) {
		ShowWindow(g_libraryWindow, SW_HIDE);
	}
}

static void ToggleFavoriteByVisibleIndex(int visibleIndex) {
	if (visibleIndex >= 0 && visibleIndex < g_visibleBookCount) {
		int bookIndex = g_visibleBooks[visibleIndex];
		g_books[bookIndex].favorite = !g_books[bookIndex].favorite;
		SaveLibraryBooks();
		RefreshLibraryView();
	}
}

static void EditBookAuthorByVisibleIndex(int visibleIndex) {
	if (visibleIndex >= 0 && visibleIndex < g_visibleBookCount) {
		EditBookAuthor(g_libraryWindow, g_visibleBooks[visibleIndex]);
	}
}

static void RemoveBookByVisibleIndex(int visibleIndex) {
	WCHAR message[256];
	int bookIndex;
	if (visibleIndex < 0 || visibleIndex >= g_visibleBookCount) {
		return;
	}
	bookIndex = g_visibleBooks[visibleIndex];
	StringCchPrintfW(message, 256,
		L"只从书架移除《%s》吗？\r\nTXT 文件不会被删除。",
		g_books[bookIndex].title);
	if (MessageBoxW(g_libraryWindow, message, LIBRARY_TITLE,
			MB_YESNO | MB_ICONQUESTION) != IDYES) {
		return;
	}
	if (bookIndex < g_bookCount - 1) {
		memmove(&g_books[bookIndex], &g_books[bookIndex + 1],
			(size_t)(g_bookCount - bookIndex - 1) * sizeof(g_books[0]));
	}
	g_bookCount--;
	SaveLibraryBooks();
	PopulateAuthorFilter();
	RefreshLibraryView();
}

static void ShowBookContextMenu(int visibleIndex, POINT point) {
	HMENU menu;
	int command;
	if (visibleIndex < 0 || visibleIndex >= g_visibleBookCount) {
		return;
	}
	g_libraryContextBook = visibleIndex;
	menu = CreatePopupMenu();
	if (menu == NULL) {
		return;
	}
	AppendMenuW(menu, MF_STRING, IDM_LIBRARY_OPEN_BOOK, L"打开");
	AppendMenuW(menu, MF_STRING, IDM_LIBRARY_EDIT_AUTHOR, L"编辑作者...");
	AppendMenuW(menu, MF_STRING, IDM_LIBRARY_TOGGLE_FAVORITE,
		g_books[g_visibleBooks[visibleIndex]].favorite ? L"取消特别喜欢" : L"设为特别喜欢");
	AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
	AppendMenuW(menu, MF_STRING, IDM_LIBRARY_REMOVE_BOOK, L"从书架移除");
	SetForegroundWindow(g_libraryWindow);
	command = (int)TrackPopupMenuEx(menu,
		TPM_RETURNCMD | TPM_RIGHTBUTTON, point.x, point.y, g_bookView, NULL);
	DestroyMenu(menu);
	switch (command) {
	case IDM_LIBRARY_OPEN_BOOK:
		OpenBookByIndex(visibleIndex);
		break;
	case IDM_LIBRARY_EDIT_AUTHOR:
		EditBookAuthorByVisibleIndex(visibleIndex);
		break;
	case IDM_LIBRARY_TOGGLE_FAVORITE:
		ToggleFavoriteByVisibleIndex(visibleIndex);
		break;
	case IDM_LIBRARY_REMOVE_BOOK:
		RemoveBookByVisibleIndex(visibleIndex);
		break;
	}
}

static LRESULT CALLBACK BookViewWndProc(HWND hwnd, UINT message, WPARAM wParam,
	LPARAM lParam) {
	switch (message) {
	case WM_CREATE:
		g_bookView = hwnd;
		return 0;
	case WM_SIZE:
		RecalculateBookLayout(LOWORD(lParam));
		UpdateBookScrollBar();
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;
	case WM_USER + 1:
	{
		RECT client;
		GetClientRect(hwnd, &client);
		RecalculateBookLayout(client.right);
		UpdateBookScrollBar();
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;
	}
	case WM_ERASEBKGND:
		return 1;
	case WM_PAINT:
		PaintBookView(hwnd);
		return 0;
	case WM_VSCROLL:
	{
		SCROLLINFO info;
		int position;
		memset(&info, 0, sizeof(info));
		info.cbSize = sizeof(info);
		info.fMask = SIF_ALL;
		GetScrollInfo(hwnd, SB_VERT, &info);
		position = info.nPos;
		switch (LOWORD(wParam)) {
		case SB_LINEUP:
			position -= LibraryScale(hwnd, 36);
			break;
		case SB_LINEDOWN:
			position += LibraryScale(hwnd, 36);
			break;
		case SB_PAGEUP:
			position -= (int)info.nPage;
			break;
		case SB_PAGEDOWN:
			position += (int)info.nPage;
			break;
		case SB_THUMBTRACK:
		case SB_THUMBPOSITION:
			position = info.nTrackPos;
			break;
		}
		ScrollBookView(position - g_libraryScrollY);
		return 0;
	}
	case WM_MOUSEWHEEL:
		ScrollBookView(-GET_WHEEL_DELTA_WPARAM(wParam) *
			LibraryScale(hwnd, 36) / WHEEL_DELTA);
		return 0;
	case WM_MOUSEMOVE:
	{
		POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
		int hover = HitTestBook(point);
		TRACKMOUSEEVENT tracking;
		if (hover != g_libraryHoverBook) {
			g_libraryHoverBook = hover;
			InvalidateRect(hwnd, NULL, FALSE);
		}
		memset(&tracking, 0, sizeof(tracking));
		tracking.cbSize = sizeof(tracking);
		tracking.dwFlags = TME_LEAVE;
		tracking.hwndTrack = hwnd;
		TrackMouseEvent(&tracking);
		return 0;
	}
	case WM_MOUSELEAVE:
		if (g_libraryHoverBook != -1) {
			g_libraryHoverBook = -1;
			InvalidateRect(hwnd, NULL, FALSE);
		}
		return 0;
	case WM_LBUTTONUP:
	{
		POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
		int visibleIndex = HitTestBook(point);
		if (visibleIndex >= 0) {
			if (HitTestStar(point, visibleIndex)) {
				ToggleFavoriteByVisibleIndex(visibleIndex);
			} else {
				OpenBookByIndex(visibleIndex);
			}
		}
		return 0;
	}
	case WM_RBUTTONUP:
	{
		POINT point = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
		POINT screenPoint = point;
		int visibleIndex = HitTestBook(point);
		ClientToScreen(hwnd, &screenPoint);
		ShowBookContextMenu(visibleIndex, screenPoint);
		return 0;
	}
	case WM_DESTROY:
		g_bookView = NULL;
		return 0;
	}
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

static void RecreateLibraryFonts(HWND hwnd) {
	LOGFONTW font;
	UINT dpi = LibraryDpi(hwnd);
	if (g_libraryTitleFont != NULL) DeleteObject(g_libraryTitleFont);
	if (g_libraryBodyFont != NULL) DeleteObject(g_libraryBodyFont);
	if (g_librarySmallFont != NULL) DeleteObject(g_librarySmallFont);
	if (g_libraryUiFont != NULL) DeleteObject(g_libraryUiFont);

	memset(&font, 0, sizeof(font));
	font.lfCharSet = DEFAULT_CHARSET;
	font.lfQuality = CLEARTYPE_QUALITY;
	StringCchCopyW(font.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
	font.lfHeight = -MulDiv(14, (int)dpi, 72);
	font.lfWeight = FW_SEMIBOLD;
	g_libraryTitleFont = CreateFontIndirectW(&font);
	font.lfHeight = -MulDiv(10, (int)dpi, 72);
	font.lfWeight = FW_NORMAL;
	g_libraryBodyFont = CreateFontIndirectW(&font);
	font.lfHeight = -MulDiv(9, (int)dpi, 72);
	g_librarySmallFont = CreateFontIndirectW(&font);
	font.lfHeight = -MulDiv(10, (int)dpi, 72);
	g_libraryUiFont = CreateFontIndirectW(&font);
}

static void ApplyLibraryFonts(void) {
	int controls[] = {
		IDC_LIBRARY_IMPORT, IDC_LIBRARY_SET_PATH, IDC_LIBRARY_EXPORT,
		IDC_LIBRARY_SORT, IDC_LIBRARY_FAVORITE_FILTER, IDC_LIBRARY_AUTHOR_FILTER,
		IDC_LIBRARY_STATUS, IDC_LIBRARY_READER, IDC_LIBRARY_SORT_LABEL,
		IDC_LIBRARY_FILTER_LABEL, IDC_LIBRARY_AUTHOR_LABEL
	};
	int i;
	for (i = 0; i < (int)(sizeof(controls) / sizeof(controls[0])); i++) {
		HWND control = GetDlgItem(g_libraryWindow, controls[i]);
		if (control != NULL) {
			SendMessageW(control, WM_SETFONT, (WPARAM)g_libraryUiFont, TRUE);
		}
	}
}

static void LayoutLibraryWindow(void) {
	RECT client;
	int width;
	int padding;
	int gap;
	int top;
	int rowHeight;
	int x;

	if (g_libraryWindow == NULL || !GetClientRect(g_libraryWindow, &client)) {
		return;
	}
	width = client.right;
	padding = LibraryScale(g_libraryWindow, 14);
	gap = LibraryScale(g_libraryWindow, 8);
	rowHeight = LibraryScale(g_libraryWindow, 32);
	top = LibraryScale(g_libraryWindow, 10);
	x = padding;

	MoveWindow(g_libraryImportButton, x, top, LibraryScale(g_libraryWindow, 108),
		rowHeight, TRUE);
	x += LibraryScale(g_libraryWindow, 108) + gap;
	MoveWindow(g_librarySetPathButton, x, top, LibraryScale(g_libraryWindow, 126),
		rowHeight, TRUE);
	x += LibraryScale(g_libraryWindow, 126) + gap;
	MoveWindow(g_libraryExportButton, x, top, LibraryScale(g_libraryWindow, 142),
		rowHeight, TRUE);
	x += LibraryScale(g_libraryWindow, 142) + gap;
	MoveWindow(g_libraryReaderButton, x, top, LibraryScale(g_libraryWindow, 88),
		rowHeight, TRUE);

	top += rowHeight + LibraryScale(g_libraryWindow, 9);
	x = padding;
	MoveWindow(g_librarySortLabel, x, top + LibraryScale(g_libraryWindow, 8),
		LibraryScale(g_libraryWindow, 38), LibraryScale(g_libraryWindow, 18), TRUE);
	x += LibraryScale(g_libraryWindow, 38);
	MoveWindow(g_librarySortCombo, x, top, LibraryScale(g_libraryWindow, 162),
		LibraryScale(g_libraryWindow, 220), TRUE);
	x += LibraryScale(g_libraryWindow, 162) + gap * 2;
	MoveWindow(g_libraryFilterLabel, x, top + LibraryScale(g_libraryWindow, 8),
		LibraryScale(g_libraryWindow, 38), LibraryScale(g_libraryWindow, 18), TRUE);
	x += LibraryScale(g_libraryWindow, 38);
	MoveWindow(g_libraryFavoriteCombo, x, top, LibraryScale(g_libraryWindow, 150),
		LibraryScale(g_libraryWindow, 220), TRUE);
	x += LibraryScale(g_libraryWindow, 150) + gap * 2;
	MoveWindow(g_libraryAuthorLabel, x, top + LibraryScale(g_libraryWindow, 8),
		LibraryScale(g_libraryWindow, 42), LibraryScale(g_libraryWindow, 18), TRUE);
	x += LibraryScale(g_libraryWindow, 42);
	MoveWindow(g_libraryAuthorCombo, x, top, LibraryScale(g_libraryWindow, 176),
		LibraryScale(g_libraryWindow, 260), TRUE);
	x += LibraryScale(g_libraryWindow, 176) + gap * 2;
	MoveWindow(g_libraryStatus, x, top,
		max(0, width - x - padding), rowHeight, TRUE);

	MoveWindow(g_bookView, 0, LibraryScale(g_libraryWindow, LIBRARY_TOOLBAR_HEIGHT),
		client.right, client.bottom - LibraryScale(g_libraryWindow,
			LIBRARY_TOOLBAR_HEIGHT), TRUE);
	ApplyRoundedWindowCorners(g_libraryWindow);
}

static BOOL CopyDirectoryRecursive(const WCHAR* source, const WCHAR* destination) {
	WCHAR pattern[MAX_PATH];
	WIN32_FIND_DATAW data;
	HANDLE find;
	BOOL success = TRUE;
	EnsureDirectory(destination);
	StringCchPrintfW(pattern, MAX_PATH, L"%s\\*", source);
	find = FindFirstFileW(pattern, &data);
	if (find == INVALID_HANDLE_VALUE) {
		return GetLastError() == ERROR_FILE_NOT_FOUND;
	}
	do {
		WCHAR sourcePath[MAX_PATH];
		WCHAR destinationPath[MAX_PATH];
		if (wcscmp(data.cFileName, L".") == 0 ||
			wcscmp(data.cFileName, L"..") == 0) {
			continue;
		}
		StringCchPrintfW(sourcePath, MAX_PATH, L"%s\\%s", source, data.cFileName);
		StringCchPrintfW(destinationPath, MAX_PATH, L"%s\\%s", destination,
			data.cFileName);
		if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (!CopyDirectoryRecursive(sourcePath, destinationPath)) {
				success = FALSE;
				break;
			}
		} else if (!CopyFileW(sourcePath, destinationPath, FALSE)) {
			success = FALSE;
			break;
		}
	} while (FindNextFileW(find, &data));
	FindClose(find);
	return success;
}

static BOOL DeleteDirectoryRecursive(const WCHAR* directory) {
	WCHAR pattern[MAX_PATH];
	WIN32_FIND_DATAW data;
	HANDLE find;
	BOOL success = TRUE;
	StringCchPrintfW(pattern, MAX_PATH, L"%s\\*", directory);
	find = FindFirstFileW(pattern, &data);
	if (find != INVALID_HANDLE_VALUE) {
		do {
			WCHAR path[MAX_PATH];
			if (wcscmp(data.cFileName, L".") == 0 ||
				wcscmp(data.cFileName, L"..") == 0) {
				continue;
			}
			StringCchPrintfW(path, MAX_PATH, L"%s\\%s", directory, data.cFileName);
			if (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				if (!DeleteDirectoryRecursive(path)) success = FALSE;
			} else if (!DeleteFileW(path)) {
				success = FALSE;
			}
		} while (FindNextFileW(find, &data));
		FindClose(find);
	}
	if (!RemoveDirectoryW(directory)) success = FALSE;
	return success;
}

static void EscapePowerShellPath(const WCHAR* source, LPWSTR destination,
	int destinationCount) {
	size_t i = 0;
	size_t j = 0;
	while (source[i] != L'\0' && j + 2 < (size_t)destinationCount) {
		if (source[i] == L'\'') {
			destination[j++] = L'\'';
		}
		destination[j++] = source[i++];
	}
	destination[j] = L'\0';
}

static BOOL CompressPortableStaging(const WCHAR* stage, const WCHAR* zipPath) {
	WCHAR powershell[MAX_PATH];
	WCHAR archiveModule[MAX_PATH];
	WCHAR exePath[MAX_PATH];
	WCHAR libraryPath[MAX_PATH];
	WCHAR readmePath[MAX_PATH];
	WCHAR escapedExe[MAX_PATH * 2];
	WCHAR escapedArchiveModule[MAX_PATH * 2];
	WCHAR escapedLibrary[MAX_PATH * 2];
	WCHAR escapedReadme[MAX_PATH * 2];
	WCHAR escapedZip[MAX_PATH * 2];
	WCHAR command[32768];
	STARTUPINFOW startup;
	PROCESS_INFORMATION process;
	BOOL success;
	DWORD exitCode = 1;
#ifdef VITRITE_LIBRARY_TEST
	HANDLE testLog = INVALID_HANDLE_VALUE;
#endif

	GetSystemDirectoryW(powershell, MAX_PATH);
	StringCchPrintfW(archiveModule, MAX_PATH,
		L"%s\\WindowsPowerShell\\v1.0\\Modules\\Microsoft.PowerShell.Archive\\Microsoft.PowerShell.Archive.psd1",
		powershell);
	StringCchCatW(powershell, MAX_PATH, L"\\WindowsPowerShell\\v1.0\\powershell.exe");
	StringCchPrintfW(exePath, MAX_PATH, L"%s\\Vitrite.exe", stage);
	StringCchPrintfW(libraryPath, MAX_PATH, L"%s\\library", stage);
	StringCchPrintfW(readmePath, MAX_PATH, L"%s\\PORTABLE_README.txt", stage);
	EscapePowerShellPath(exePath, escapedExe, MAX_PATH * 2);
	EscapePowerShellPath(archiveModule, escapedArchiveModule, MAX_PATH * 2);
	EscapePowerShellPath(libraryPath, escapedLibrary, MAX_PATH * 2);
	EscapePowerShellPath(readmePath, escapedReadme, MAX_PATH * 2);
	EscapePowerShellPath(zipPath, escapedZip, MAX_PATH * 2);
	StringCchPrintfW(command, 32768,
		L"\"%s\" -NoProfile -NonInteractive -WindowStyle Hidden -Command "
		L"\"$ErrorActionPreference='Stop'; Import-Module '%s' -Force; "
		L"Compress-Archive -LiteralPath "
		L"'%s','%s','%s' -DestinationPath '%s' -Force\"",
		powershell, escapedArchiveModule, escapedExe, escapedLibrary,
		escapedReadme, escapedZip);
#ifdef VITRITE_LIBRARY_TEST
	StringCchCopyW(g_libraryTestCommand, 32768, command);
#endif
	memset(&startup, 0, sizeof(startup));
	startup.cb = sizeof(startup);
#ifdef VITRITE_LIBRARY_TEST
	{
		WCHAR tempPath[MAX_PATH];
		GetTempPathW(MAX_PATH, tempPath);
		StringCchPrintfW(g_libraryTestPowerShellLog, MAX_PATH,
			L"%sVitritePortableTest.log", tempPath);
		testLog = CreateFileW(g_libraryTestPowerShellLog, GENERIC_WRITE,
			FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		if (testLog != INVALID_HANDLE_VALUE) {
			SetHandleInformation(testLog, HANDLE_FLAG_INHERIT,
				HANDLE_FLAG_INHERIT);
			startup.dwFlags |= STARTF_USESTDHANDLES;
			startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
			startup.hStdOutput = testLog;
			startup.hStdError = testLog;
		}
	}
#endif
	memset(&process, 0, sizeof(process));
	success = CreateProcessW(NULL, command, NULL, NULL,
#ifdef VITRITE_LIBRARY_TEST
		TRUE,
#else
		FALSE,
#endif
		CREATE_NO_WINDOW, NULL, NULL, &startup, &process);
#ifdef VITRITE_LIBRARY_TEST
	g_libraryTestCompressCreateSuccess = success;
	g_libraryTestCompressExitCode = exitCode;
#endif
	if (success) {
		WaitForSingleObject(process.hProcess, INFINITE);
		GetExitCodeProcess(process.hProcess, &exitCode);
#ifdef VITRITE_LIBRARY_TEST
		g_libraryTestCompressExitCode = exitCode;
#endif
		CloseHandle(process.hThread);
		CloseHandle(process.hProcess);
	}
#ifdef VITRITE_LIBRARY_TEST
	if (testLog != INVALID_HANDLE_VALUE) CloseHandle(testLog);
#endif
	return success && exitCode == 0 &&
		GetFileAttributesW(zipPath) != INVALID_FILE_ATTRIBUTES;
}

static BOOL BuildPortablePackage(const WCHAR* zipPath) {
	WCHAR stage[MAX_PATH];
	WCHAR temp[MAX_PATH];
	WCHAR executable[MAX_PATH];
	WCHAR destinationExe[MAX_PATH];
	WCHAR destinationLibrary[MAX_PATH];
	WCHAR readmePath[MAX_PATH];
	const char* readme =
		"\xEF\xBB\xBFVitrite 便携书库\r\n\r\n"
		"解压全部文件后，直接运行 Vitrite.exe。\r\n"
		"程序会自动读取同目录下 library 文件夹中的书籍、作者和特别喜欢标记。\r\n"
		"不需要重新导入，也不需要设置书库路径。\r\n";
	HANDLE file;
	DWORD written;
	BOOL success;

#ifdef VITRITE_LIBRARY_TEST
	g_libraryTestExportStage = 0;
#endif
	GetTempPathW(MAX_PATH, temp);
	StringCchPrintfW(stage, MAX_PATH, L"%sVitritePortable-%lu-%llu", temp,
		GetCurrentProcessId(), GetTickCount64());
#ifdef VITRITE_LIBRARY_TEST
	StringCchCopyW(g_libraryTestStagingPath, MAX_PATH, stage);
#endif
	if (!CreateDirectoryW(stage, NULL)) {
#ifdef VITRITE_LIBRARY_TEST
		g_libraryTestExportStage = 1;
#endif
		return FALSE;
	}
	GetModuleFileNameW(NULL, executable, MAX_PATH);
	StringCchPrintfW(destinationExe, MAX_PATH, L"%s\\Vitrite.exe", stage);
	StringCchPrintfW(destinationLibrary, MAX_PATH, L"%s\\library", stage);
	StringCchPrintfW(readmePath, MAX_PATH, L"%s\\PORTABLE_README.txt", stage);
	success = CopyFileW(executable, destinationExe, FALSE);
	if (!success) {
#ifdef VITRITE_LIBRARY_TEST
		g_libraryTestExportStage = 2;
#endif
	} else {
		success = CopyDirectoryRecursive(g_libraryRoot, destinationLibrary);
		if (!success) {
#ifdef VITRITE_LIBRARY_TEST
			g_libraryTestExportStage = 3;
#endif
		}
	}
	file = CreateFileW(readmePath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		success = FALSE;
#ifdef VITRITE_LIBRARY_TEST
		g_libraryTestExportStage = 4;
#endif
	} else {
		if (!WriteFile(file, readme, (DWORD)strlen(readme), &written, NULL)) {
			success = FALSE;
#ifdef VITRITE_LIBRARY_TEST
			g_libraryTestExportStage = 5;
#endif
		}
		CloseHandle(file);
	}
	if (success) {
		success = CompressPortableStaging(stage, zipPath);
		if (!success) {
#ifdef VITRITE_LIBRARY_TEST
			g_libraryTestExportStage = 6;
#endif
		}
	}
#ifdef VITRITE_LIBRARY_TEST
	if (success) {
		DeleteDirectoryRecursive(stage);
	}
#else
	DeleteDirectoryRecursive(stage);
#endif
	return success;
}

static BOOL PathIsInsideLibrary(const WCHAR* path) {
	size_t rootLength = wcslen(g_libraryRoot);
	if (_wcsnicmp(path, g_libraryRoot, rootLength) != 0) {
		return FALSE;
	}
	return path[rootLength] == L'\0' || path[rootLength] == L'\\' ||
		path[rootLength] == L'/';
}

static void ExportPortableLibrary(HWND owner) {
	OPENFILENAMEW dialog;
	WCHAR zipPath[MAX_PATH];
	BOOL success;
	SYSTEMTIME currentTime;

	SaveLibraryBooks();
	memset(zipPath, 0, sizeof(zipPath));
	GetLocalTime(&currentTime);
	StringCchPrintfW(zipPath, MAX_PATH, L"Vitrite-Portable-Library-%04d%02d%02d.zip",
		currentTime.wYear, currentTime.wMonth, currentTime.wDay);
	memset(&dialog, 0, sizeof(dialog));
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = owner;
	dialog.lpstrFilter = L"ZIP 压缩包 (*.zip)\0*.zip\0\0";
	dialog.lpstrFile = zipPath;
	dialog.nMaxFile = MAX_PATH;
	dialog.lpstrDefExt = L"zip";
	dialog.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_EXPLORER;
	dialog.lpstrTitle = L"导出便携书库";
	if (!GetSaveFileNameW(&dialog)) {
		return;
	}
	if (PathIsInsideLibrary(zipPath)) {
		MessageBoxW(owner, L"请不要把便携包保存到书库目录内部。",
			LIBRARY_TITLE, MB_OK | MB_ICONWARNING);
		return;
	}
	SetWindowTextW(g_libraryStatus, L"正在生成便携书库，请稍候...");
	UpdateWindow(g_libraryStatus);
	success = BuildPortablePackage(zipPath);
	UpdateLibraryStatus();
	if (success) {
		MessageBoxW(owner,
			L"便携书库已导出：\r\n另一台电脑解压后直接运行 Vitrite.exe 即可阅读。",
			LIBRARY_TITLE, MB_OK | MB_ICONINFORMATION);
	} else {
		MessageBoxW(owner, L"导出便携书库失败。", LIBRARY_TITLE,
			MB_OK | MB_ICONERROR);
	}
}

static INT_PTR CALLBACK AuthorDialogProc(HWND dialog, UINT message,
	WPARAM wParam, LPARAM lParam) {
	AUTHOR_DIALOG_DATA* data;
	switch (message) {
	case WM_INITDIALOG:
		data = (AUTHOR_DIALOG_DATA*)lParam;
		SetWindowLongPtrW(dialog, DWLP_USER, (LONG_PTR)data);
		SetDlgItemTextW(dialog, IDC_BOOK_AUTHOR_TITLE, data->title);
		SetDlgItemTextW(dialog, IDC_BOOK_AUTHOR_NAME, data->author);
		ApplyRoundedWindowCorners(dialog);
		SendDlgItemMessageW(dialog, IDC_BOOK_AUTHOR_NAME, EM_SETSEL, 0, -1);
		SetFocus(GetDlgItem(dialog, IDC_BOOK_AUTHOR_NAME));
		return FALSE;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDOK) {
			data = (AUTHOR_DIALOG_DATA*)GetWindowLongPtrW(dialog, DWLP_USER);
			if (data != NULL) {
				GetDlgItemTextW(dialog, IDC_BOOK_AUTHOR_NAME,
					data->author, 128);
				while (data->author[0] == L' ' || data->author[0] == L'\t') {
					memmove(data->author, data->author + 1,
						wcslen(data->author) * sizeof(WCHAR));
				}
				{
					size_t length = wcslen(data->author);
					while (length > 0 &&
						(data->author[length - 1] == L' ' ||
						 data->author[length - 1] == L'\t')) {
						data->author[--length] = L'\0';
					}
				}
			}
			EndDialog(dialog, IDOK);
			return TRUE;
		}
		if (LOWORD(wParam) == IDCANCEL) {
			EndDialog(dialog, IDCANCEL);
			return TRUE;
		}
		break;
	case WM_CLOSE:
		EndDialog(dialog, IDCANCEL);
		return TRUE;
	}
	return FALSE;
}

static BOOL PromptBookAuthor(HWND owner, const WCHAR* title,
	LPWSTR author, int authorCount) {
	AUTHOR_DIALOG_DATA data;
	memset(&data, 0, sizeof(data));
	StringCchCopyW(data.title, 192, title);
	if (author != NULL) {
		StringCchCopyW(data.author, 128, author);
	}
	INT_PTR result = DialogBoxParamW(g_libraryInstance,
		MAKEINTRESOURCEW(IDD_BOOK_AUTHOR), owner, AuthorDialogProc,
		(LPARAM)&data);
	if (result == IDOK) {
		if (author != NULL) {
			StringCchCopyW(author, (size_t)authorCount, data.author);
		}
		return TRUE;
	}
	return FALSE;
}

static void EditBookAuthor(HWND owner, int bookIndex) {
	WCHAR author[128];
	if (bookIndex < 0 || bookIndex >= g_bookCount) {
		return;
	}
	StringCchCopyW(author, 128, g_books[bookIndex].author);
	if (PromptBookAuthor(owner, g_books[bookIndex].title, author, 128)) {
		StringCchCopyW(g_books[bookIndex].author, 128, author);
		SaveLibraryBooks();
		PopulateAuthorFilter();
		RefreshLibraryView();
	}
}

static BOOL BuildUniqueDestination(const WCHAR* sourcePath, LPWSTR relativePath,
	int relativeCount, LPWSTR fullPath, int fullCount) {
	WCHAR fileName[MAX_PATH];
	WCHAR baseName[MAX_PATH];
	WCHAR extension[16];
	LPWSTR dot;
	int suffix;
	const WCHAR* sourceName = wcsrchr(sourcePath, L'\\');
	if (sourceName == NULL) {
		sourceName = wcsrchr(sourcePath, L'/');
	}
	sourceName = sourceName != NULL ? sourceName + 1 : sourcePath;
	StringCchCopyW(fileName, MAX_PATH, sourceName);
	StringCchCopyW(baseName, MAX_PATH, fileName);
	dot = wcsrchr(baseName, L'.');
	extension[0] = L'\0';
	if (dot != NULL) {
		StringCchCopyW(extension, 16, dot);
		*dot = L'\0';
	}
	for (suffix = 0; suffix < 10000; suffix++) {
		if (suffix == 0) {
			StringCchCopyW(fileName, MAX_PATH, sourceName);
		} else {
			StringCchPrintfW(fileName, MAX_PATH, L"%s (%d)%s",
				baseName, suffix + 1, extension);
		}
		StringCchPrintfW(fullPath, (size_t)fullCount, L"%s\\%s",
			g_libraryRoot, fileName);
		if (GetFileAttributesW(fullPath) == INVALID_FILE_ATTRIBUTES) {
			StringCchCopyW(relativePath, (size_t)relativeCount, fileName);
			return TRUE;
		}
		if (lstrcmpiW(sourcePath, fullPath) == 0) {
			StringCchCopyW(relativePath, (size_t)relativeCount, fileName);
			return TRUE;
		}
	}
	return FALSE;
}

static BOOL ImportSingleBook(HWND owner, const WCHAR* sourcePath,
	const WCHAR* author) {
	WCHAR relativePath[MAX_PATH];
	WCHAR destination[MAX_PATH];
	WCHAR title[192];
	const WCHAR* fileName;
	LIBRARY_BOOK* book;
	int i;

	if (sourcePath == NULL || !IsTxtFile(sourcePath) ||
		GetFileAttributesW(sourcePath) == INVALID_FILE_ATTRIBUTES) {
		return FALSE;
	}
	if (g_bookCount >= LIBRARY_MAX_BOOKS) {
		MessageBoxW(owner, L"书库中的书籍数量已达到上限。",
			LIBRARY_TITLE, MB_OK | MB_ICONWARNING);
		return FALSE;
	}
	EnsureDirectory(g_libraryRoot);
	if (!BuildUniqueDestination(sourcePath, relativePath, MAX_PATH,
			destination, MAX_PATH)) {
		return FALSE;
	}
	if (lstrcmpiW(sourcePath, destination) != 0 &&
		!CopyFileW(sourcePath, destination, FALSE)) {
		MessageBoxW(owner, L"无法复制 TXT 文件到书库目录。",
			LIBRARY_TITLE, MB_OK | MB_ICONERROR);
		return FALSE;
	}
	for (i = 0; i < g_bookCount; i++) {
		if (lstrcmpiW(g_books[i].relativePath, relativePath) == 0) {
			return TRUE;
		}
	}
	book = &g_books[g_bookCount++];
	memset(book, 0, sizeof(*book));
	StringCchCopyW(book->relativePath, MAX_PATH, relativePath);
	fileName = wcsrchr(relativePath, L'\\');
	fileName = fileName != NULL ? fileName + 1 : relativePath;
	TitleFromFileName(fileName, title, 192);
	StringCchCopyW(book->title, 192, title);
	if (author != NULL) {
		StringCchCopyW(book->author, 128, author);
	}
	return TRUE;
}

static void ImportDroppedFiles(HWND owner, HDROP drop) {
	UINT count = DragQueryFileW(drop, 0xFFFFFFFF, NULL, 0);
	UINT i;
	UINT imported = 0;
	UINT skipped = 0;
	WCHAR path[MAX_PATH];
	WCHAR title[192];
	WCHAR author[128];
	const WCHAR* fileName;

	for (i = 0; i < count; i++) {
		if (DragQueryFileW(drop, i, path, MAX_PATH) == 0) {
			continue;
		}
		if (!IsTxtFile(path) ||
			GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES ||
			(GetFileAttributesW(path) & FILE_ATTRIBUTE_DIRECTORY)) {
			skipped++;
			continue;
		}
		fileName = wcsrchr(path, L'\\');
		fileName = fileName != NULL ? fileName + 1 : path;
		TitleFromFileName(fileName, title, 192);
		author[0] = L'\0';
		PromptBookAuthor(owner, title, author, 128);
		if (ImportSingleBook(owner, path, author)) {
			imported++;
		} else {
			skipped++;
		}
	}
	DragFinish(drop);
	SaveLibraryBooks();
	PopulateAuthorFilter();
	RefreshLibraryView();
	if (imported == 0 && skipped > 0) {
		MessageBoxW(owner, L"没有导入任何 TXT 文件。",
			LIBRARY_TITLE, MB_OK | MB_ICONWARNING);
	}
}

static void ImportTxtFilesFromDialog(HWND owner) {
	OPENFILENAMEW dialog;
	WCHAR* buffer;
	WCHAR directory[MAX_PATH];
	WCHAR author[128];
	WCHAR title[192];
	const WCHAR* item;
	const WCHAR* fileName;
	UINT imported = 0;

	buffer = (WCHAR*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
		65536 * sizeof(WCHAR));
	if (buffer == NULL) {
		return;
	}
	memset(&dialog, 0, sizeof(dialog));
	dialog.lStructSize = sizeof(dialog);
	dialog.hwndOwner = owner;
	dialog.lpstrFilter = L"文本文件 (*.txt)\0*.txt\0\0";
	dialog.lpstrFile = buffer;
	dialog.nMaxFile = 65536;
	dialog.lpstrDefExt = L"txt";
	dialog.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER |
		OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	dialog.lpstrTitle = L"导入 TXT 到书架";
	if (!GetOpenFileNameW(&dialog)) {
		HeapFree(GetProcessHeap(), 0, buffer);
		return;
	}
	item = buffer;
	if (buffer[wcslen(buffer) + 1] == L'\0') {
		fileName = wcsrchr(buffer, L'\\');
		fileName = fileName != NULL ? fileName + 1 : buffer;
		TitleFromFileName(fileName, title, 192);
		author[0] = L'\0';
		PromptBookAuthor(owner, title, author, 128);
		if (ImportSingleBook(owner, buffer, author)) imported++;
	} else {
		StringCchCopyW(directory, MAX_PATH, buffer);
		item = buffer + wcslen(buffer) + 1;
		while (*item != L'\0') {
			WCHAR path[MAX_PATH];
			StringCchPrintfW(path, MAX_PATH, L"%s\\%s", directory, item);
			TitleFromFileName(item, title, 192);
			author[0] = L'\0';
			PromptBookAuthor(owner, title, author, 128);
			if (ImportSingleBook(owner, path, author)) imported++;
			item += wcslen(item) + 1;
		}
	}
	HeapFree(GetProcessHeap(), 0, buffer);
	SaveLibraryBooks();
	PopulateAuthorFilter();
	RefreshLibraryView();
	if (imported == 0) {
		MessageBoxW(owner, L"没有导入任何 TXT 文件。",
			LIBRARY_TITLE, MB_OK | MB_ICONWARNING);
	}
}

static void ChooseLibraryFolder(HWND owner) {
	BROWSEINFOW browse;
	PIDLIST_ABSOLUTE item;
	WCHAR path[MAX_PATH];
	memset(&browse, 0, sizeof(browse));
	browse.hwndOwner = owner;
	browse.lpszTitle = L"选择书库目录";
	browse.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	item = SHBrowseForFolderW(&browse);
	if (item == NULL) {
		return;
	}
	if (SHGetPathFromIDListW(item, path)) {
		NormalizeLibraryRoot(path);
		EnsureDirectory(path);
		StringCchCopyW(g_libraryRoot, MAX_PATH, path);
		SaveLibraryRoot();
		LoadLibraryBooks();
		PopulateAuthorFilter();
		RefreshLibraryView();
		SetWindowTextW(g_libraryWindow, LIBRARY_TITLE);
	}
	CoTaskMemFree(item);
}

static void PopulateLibraryCombos(void) {
	SendMessageW(g_librarySortCombo, CB_ADDSTRING, 0, (LPARAM)L"按书名拼音");
	SendMessageW(g_librarySortCombo, CB_ADDSTRING, 0, (LPARAM)L"按作者分类");
	SendMessageW(g_librarySortCombo, CB_SETCURSEL, g_librarySortMode, 0);
	SendMessageW(g_libraryFavoriteCombo, CB_ADDSTRING, 0, (LPARAM)L"全部书籍");
	SendMessageW(g_libraryFavoriteCombo, CB_ADDSTRING, 0, (LPARAM)L"只显示特别喜欢");
	SendMessageW(g_libraryFavoriteCombo, CB_ADDSTRING, 0, (LPARAM)L"只显示未特别喜欢");
	SendMessageW(g_libraryFavoriteCombo, CB_SETCURSEL,
		g_libraryFavoriteFilter, 0);
	PopulateAuthorFilter();
}

static void CreateLibraryControls(HWND hwnd) {
	g_libraryBackgroundBrush = CreateSolidBrush(LIBRARY_BG_COLOR);
	g_libraryToolbarBrush = CreateSolidBrush(LIBRARY_TOOLBAR_COLOR);
	g_libraryCardBrush = CreateSolidBrush(LIBRARY_CARD_COLOR);
	RecreateLibraryFonts(hwnd);

	g_libraryImportButton = CreateWindowExW(0, L"BUTTON", L"导入 TXT",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hwnd,
		(HMENU)(INT_PTR)IDC_LIBRARY_IMPORT, g_libraryInstance, NULL);
	g_librarySetPathButton = CreateWindowExW(0, L"BUTTON", L"设置书库...",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hwnd,
		(HMENU)(INT_PTR)IDC_LIBRARY_SET_PATH, g_libraryInstance, NULL);
	g_libraryExportButton = CreateWindowExW(0, L"BUTTON", L"导出便携包...",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hwnd,
		(HMENU)(INT_PTR)IDC_LIBRARY_EXPORT, g_libraryInstance, NULL);
	g_libraryReaderButton = CreateWindowExW(0, L"BUTTON", L"阅读器",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP, 0, 0, 0, 0, hwnd,
		(HMENU)(INT_PTR)IDC_LIBRARY_READER, g_libraryInstance, NULL);
	g_librarySortLabel = CreateWindowExW(0, L"STATIC", L"排序",
		WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd,
		(HMENU)(INT_PTR)IDC_LIBRARY_SORT_LABEL, g_libraryInstance, NULL);
	g_librarySortCombo = CreateWindowExW(0, WC_COMBOBOXW, L"",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_LIBRARY_SORT, g_libraryInstance, NULL);
	g_libraryFilterLabel = CreateWindowExW(0, L"STATIC", L"筛选",
		WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd,
		(HMENU)(INT_PTR)IDC_LIBRARY_FILTER_LABEL, g_libraryInstance, NULL);
	g_libraryFavoriteCombo = CreateWindowExW(0, WC_COMBOBOXW, L"",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_LIBRARY_FAVORITE_FILTER,
		g_libraryInstance, NULL);
	g_libraryAuthorLabel = CreateWindowExW(0, L"STATIC", L"作者",
		WS_CHILD | WS_VISIBLE | SS_LEFT, 0, 0, 0, 0, hwnd,
		(HMENU)(INT_PTR)IDC_LIBRARY_AUTHOR_LABEL, g_libraryInstance, NULL);
	g_libraryAuthorCombo = CreateWindowExW(0, WC_COMBOBOXW, L"",
		WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST | WS_VSCROLL,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_LIBRARY_AUTHOR_FILTER,
		g_libraryInstance, NULL);
	g_libraryStatus = CreateWindowExW(0, L"STATIC", L"",
		WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE | SS_ENDELLIPSIS,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_LIBRARY_STATUS,
		g_libraryInstance, NULL);
	g_bookView = CreateWindowExW(0, BOOK_VIEW_CLASS_NAME, L"",
		WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_TABSTOP,
		0, 0, 0, 0, hwnd, (HMENU)(INT_PTR)IDC_LIBRARY_VIEW,
		g_libraryInstance, NULL);

	PopulateLibraryCombos();
	ApplyLibraryFonts();
	LayoutLibraryWindow();
	RefreshLibraryView();
}

static LRESULT CALLBACK LibraryWindowProc(HWND hwnd, UINT message,
	WPARAM wParam, LPARAM lParam) {
	switch (message) {
	case WM_CREATE:
		g_libraryWindow = hwnd;
		CreateLibraryControls(hwnd);
		DragAcceptFiles(hwnd, TRUE);
		ApplyRoundedWindowCorners(hwnd);
		return 0;
	case WM_SIZE:
		LayoutLibraryWindow();
		return 0;
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = LibraryScale(hwnd, 760);
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = LibraryScale(hwnd, 500);
		return 0;
	case WM_DPICHANGED:
	{
		RECT* suggested = (RECT*)lParam;
		SetWindowPos(hwnd, NULL, suggested->left, suggested->top,
			suggested->right - suggested->left,
			suggested->bottom - suggested->top,
			SWP_NOZORDER | SWP_NOACTIVATE);
		RecreateLibraryFonts(hwnd);
		ApplyLibraryFonts();
		LayoutLibraryWindow();
		return 0;
	}
	case WM_ERASEBKGND:
	{
		RECT client;
		RECT toolbar;
		GetClientRect(hwnd, &client);
		FillRect((HDC)wParam, &client, g_libraryBackgroundBrush);
		toolbar = client;
		toolbar.bottom = LibraryScale(hwnd, LIBRARY_TOOLBAR_HEIGHT);
		FillRect((HDC)wParam, &toolbar, g_libraryToolbarBrush);
		return 1;
	}
	case WM_CTLCOLORSTATIC:
		SetBkMode((HDC)wParam, TRANSPARENT);
		SetTextColor((HDC)wParam, LIBRARY_SUBTLE_COLOR);
		return (LRESULT)g_libraryToolbarBrush;
	case WM_DROPFILES:
		ImportDroppedFiles(hwnd, (HDROP)wParam);
		return 0;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_LIBRARY_IMPORT:
			ImportTxtFilesFromDialog(hwnd);
			return 0;
		case IDC_LIBRARY_SET_PATH:
			ChooseLibraryFolder(hwnd);
			return 0;
		case IDC_LIBRARY_EXPORT:
			ExportPortableLibrary(hwnd);
			return 0;
		case IDC_LIBRARY_READER:
			BringTextReaderToFront();
			ShowWindow(hwnd, SW_HIDE);
			return 0;
		case IDC_LIBRARY_SORT:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				g_librarySortMode = (int)SendMessageW(g_librarySortCombo,
					CB_GETCURSEL, 0, 0);
				SaveLibraryRoot();
				RefreshLibraryView();
			}
			return 0;
		case IDC_LIBRARY_FAVORITE_FILTER:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				g_libraryFavoriteFilter = (int)SendMessageW(
					g_libraryFavoriteCombo, CB_GETCURSEL, 0, 0);
				SaveLibraryRoot();
				RefreshLibraryView();
			}
			return 0;
		case IDC_LIBRARY_AUTHOR_FILTER:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				int selected = (int)SendMessageW(g_libraryAuthorCombo,
					CB_GETCURSEL, 0, 0);
				if (selected <= 0) {
					g_libraryAuthorFilter[0] = L'\0';
				} else {
					SendMessageW(g_libraryAuthorCombo, CB_GETLBTEXT,
						selected, (LPARAM)g_libraryAuthorFilter);
				}
				SaveLibraryRoot();
				RefreshLibraryView();
			}
			return 0;
		}
		return 0;
	case WM_CLOSE:
		SaveLibraryBooks();
		SaveLibraryRoot();
		SaveLibraryWindowRect();
		DestroyWindow(hwnd);
		return 0;
	case WM_DESTROY:
		DragAcceptFiles(hwnd, FALSE);
		g_libraryWindow = NULL;
		return 0;
	}
	return DefWindowProcW(hwnd, message, wParam, lParam);
}

BOOL InitializeLibrary(HINSTANCE instance) {
	WNDCLASSEXW windowClass;
	g_libraryInstance = instance;
	g_books = (LIBRARY_BOOK*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY,
		LIBRARY_MAX_BOOKS * sizeof(LIBRARY_BOOK));
	if (g_books == NULL) {
		return FALSE;
	}
	if (!g_libraryClassRegistered) {
		memset(&windowClass, 0, sizeof(windowClass));
		windowClass.cbSize = sizeof(windowClass);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = LibraryWindowProc;
		windowClass.hInstance = instance;
		windowClass.hIcon = (HICON)LoadImageW(instance,
			MAKEINTRESOURCEW(IDI_ICON1), IMAGE_ICON, 32, 32, LR_SHARED);
		windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
		windowClass.lpszClassName = LIBRARY_CLASS_NAME;
		if (!RegisterClassExW(&windowClass)) {
			return FALSE;
		}
		g_libraryClassRegistered = TRUE;
	}
	if (!g_bookViewClassRegistered) {
		memset(&windowClass, 0, sizeof(windowClass));
		windowClass.cbSize = sizeof(windowClass);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = BookViewWndProc;
		windowClass.hInstance = instance;
		windowClass.hCursor = LoadCursorW(NULL, IDC_ARROW);
		windowClass.lpszClassName = BOOK_VIEW_CLASS_NAME;
		if (!RegisterClassExW(&windowClass)) {
			return FALSE;
		}
		g_bookViewClassRegistered = TRUE;
	}
	LoadLibraryRoot();
	LoadLibraryWindowRect();
	LoadLibraryBooks();
	return TRUE;
}

BOOL LibraryHasPortableData(void) {
	WCHAR path[MAX_PATH];
	return GetPortableLibraryRoot(path, MAX_PATH);
}

void ShowLibraryWindow(HWND owner) {
	MONITORINFO monitorInfo;
	HMONITOR monitor;
	POINT point;
	int width;
	int height;
	int x;
	int y;

	if (!g_libraryClassRegistered) {
		return;
	}
	if (g_libraryWindow != NULL && IsWindow(g_libraryWindow)) {
		ShowWindow(g_libraryWindow, SW_RESTORE);
		SetForegroundWindow(g_libraryWindow);
		RefreshLibraryView();
		return;
	}
	width = g_librarySavedRect.right >= 720 ? g_librarySavedRect.right :
		LibraryScale(NULL, 1040);
	height = g_librarySavedRect.bottom >= 480 ? g_librarySavedRect.bottom :
		LibraryScale(NULL, 720);
	if (g_librarySavedRect.right >= 720) {
		x = g_librarySavedRect.left;
		y = g_librarySavedRect.top;
		point.x = x;
		point.y = y;
		monitor = MonitorFromPoint(point, MONITOR_DEFAULTTONEAREST);
		memset(&monitorInfo, 0, sizeof(monitorInfo));
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfoW(monitor, &monitorInfo) &&
			(x + width > monitorInfo.rcWork.right ||
			 y + height > monitorInfo.rcWork.bottom)) {
			x = monitorInfo.rcWork.left +
				(monitorInfo.rcWork.right - monitorInfo.rcWork.left - width) / 2;
			y = monitorInfo.rcWork.top +
				(monitorInfo.rcWork.bottom - monitorInfo.rcWork.top - height) / 2;
		}
	} else {
		x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
		y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
	}
	g_libraryWindow = CreateWindowExW(WS_EX_APPWINDOW, LIBRARY_CLASS_NAME,
		LIBRARY_TITLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		x, y, width, height, owner, NULL, g_libraryInstance, NULL);
	if (g_libraryWindow == NULL) {
		return;
	}
	ShowWindow(g_libraryWindow, SW_SHOW);
	UpdateWindow(g_libraryWindow);
	SetForegroundWindow(g_libraryWindow);
}

void DestroyLibraryWindow(void) {
	if (g_libraryWindow != NULL && IsWindow(g_libraryWindow)) {
		SaveLibraryBooks();
		SaveLibraryRoot();
		SaveLibraryWindowRect();
		DestroyWindow(g_libraryWindow);
	}
}

void CleanupLibrary(void) {
	DestroyLibraryWindow();
	if (g_libraryTitleFont != NULL) DeleteObject(g_libraryTitleFont);
	if (g_libraryBodyFont != NULL) DeleteObject(g_libraryBodyFont);
	if (g_librarySmallFont != NULL) DeleteObject(g_librarySmallFont);
	if (g_libraryUiFont != NULL) DeleteObject(g_libraryUiFont);
	g_libraryTitleFont = NULL;
	g_libraryBodyFont = NULL;
	g_librarySmallFont = NULL;
	g_libraryUiFont = NULL;
	if (g_libraryBackgroundBrush != NULL) DeleteObject(g_libraryBackgroundBrush);
	if (g_libraryToolbarBrush != NULL) DeleteObject(g_libraryToolbarBrush);
	if (g_libraryCardBrush != NULL) DeleteObject(g_libraryCardBrush);
	g_libraryBackgroundBrush = NULL;
	g_libraryToolbarBrush = NULL;
	g_libraryCardBrush = NULL;
	if (g_libraryClassRegistered) {
		UnregisterClassW(LIBRARY_CLASS_NAME, g_libraryInstance);
		g_libraryClassRegistered = FALSE;
	}
	if (g_bookViewClassRegistered) {
		UnregisterClassW(BOOK_VIEW_CLASS_NAME, g_libraryInstance);
		g_bookViewClassRegistered = FALSE;
	}
	if (g_books != NULL) {
		HeapFree(GetProcessHeap(), 0, g_books);
		g_books = NULL;
	}
}

#ifdef VITRITE_LIBRARY_TEST
BOOL LibraryTestSetRoot(const WCHAR* root) {
	if (root == NULL || root[0] == L'\0') {
		return FALSE;
	}
	StringCchCopyW(g_libraryRoot, MAX_PATH, root);
	NormalizeLibraryRoot(g_libraryRoot);
	EnsureDirectory(g_libraryRoot);
	LoadLibraryBooks();
	return TRUE;
}

BOOL LibraryTestImportFile(const WCHAR* path, const WCHAR* author) {
	return ImportSingleBook(NULL, path, author);
}

int LibraryTestGetVisibleCount(void) {
	return g_visibleBookCount;
}

const WCHAR* LibraryTestGetVisibleTitle(int index) {
	if (index < 0 || index >= g_visibleBookCount) {
		return NULL;
	}
	return g_books[g_visibleBooks[index]].title;
}

const WCHAR* LibraryTestGetVisibleAuthor(int index) {
	if (index < 0 || index >= g_visibleBookCount) {
		return NULL;
	}
	return g_books[g_visibleBooks[index]].author;
}

BOOL LibraryTestGetVisibleFavorite(int index) {
	if (index < 0 || index >= g_visibleBookCount) {
		return FALSE;
	}
	return g_books[g_visibleBooks[index]].favorite;
}

void LibraryTestSetView(int sortMode, int favoriteFilter,
	const WCHAR* authorFilter) {
	g_librarySortMode = sortMode;
	g_libraryFavoriteFilter = favoriteFilter;
	if (authorFilter == NULL) {
		g_libraryAuthorFilter[0] = L'\0';
	} else {
		StringCchCopyW(g_libraryAuthorFilter, 128, authorFilter);
	}
	RefreshLibraryView();
}

BOOL LibraryTestToggleVisibleFavorite(int index) {
	if (index < 0 || index >= g_visibleBookCount) {
		return FALSE;
	}
	ToggleFavoriteByVisibleIndex(index);
	return TRUE;
}

void LibraryTestSave(void) {
	SaveLibraryBooks();
}

BOOL LibraryTestExport(const WCHAR* path) {
	return BuildPortablePackage(path);
}

int LibraryTestGetExportFailureStage(void) {
	return g_libraryTestExportStage;
}

BOOL LibraryTestGetCompressCreateSuccess(void) {
	return g_libraryTestCompressCreateSuccess;
}

DWORD LibraryTestGetCompressExitCode(void) {
	return g_libraryTestCompressExitCode;
}

const WCHAR* LibraryTestGetStagingPath(void) {
	return g_libraryTestStagingPath;
}

const WCHAR* LibraryTestGetCompressCommand(void) {
	return g_libraryTestCommand;
}

const WCHAR* LibraryTestGetPowerShellLogPath(void) {
	return g_libraryTestPowerShellLog;
}

HWND LibraryTestGetWindow(void) {
	return g_libraryWindow;
}
#endif
