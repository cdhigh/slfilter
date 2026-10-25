# Sprint-Layout 6.0 Macro Filter Plugin (`slfilter`)

A lightweight (~20 KB) plugin DLL for **Sprint-Layout 6.0** that enhances the built-in Macro Library panel (top-right TreeView) with **real-time filtering, instant preview, and smooth keyboard navigation**.

Zero binary modifications to `layout60.exe` — deployed simply by dropping `version.dll` into the application directory, and completely uninstalled by deleting it.

---

## Features

![Screenshot](https://raw.githubusercontent.com/cdhigh/slfilter/main/screenshot.png)

- **Zero-Intrusion Deployment**: Drop `version.dll` next to `layout60.exe` to install. Delete it to restore original behavior. No administrator rights or registry modifications required.
- **Multi-Language Support**: Automatically detects and binds to English (`Macros`), German (`Makros`), Russian (`Макросы`), and Chinese (`元件库`) editions of Sprint-Layout.
- **Accurate Filename Search**: Matches strictly against component `.LMK` filenames (directory names are excluded to prevent false positives). Case-insensitive and supports multiple keywords separated by spaces (logical AND).
- **Synchronized Real-Time Preview**:
  - The first matching component is automatically selected and immediately previewed in the bottom-right preview pane.
  - Single-clicking or navigating results via **↑ / ↓ / PageUp / PageDown** updates the preview in real-time without stealing focus from the list.
- **Direct Drag & Drop to PCB**: Click and drag any component directly from the filter results list onto the PCB layout board, seamlessly matching the native TreeView drag-and-drop behavior.
- **Locate and Place**: Pressing **Enter** or **double-clicking** an item closes the filter list, auto-expands ancestor directories, selects the item in the original TreeView, and hands focus back to the tree.
- **Smooth Keyboard Navigation**:
  - Filter box: Full **← / →** cursor movement, text editing, `Home` / `End`, `Ctrl+A`, and `Shift` selection.
  - Pressing **↓** moves focus directly into the results list.
  - Pressing **↑** at the top of the results list returns smoothly to the filter box.
  - Pressing **Esc** clears the search or returns focus to the tree.
- **Global Hotkey (`Ctrl + Alt + F`)**: Instantly focuses and selects the search box from anywhere within the application.
- **High Performance & Zero Flicker**: Pure event-driven resizing; eliminated periodic timer polls, ensuring the TreeView never flickers.

---

## Quick Start

1. Copy [`version.dll`](https://github.com/cdhigh/slfilter/releases) into your Sprint-Layout 6.0 folder (the same directory as `layout60.exe`).
2. Launch `layout60.exe`. The Macro Library title bar will now host the filter input box.
3. Type keywords to search. Use the arrow keys to browse and preview components, and press **Enter** to locate.

---

## Project Structure

| File | Description |
| :--- | :--- |
| `src/slfilter.c` | Core implementation in pure Win32 C |
| `src/tvdefs.h` | Cross-compiler Win32 macros and backwards compatibility layer |
| `src/version.def` | Clean module definition file for undecorated stdcall exports |
| `src/version.rc` | PE version information and metadata resource |
| `src/fix_exports.py` | Export table patch script for TinyCC builds |
| `build.cmd` | **Universal smart build script** (auto-detects available compilers) |
| `build_vs.cmd` | Visual Studio 2019 / 2022 (MSVC x86) one-click build script |
| `build_cmake.cmd` | CMake (Win32) one-click build script |
| `build_devcpp.cmd` | Dev-Cpp / MinGW 32-bit GCC one-click build script |
| `build_tcc.cmd` | TinyCC (32-bit) one-click build script |
| `build_vc6.cmd` | Visual C++ 6.0 one-click build script |
| `CMakeLists.txt` | Standard CMake configuration |

---

## Building from Source

> [!IMPORTANT]
> `layout60.exe` is a **32-bit (x86)** Windows executable. Any DLL compiled for this project **must target 32-bit x86**. A 64-bit DLL will fail to load with error code `0xc000007b`.

### 1. Smart Universal Build (Recommended)
Simply run `build.cmd`. It automatically detects the best compiler installed on your machine:
```cmd
build.cmd
```
You can also explicitly specify a target toolchain:
```cmd
build.cmd vs        # Visual Studio 2019 / 2022 (MSVC x86)
build.cmd cmake     # CMake (Win32)
build.cmd devcpp    # Dev-Cpp / MinGW 32-bit GCC
build.cmd tcc       # TinyCC (32-bit)
build.cmd vc6       # Visual C++ 6.0
```

### 2. Visual Studio 2019 / 2022 (MSVC)
Double-click or run:
```cmd
build_vs.cmd
```
*(Automatically detects Visual Studio via `vswhere.exe` and sets up the 32-bit environment; no manual opening of Native Tools Command Prompt required).*

### 3. CMake
```cmd
build_cmake.cmd
```
*Or manually:*
```cmd
cmake -B build -A Win32
cmake --build build --config Release
```

### 4. Dev-Cpp / MinGW (32-bit GCC)
```cmd
build_devcpp.cmd
```

### 5. TinyCC (TCC)
```cmd
build_tcc.cmd
```

### 6. Visual C++ 6.0 (VC6)
```cmd
build_vc6.cmd
```

---

## How It Works (Technical Overview)

1. **Proxy DLL Hijacking**: `layout60.exe` imports version inspection functions from `version.dll`. Because `version.dll` is not in Windows' `KnownDLLs` list, Windows checks the application directory before `C:\Windows\System32`. `slfilter` intercepts these calls, forwards them transparently to the real `version.dll`, and initializes the plugin on the main UI thread.
2. **Title Bar Container Detection**: A 4-level matching algorithm identifies the Macro panel container across localized versions (English, German, Russian, Chinese) while strictly validating window bounds.
3. **Delphi VCL Key Hook**: Delphi forms default to consuming arrow keys in `TCustomForm.DialogKey` to switch controls. A thread-level `WH_GETMESSAGE` hook intercepts arrow keys, `Enter`, and `Esc`, dispatching them directly to the filter controls and preventing focus theft.
4. **Synchronized Tree Events**: Selecting an item in the filter list sends `TVM_SELECTITEM` with `TVGN_CARET` to the background TreeView, triggering Delphi's native `TV_MakrosChange` handler to refresh the preview canvas seamlessly.

---

## Debugging

To view diagnostic logs during development, set the environment variable:
```cmd
set SLFILTER_LOG=1
layout60.exe
```
Log output is written to `%TEMP%\slfilter.log`.

---

## License

This project is released under the [MIT License](LICENSE).
