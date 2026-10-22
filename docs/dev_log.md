# Development Log

## 2026-10-22: Transparent Interaction and Opacity Controls

### Changes
*   Corrected the 90% global opacity target.
*   Added Ctrl+wheel opacity adjustment in ten-percent steps.
*   Added a clickable, nearly invisible hit layer for color-key transparent reading surfaces.
*   Forwarded clicks, dragging, context menus, and wheel input from the hit layer to RichEdit.
*   Increased wheel scrolling to three lines per notch.
*   Preserved color-key flags during opacity animations.

## 2026-10-22: White Reader and Transparent Surface

### Changes
*   Added periodic opacity reconciliation so global shortcut changes appear in the reader selector.
*   Preserved layered color keys during shared opacity animations.
*   Unified normal surfaces around white and removed the purple action accent.
*   Added white/transparent reader backgrounds with persistent configuration.
*   Added background luminance sampling and automatic black/white text in transparent mode.
*   Removed the RichEdit client border and request a borderless DWM frame where supported.

## 2026-10-22: Reader UI Redesign

### Design
*   Added the Stark Windows reader decision brief and visual specification in `docs/ui-redesign.md`.
*   Kept the native Win32 C/C++, RichEdit, tray, global hotkeys, and business logic.

### Implementation
*   Reworked the command deck into file, typography, and display groups.
*   Added owner-drawn native buttons with hover, pressed, disabled, and keyboard-focus states.
*   Added a separate title/count/progress row.
*   Added a centered, maximum-width reading canvas with additional text insets.
*   Added system light/dark/high-contrast themes and live response to system theme changes.

## 2026-10-22: Window Pinning and Responsive Reader Information

### Changes
*   Added owner-drawn pin buttons with independent persisted always-on-top state for reader and bookshelf.
*   Extended reader opacity choices down to 10%.
*   Split the reader toolbar into action and information rows with compact-window sizing.
*   Added live book title, non-whitespace character count, and scroll-based reading progress.
*   Removed encoding information from the reading status line.

## 2026-10-21: Bookshelf Catalog and Portable Library

### Summary
Expanded Vitrite into a persistent personal TXT library while keeping the reader and transparency utility lightweight.

### Changes
*   Added rounded corners through Windows 11 DWM attributes with a Windows 10 region fallback.
*   Added a card-based bookshelf with drag-and-drop and multi-file TXT import.
*   Added copy-on-import storage, editable authors, favorite stars, pinyin sorting, author filtering, and favorite-state filtering.
*   Stored book metadata inside the library in `library.ini`.
*   Added one-click portable export with the executable, library, authors, favorites, and configuration.
*   Added automatic portable-library discovery from a sibling `library` directory.

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
