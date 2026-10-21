# Development Log

## 2026-10-21: Integrated TXT Reader

### Summary
Added a dedicated reading window while preserving the existing transparency and always-on-top workflow.

### Changes
*   Added a RichEdit-based TXT reader with width-aware automatic word and character wrapping.
*   Added UTF-8, UTF-16 LE/BE, BOM, and system-codepage loading with normalized line endings.
*   Added adjustable font size, selectable window opacity, keyboard shortcuts, and persisted reader settings.
*   Double-clicking the tray icon now opens the reader; the tray menu also exposes the reader explicitly.
*   Added single-instance file handoff so opening another TXT reuses the existing reader process.
*   Added a `--startup` launch mode so Windows sign-in starts the tray utility silently.

## 2026-01-05: Project Revival & Modernization

### Summary
Revived the 18-year-old Vitrite codebase for Windows 10/11. The original goal was to modernize the utility while maintaining its lightweight nature.

### Changes Implemented

#### 1. Core Codebase
*   **Removed Legacy Hooks**: Replaced `SetWindowsHookEx` (DLL injection) with `RegisterHotKey`. This eliminates the need for a separate DLL and prevents AV false positives.
*   **Crash Recovery**: Added `RegisterWindowMessage("TaskbarCreated")` to handle `explorer.exe` restarts. The tray icon now automatically reappears if the shell crashes.
*   **Input Handling**: Fixed tray icon click handling. Windows 10/11 uses `NIN_SELECT` (0x400) for `NOTIFYICON_VERSION_4` click events, which was missing. Added `NIN_KEYSELECT` for keyboard accessibility (Enter key on tray icon).

#### 2. User Interface
*   **High DPI**: Added `PerMonitorV2` manifest to support high-resolution displays.
*   **Dark Mode**: Implemented `DwmSetWindowAttribute` to support dark mode title bars on Windows 10/11.
*   **Localization**: Added Japanese (ja-JP) resource strings. Now utilizing standard Windows resource loading to switch between English, Chinese, and Japanese based on system UI language.

#### 3. Documentation
*   **README**: Replaced legacy text file with modern Markdown (`README.md`), including badges and clear build instructions.
*   **License**: Added `LICENSE` file (GPL v2) to comply with the original author's intent found in source comments.

### Pending / Future Ideas
*   GitHub Actions workflow for automated release builds.
*   Installer (MSI/Inno Setup)? (Current standalone EXE is likely sufficient).
