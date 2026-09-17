# Mosaic

A native Win32 + Direct2D + DirectComposition dashboard shell. No Electron,
no browser runtime, no third-party UI framework — just the Windows graphics
stack, driven directly.

## to run this you must have the following tools
1. Visual Studio Build Tools 2022(**Desktop development with C++**)
2. CMake
3. Ninja
4. VS Code extensions: **CMake Tools** (`ms-vscode.cmake-tools`) and **C/C++** (`ms-vscode.cpptools`), both from Microsoft

## run
1. clone it
2. open the mosaic folder
3. press `Ctrl+Shift+P` → **CMake: Select a Kit** → choose
   **"Visual Studio Build Tools 2022 Release - amd64"**
4. `Ctrl+Shift+P` → **CMake: Configure**.
5. `Ctrl+Shift+P` → **CMake: Build**
6. The executable lands at `build/bin/Mosaic.exe`