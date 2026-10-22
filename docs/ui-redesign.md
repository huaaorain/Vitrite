# Vitrite Windows Reader UI Redesign

## UI Decision Brief

### Track

**System-like native Win32.** Vitrite remains a single native C/C++ executable using
RichEdit, the system tray, global hotkeys, and the existing book/library logic. This
redesign uses Win32 custom painting and native child controls; it does not introduce
WinUI, WebView, Electron, or a new application framework.

### Product Archetype

The reader is a **document/knowledge utility with a lightweight command deck**.
Its primary surface is text. Controls support the reading task and should recede
when the user is not interacting with them.

The bookshelf remains a library/collection surface. The visual language introduced
here should later be reusable for the bookshelf without changing its workflows.

### Primary User Job

Open a TXT book, read for long periods, know where they are in the book, and adjust
typography or window presentation without losing the text surface. Secondary jobs
are opening the bookshelf, changing opacity, and pinning the window above other apps.

### Information Hierarchy

1. Reading content.
2. Book title.
3. Character count and reading progress.
4. Open/library actions.
5. Typography, opacity, and pin controls.

Encoding information is intentionally removed. It is diagnostic rather than useful
during reading.

### Window Model

- One reader window, minimum width 560 epx and minimum height 420 epx.
- Fixed two-row header:
  - Command row: file actions, typography group, display utilities.
  - Context row: book title, character count, reading progress.
- Reading canvas below the header.
- Wide windows cap the reading column at 1040 epx and center it; the surrounding
  window background becomes intentional whitespace.
- Compact mode below 760 epx reduces control widths and gaps without hiding book
  name, count, progress, or the pin state.
- Closing the reader keeps the tray utility alive.

### Command Model

- Primary group: `打开 TXT`, `书架`.
- Typography group: `A−`, current size label, `A+`.
- Display group: opacity selector and pin button.
- Menu commands remain available for keyboard and discovery:
  - `Ctrl+O`: open TXT.
  - `Ctrl+B`: open bookshelf.
  - `Ctrl++ / Ctrl+-`: font size.
  - `Ctrl+Shift+1..9 / 0`: window opacity via existing global shortcuts.

### Interaction States

- Default: tonal control background with a quiet outline.
- Hover: one-step tonal lift; no animation.
- Pressed: stronger tonal fill and inset appearance.
- Keyboard focus: 2 epx system-highlight outline plus focus cue.
- Pinned: filled accent pin glyph and accent outline.
- Unpinned: neutral outline pin glyph.
- Read-only document: text selection remains native RichEdit behavior.
- High contrast: system window, text, button, highlight, and highlight-text colors
  replace the branded palette.

### Accessibility and Platform Evidence

- Preserve native keyboard focus and tab order.
- Buttons keep accessible window text even when custom painted.
- The pin is not color-only: its silhouette and pressed state both change.
- Progress is reported in text (`进度：46%`), not only as a visual bar.
- Respect Windows reduced visual effects by using no animation.
- Follow `WM_SETTINGCHANGE`, `WM_THEMECHANGED`, and `WM_SYSCOLORCHANGE`.

### Anti-Default Position

This is not a generic dashboard or a stack of cards. It is a quiet reading desk:
a narrow command deck, an editorial title rail, and a paper-like text canvas.

## Visual Specification

### Layout Grid

- Base unit: 4 epx; preferred gap: 8 epx.
- Outer horizontal padding: 16 epx regular, 12 epx compact.
- Header height: 108 epx regular, 104 epx compact.
- Command row height: 36 epx.
- Context row height: 38 epx.
- Reading surface inset: 20 epx horizontal, 16 epx vertical.
- Maximum reading column: 1040 epx.

### Layout Sketch

```text
┌ Vitrite TXT Reader ──────────────────────────────────────────────┐
│ [打开 TXT] [书架]  │  [ A− ][ 16 pt ][ A+ ] │  [不透明度 ▼] [pin] │
│ 余烬归队                                      字数 16885  进度 37% │
├──────────────────────────────────────────────────────────────────┤
│               ┌──────── reading paper ────────┐                   │
│               │ comfortable inset + text      │                   │
│               │                               │                   │
│               └───────────────────────────────┘                   │
└──────────────────────────────────────────────────────────────────┘
```

### Typography

- UI family: `Segoe UI Variable Text` where available; fallback to
  `Microsoft YaHei UI` for Simplified Chinese and Windows 10.
- Document title: 15 pt, weight 600, single line with end ellipsis.
- UI controls and metadata: 10 pt, weight 400.
- Reading default: 16 pt, weight 400, line spacing controlled by RichEdit.
- Minimum reading size: 9 pt; maximum: 42 pt.

### Light Theme Tokens

| Token | Value |
| :--- | :--- |
| Window background | `#F4F1EB` |
| Header | `#FBF9F4` |
| Reading surface | `#FFFDF8` |
| Primary text | `#292722` |
| Secondary text | `#6C665D` |
| Accent | `#7358B9` |
| Accent text | `#FFFFFF` |
| Button | `#F1ECE3` |
| Button hover | `#E8E0D4` |
| Border | `#D8D0C3` |
| Separator | `#DED7CB` |

### Dark Theme Tokens

| Token | Value |
| :--- | :--- |
| Window background | `#1D1C1A` |
| Header | `#24231F` |
| Reading surface | `#181715` |
| Primary text | `#F0ECE5` |
| Secondary text | `#AAA39A` |
| Accent | `#B89EFF` |
| Accent text | `#21162F` |
| Button | `#302E2A` |
| Button hover | `#3B3832` |
| Border | `#4B4740` |
| Separator | `#413E38` |

### High Contrast

When Windows High Contrast is active:

- Window: `COLOR_WINDOW`.
- Header/buttons: `COLOR_BTNFACE`.
- Text: `COLOR_WINDOWTEXT`.
- Accent/pressed/focus: `COLOR_HIGHLIGHT`.
- Accent text: `COLOR_HIGHLIGHTTEXT`.
- Borders: `COLOR_WINDOWTEXT`.
- Disable decorative tonal fills.

### Component Contract

- Buttons are 32–36 epx high and use 7 epx corner radius.
- File-action group and typography group are separated by a 1 epx vertical rule.
- Typography is a true segmented control: decrement, current size, increment.
- Opacity selector stays visible at every supported width.
- Pin control is 34 epx square and sits at the right edge of the command row.
- Book title, count, and progress occupy one context row; only the title may
  ellipsize.
- The reading canvas never sits under the header controls and always retains
  internal text padding.

### Acceptance Checks

- 560, 900, and 1440 epx window widths have no overlapping controls.
- Long titles ellipsize without moving count or progress.
- Pin toggles `WS_EX_TOPMOST` independently for reader and bookshelf.
- Minimum opacity is 10%.
- Progress updates after scrolling, resizing, and font changes.
- Light, dark, and high-contrast states retain readable text and visible focus.

### Implementation Evidence

- Reader regression test verifies resizing at 1200 and 560 epx, plus a 900 epx
  visual-review capture.
- Button hover and keyboard focus are asserted in the automated test.
- Light, dark, and high-contrast theme variants are rendered separately without
  changing the user's Windows settings.
- The reading canvas uses a 1040 epx maximum column and 20 epx horizontal plus
  16 epx vertical text insets.
