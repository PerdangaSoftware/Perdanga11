<div align="center">

<img src="assets/logo/perdanga11.png" width="130" alt="Perdanga11 Logo"/>

<h1 align="center">Perdanga11</h1>

<p align="center">
  <b>Native C++ replacement for the Windows 11 Start Menu with custom tabs and multi file search.</b>
</p>

<br/>

<img src="https://gitlab.com/perdanga/perdanga11/-/raw/main/screenshots/Perdanga11menu.png?ref_type=heads" width="520" alt="Perdanga11 Interface Preview"/>

</div>

---

## Highlights & Features

- **Fast Start:** Written entirely in pure native Win32/C++17 with zero third-party dependencies. Idles at **< 20 MB RAM** with instant display response.
- **Deep Multi Search:** Background multi-threaded scans applications, Desktop items, user libraries, and all connected drives ($A:\dots Z:$) with specific support for documents and media (`.txt`, `.png`, `.jpg`, `.pdf`, `.docx`, etc.).
- **Custom Categorized Tabs:** Organize your workspace with customizable tabs (Pinned, Folders, Tools, Games, etc.). Drag-and-drop tiles to rearrange, rename tabs, or create new categories with a simple right-click.
- **Consecutive Multi-Launch:** Launch multiple programs in succession without the menu auto-closing simply by holding `Shift` while clicking (or via middle mouse click). Standard single-click launches and dismisses immediately.
- **Intelligent Path:** Hovering any tile displays a clean, floating info card showing the program's actual installation directory (automatically resolving `.lnk` shortcuts to the real `.exe` destination) and its full, un-truncated title.
- **Native Explorer Pinning & SendTo:** Pin files, folders, and executables directly from Windows Explorer or the Desktop.
- **Smooth Fluent Design:** Hardware-accelerated GDI+ and DWM backdrop rendering featuring Windows 11 dark/light mode detection.

---

## Technology Stack

- **Core Architecture:** C++17 (MSVC), Windows API (Win32, OLE2, Shell APIs, DWM).
- **Graphics & Rendering:** GDI+, DirectComposition, Fluent 2 optical typography hierarchy (`Display`, `Text`, `Small`).
- **System Hooks & Interception:** Low-Level Windows Hooks (`WH_KEYBOARD_LL`, `WH_MOUSE_LL`), UI Automation Core for Taskbar detection, full-screen foreground monitoring, and UIPI message filtering (`ChangeWindowMessageFilter`).
- **Persistence & Config:** Dual-mode storage architecture supporting both portable configuration and user-level `%APPDATA%\Perdanga11\config.ini` persistence (preserving UTF-16 LE BOM Cyrillic and international character encoding).
- **Packaging:** Inno Setup 6.

---

## 📂 Project Structure

```text
Perdanga11/
├── assets/                  # Graphical assets
│   ├── ico/
│   │   └── perdanga11.ico   # Application icon
│   └── logo/
│       └── perdanga11.png   # Fluent brand logo
│
├── src/                     # Core Application Source Code
│   ├── AppIndexer.hpp       # Asynchronous file indexer, icon cache & search scorer
│   ├── Config.hpp           # State management, INI serializer & localization
│   ├── Hooks.hpp            # Keyboard/mouse low-level hooks & UI Automation
│   ├── MenuInteraction.hpp  # Context menus, modal dialogs & system power actions
│   ├── MenuRenderer.hpp     # GDI+ double-buffered visual rendering pipeline
│   ├── MenuState.hpp        # Window state, debounced search worker & animation
│   ├── MenuWindow.hpp       # Main Win32 window message loop & event handler
│   ├── main.cpp             # Entry point, single-instance mutex & tray icon
│   ├── resource.h           # Resource ID definitions
│   └── resource.rc          # Windows resource compiler definition
│
├── installer/               # Distribution & Installer
│   ├── installer.cpp        # Lightweight standalone C++ setup utility
│   ├── installer.rc         # Embedded payload resources
│   └── setup.iss            # Inno Setup compilation script with custom theme
│
├── bin/                     # Output Directory for Binaries
│   ├── Perdanga11.exe       # Compiled executable
│   └── config.ini           # User layout and tab configuration
│
├── build/                   # Temporary build artifacts (.obj, .res)
├── dist/                    # Packaged installers (Perdanga11_Setup.exe)
├── build.bat                # Automated one-click compilation & packaging script
└── clean.bat                # Cleanup script for temporary build artifacts
```

---

## Build

### Prerequisites

1. **Windows 10 / 11 (64-bit)**
2. **Visual Studio 2022** (or MSVC Build Tools) with the **"Desktop development with C++"** workload.
3. **Inno Setup 6** (Optional, required only for compiling the setup installer wizard).

---

### Step-by-Step Compilation Guide

1. Open **x64 Native Tools Command Prompt for VS**.
2. Navigate to the project folder:
   ```cmd
   cd "C:\Path\To\Perdanga11"
   ```
3. Run the master build script:
   ```cmd
   build.bat
   ```

The script will automatically:
- Compile application resources (`resource.rc`).
- Compile and link `bin\Perdanga11.exe`.
- Prepare high-resolution installer artwork from `assets\logo\perdanga11.png`.
- Compile the setup installer (`dist\Perdanga11_Setup.exe`) if Inno Setup is present.
- Launch `Perdanga11.exe` immediately for testing.

---

### Cleaning Build Artifacts

To clean all intermediate `.obj` and `.res` files from the `build\` folder:

```cmd
clean.bat
```

---

<br>

<div align="center">
  <img src="https://gitlab.com/perdanga/perdanga11/-/raw/main/assets/ico/perdanga11.ico?ref_type=heads" width="80" alt="perdanga11.ico"/>
  <br><br>
  <h2>Perdanga Forever!</h2>
</div>
