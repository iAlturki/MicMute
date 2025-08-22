# iAlturki-MicMute

🎙️ Simple microphone mute utility for Windows with visual overlay and hotkey support.

## Features

- **Hotkey toggle** - F8 (default) or Ctrl+M to mute/unmute instantly
- **Visual overlay** - Shows mute status on screen (9 positions, 5 sizes)
- **System tray** - Right-click for settings, double-click to toggle
- **Audio feedback** - Windows notification sounds
- **Multi-monitor** - Works across all displays
- **Startup support** - Auto-start with Windows

## Installation

1. Download `iAlturki-MicMute.exe` from [Releases](../../releases)
2. Download `Mic_Muted_icon.ico` and place in same folder
3. Run the executable
4. Press F8 to mute/unmute your microphone

## System Requirements

- Windows 10/11
- ~125KB disk space
- Minimal CPU/RAM usage

## Build from Source

Requires Visual Studio 2022:

```bash
git clone https://github.com/iAlturki/MicMute.git
cd MicMute
# GitHub Actions will build automatically on releases
```

## License

MIT License - see [LICENSE](LICENSE) file.
