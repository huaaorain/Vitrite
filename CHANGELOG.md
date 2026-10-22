# Changelog

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
