# Changelog

## 0.4.0 - 2026-10-22

### Fixed

- Fixed `Ctrl+Shift+9` applying 100% instead of the documented 90% opacity.
- Added `Ctrl+mouse wheel` opacity adjustment in 10% steps.
- Added a nearly invisible mouse-hit layer for transparent reader backgrounds so clicks, text selection, context menus, and wheel input no longer fall through to the window behind.
- Forwarded transparent-surface wheel input to RichEdit and increased reader wheel scrolling to three lines per notch.
- Preserved transparent color-key state while global opacity animation is running.

## 0.3.1 - 2026-10-22

### Fixed

- Synchronized the reader opacity selector with global shortcut and animation changes.
- Removed the purple action accent and unified normal reader, bookshelf, and settings surfaces around white.
- Added white and transparent reader background modes.
- Added automatic black/white reading text selection based on the desktop background luminance in transparent mode.
- Preserved color-key transparency while the shared opacity animation is running.
- Removed the reader's sunken client border and suppressed the Windows 11 window border where supported.

## 0.3.0 - 2026-10-22

### Changed

- Redesigned the native Win32 reader command deck with grouped file, typography, and display controls.
- Added custom hover, pressed, disabled, and keyboard-focus drawing while retaining native button accessibility.
- Split the typography control into decrement, current size, and increment segments.
- Added a dedicated editorial title row with character count and live reading progress.
- Centered the reading canvas and capped its width, with intentional outer and inner whitespace.
- Added automatic light, dark, and Windows high-contrast theme handling.

## 0.2.1 - 2026-10-22

### Fixed

- Added pin buttons for independently toggling always-on-top behavior in the reader and bookshelf.
- Lowered the reader opacity minimum from 40% to 10%.
- Reworked the reader toolbar into a responsive two-row layout.
- Replaced the encoding status with persistent book title, character count, and reading-progress indicators.

## 0.2.0 - 2026-10-21

### Added

- Rounded Windows UI corners for the reader, bookshelf, settings, and book-info dialogs.
- Bookshelf with configurable storage folder, drag-and-drop import, multi-file import, author editing, and persistent metadata.
- Pinyin title sorting and author-based catalog filtering.
- Per-book favorite stars and all/favorites/non-favorites filters.
- Portable library export containing the executable, all books, authors, favorites, and configuration.
- Automatic portable-library discovery from the `library` folder next to `Vitrite.exe`.

## 0.1.0 - 2026-10-21

First public release of the TXT-reader edition.

### Added

- RichEdit-based TXT reader with automatic wrapping when the window is resized.
- UTF-8, UTF-8 BOM, UTF-16 LE/BE, and system-codepage text loading.
- Adjustable font size, window opacity, and persistent reader settings.
- Single-instance file handoff when opening another TXT file.
- Tray access, command-line file opening, and silent Windows startup mode.

### Preserved

- Per-window opacity hotkeys.
- Always-on-top hotkeys.
- High-DPI and tray recovery support.
