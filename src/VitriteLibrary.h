#ifndef VITRITE_LIBRARY_H
#define VITRITE_LIBRARY_H

#include <windows.h>

BOOL InitializeLibrary(HINSTANCE instance);
void CleanupLibrary(void);
void ShowLibraryWindow(HWND owner);
void DestroyLibraryWindow(void);
BOOL LibraryHasPortableData(void);

#endif
