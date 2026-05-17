# Hermes Tray Manager

A lightweight, single-file Windows system tray application for managing the [Hermes Agent](https://github.com/NousResearch/Hermes) Gateway.


## Requirements

- Windows OS
- [LLVM / Clang](https://releases.llvm.org/) toolchain (`clang++` and `llvm-rc` must be in your PATH)
- Windows Native Hermes [installation](https://hermes-agent.nousresearch.com/docs/user-guide/windows-native)

## Building from Source

The project consists of a single C++ source file (`main.cpp`) and a resource file (`resource.rc`) that embeds the tray icon.

   ```cmd
   llvm-rc resource.rc
   clang++ -std=c++20 main.cpp resource.res -o hermes-tray.exe "-Wl,/subsystem:windows"
   ```

## Usage

Simply double-click the generated `hermes-tray.exe`. It will run silently in the background and place a Hermes icon in your system tray (bottom right of the taskbar).

### Environment Variables

The application relies on the following environment variable to locate your Hermes environment:

- `HERMES_HOME`: The root directory of your Hermes installation. If not provided, it defaults to `%USERPROFILE%\AppData\Local\hermes`.

