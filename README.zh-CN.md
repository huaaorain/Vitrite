# Vitrite (现代化重制版)

![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)
![License](https://img.shields.io/badge/license-GPL-green)
![Build](https://img.shields.io/badge/build-VS2022-purple)
![Version](https://img.shields.io/badge/version-0.1.0-blue)

**Vitrite** 是一个小巧的 Windows 工具，内置自动换行 TXT 阅读器，也可以通过全局快捷键手动调整任意窗口的透明度，并支持将窗口设置为“总在最前”（置顶）。

> [!NOTE]
> **重制项目说明**: 本项目是 Ryan VanMiddlesworth 原作（约 2002 年）的现代化 Fork 版本。针对 Windows 10 和 11 进行了原生重写，提升了安全性和稳定性。

[English Documentation (英文文档)](README.md)

[下载 0.1 版本](https://github.com/hua-ao-yu/Vitrite/releases/latest)

## ✨ 现代化特性

此版本已根据现代 Windows 标准进行了全面重写：

*   **📖 内置 TXT 阅读器**: 直接打开小说文本，窗口变窄或变宽时会自动重新折行，不会再把长行截断在右侧。
*   **🔤 中文编码兼容**: 自动识别 UTF-8、UTF-8 BOM、UTF-16 LE/BE；无 BOM 的旧文本会按系统编码读取。
*   **🪟 阅读体验**: 支持字号调整、奶油色阅读背景、窗口透明度选择、滚轮阅读以及复制文本。
*   **🔗 单实例打开**: 程序已运行时，再双击另一个 TXT 会在现有阅读器中直接打开。
*   **🛡️ 更安全的实现**: 移除了传统的 DLL 注入和全局键盘钩子，改用标准的 `RegisterHotKey`。这不仅更稳定，而且不会触发反病毒软件的误报。
*   **👁️ 高 DPI 支持**: 完整的 Per-Monitor V2 DPI 感知。在 4K 屏幕和多显示器混排环境下，UI 显示依然清晰锐利。
*   **🛡️ 崩溃恢复**: 若 Windows 资源管理器崩溃或重启，托盘图标会自动恢复。
*   **🌓 深色模式支持**: “关于”对话框会尝试适配系统的深色模式（Windows 10/11）。
*   **🌐 国际化支持**: 内置英文、简体中文和日语资源，根据系统 UI 语言自动切换。
*   **🛠️ 64 位支持**: 包含针对 x86 和 x64 架构的 Release 构建配置。

## ⌨️ 快捷键

### 阅读器

| 快捷键 | 功能 |
| :--- | :--- |
| **Ctrl + O** | 打开 TXT 文件 |
| **Ctrl + 加号** | 增大字体 |
| **Ctrl + 减号** | 减小字体 |
| **Ctrl + 鼠标滚轮** | 缩放字体 |
| **Ctrl + Shift + 1..9 / 0** | 调整阅读器透明度 / 恢复不透明 |

### 任意窗口透明度

| 快捷键 | 功能 |
| :--- | :--- |
| **Ctrl + Shift + 1..9** | 设置透明度级别 (1 = 10%, 9 = 90%, 0 = 重置) |
| **Ctrl + Shift + 0** | 将窗口重置为完全不透明 (移除透明效果)* |
| **Ctrl + Shift + +** | 切换 **总在最前** (使用主键盘区的 `+` 键) |
| **Ctrl + Shift + Num+** | 切换 **总在最前** (使用小键盘区的 `+` 键) |

*\*注意：为了安全起见，重置功能仅对本次运行中被 Vitrite 修改过的窗口生效。*

## 📖 使用方法

1.  直接运行 `Vitrite.exe`，或双击通知区域的 Vitrite 图标打开阅读器。
2.  点击“打开 TXT...”选择小说文件；也可以把 TXT 路径作为启动参数，或右键 TXT 选择“打开方式”指定 `Vitrite.exe`。
3.  拖动窗口边缘即可改变宽度。超长中文句子、英文长单词都会在可用宽度内自动换行。
4.  关闭阅读器不会退出后台透明度工具，可从通知区域再次打开。
5.  阅读器工具栏和“视图”菜单都可以调整字体与不透明度；开机自启时程序保持静默，只显示通知区域图标。

## 🏗️ 构建指南

**环境要求**: Visual Studio 2022 (Community 或更高版本)，需安装 C++ 桌面开发工作负载。

1.  打开 `src\Vitrite.sln`。
2.  选择目标架构（推荐 `x64`）。
3.  生成解决方案。

可执行文件将输出到 `x64\Release` (或 `Release`) 目录。

## 📜 致谢

*   **原作者**: Ryan VanMiddlesworth (2002)
*   **许可证**: GNU General Public License (GPL)
