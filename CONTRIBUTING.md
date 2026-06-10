# Contributing to iAlturki-MicMute

## Development Setup

1. Fork the repository
2. Make your changes in the `src/` directory

## Building

GitHub Actions automatically builds releases. For local development you need Visual Studio 2022. From a **VS2022 Developer Command Prompt** in the repository root:

```bat
mkdir build
rc /fo build\resources.res src\resources.rc
cl /O2 /W3 /Fe:build\iAlturki-MicMute.exe src\*.c build\resources.res user32.lib shell32.lib ole32.lib winmm.lib gdi32.lib advapi32.lib gdiplus.lib shcore.lib /link /SUBSYSTEM:WINDOWS
```

This produces a single `build\iAlturki-MicMute.exe` with no external dependencies.

## Architecture

The project is plain C (Win32) with a simple module-per-concern layout in `src/`:

- `main.c` – entry point + message routing
- `audio.c` – event-driven Core Audio engine
- `overlay.c` – layered-window overlay + animation
- `render.c` – GDI+ drawing for the overlay badge + tray icons
- `tray.c` – tray icon + menu
- `settings.c` – registry persistence
- `hotkey.c` – hotkey registration + capture UI
- `micmute.h` – shared header

## Code Style

- Follow existing C coding conventions
- Use consistent indentation
- Add comments for complex functions
- Test on supported Windows versions

## Pull Requests

1. Create a feature branch
2. Make your changes
3. Test thoroughly
4. Commit your changes
5. Push to your fork
6. Open a Pull Request

## Issues

Report bugs or request features via GitHub Issues.
