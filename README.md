# Vitrite (Modernized)

![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)
![License](https://img.shields.io/badge/license-GPL-green)
![Build](https://img.shields.io/badge/build-VS2022-purple)
![Version](https://img.shields.io/badge/version-0.2.0-blue)

**Vitrite** is a lightweight Windows utility with a built-in, automatically wrapping TXT reader. It can also adjust the transparency of almost any visible window using global hotkeys and toggle "Always on Top" for any window.

> [!NOTE]
> **Revival Project**: This is a modernized fork of the original Vitrite (circa 2002) by Ryan VanMiddlesworth. It has been updated to run natively on Windows 10 and 11 with modern safety and stability improvements.

[中文文档 (Chinese Documentation)](README.zh-CN.md)

[Download the latest version](https://github.com/huaaorain/Vitrite/releases/latest)

## ✨ Modernization Features

This version has been rewritten to meet modern Windows standards:

*   **🛡️ Safer Implementation**: Replaced legacy DLL injection and global keyboard hooks with `RegisterHotKey`. This is much more stable and won't trigger anti-virus software.
*   **📖 Wrapping TXT Reader**: Long lines reflow to the current window width, with adjustable font size, opacity, encoding detection, and single-instance file opening.
*   **📚 Portable Bookshelf Catalog**: Drag or import TXT files, edit authors, switch the library folder, and persist metadata inside the library.
*   **⭐ Favorites**: Mark any book with the card's star and filter all, favorite-only, or non-favorite books.
*   **🔎 Pinyin and Author Views**: Sort titles by Chinese pinyin or group/filter the catalog by author.
*   **📦 One-click Portable Library**: Export the executable, books, authors, favorites, and configuration into a ZIP that runs immediately after extraction.
*   **🪟 Rounded UI**: Native Windows 11 corners with a Windows 10 fallback.
*   **👁️ High-DPI Ready**: Full Per-Monitor V2 DPI awareness. The UI looks crisp on 4K screens and mixed-DPI multi-monitor setups.
*   **🛡️ Crash Recovery**: Automatically restores the tray icon if Windows Explorer crashes or restarts.
*   **🌓 Dark Mode Support**: The "About" dialog attempts to respect the system's dark mode preference (Windows 10/11).
*   **🌐 Internationalization**: Built-in English, Simplified Chinese, and Japanese resources, automatically selected based on system UI language.
*   **🛠️ 64-bit Support**: Includes Release configurations for both x86 and x64 architectures.

## ⌨️ Hotkeys

| Hotkey | Function |
| :--- | :--- |
| **Ctrl + Shift + 1..9** | Set transparency level (1 = 10%, 9 = 90%, 0 = Reset). |
| **Ctrl + Shift + 0** | Reset window to purely opaque (removes transparency effects).* |
| **Ctrl + Shift + +** | Toggle **Always On Top** (using the main keyboard `+` key). |
| **Ctrl + Shift + Num+** | Toggle **Always On Top** (using the Numpad `+` key). |

*\*Note: For safety, the reset function only affects windows modified by Vitrite during the current session.*

In the reader, use **Ctrl+B** to open the bookshelf catalog.

## 🏗️ Building

**Requirements**: Visual Studio 2022 (Community or newer) with C++ Desktop Development workload.

1.  Open `src\Vitrite.sln`.
2.  Select your target architecture (`x64` recommended).
3.  Build the solution.

The executable will be output to the `x64\Release` (or `Release`) directory.

## 📜 Credits

*   **Original Author**: Ryan VanMiddlesworth (2002)
*   **License**: GNU General Public License (GPL)
