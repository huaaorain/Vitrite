#ifndef VITRITE_READER_H
#define VITRITE_READER_H

#include <windows.h>

BOOL InitializeTextReader(HINSTANCE instance);
void CleanupTextReader(void);
BOOL OpenTextReaderWindow(HWND owner, LPCTSTR filePath);
BOOL OpenTextReaderFromCommandLine(HWND owner, LPCTSTR commandLine);
BOOL ExtractTextReaderPathFromCommandLine(LPCTSTR commandLine, LPTSTR filePath, int filePathCount);
void BringTextReaderToFront(void);
void DestroyTextReaderWindow(void);

#endif
