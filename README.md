# iAlturki-MicMute

🎙️ Simple microphone mute utility for Windows with visual overlay and hotkey support.

[![Download Latest Release](https://img.shields.io/github/v/release/iAlturki/MicMute?label=Download&style=for-the-badge&color=brightgreen)](https://github.com/iAlturki/MicMute/releases/latest/download/iAlturki-MicMute.exe)

## Features

- **Hotkey toggle** – F8 (default) or Ctrl+M to mute/unmute instantly
- **Visual overlay** – Shows mute status on screen (9 positions, 5 sizes)  
  ![Mic muted overlay](https://github.com/user-attachments/assets/aebf26d1-4704-4326-8dff-d91f102d7f8b)

- **System tray** – Right-click for settings, double-click to toggle  
  ![System tray demo](https://github.com/user-attachments/assets/adcf059f-1e02-4cf3-af1d-1fc4e3204246)
  </br>Blue info icon means Mic is UNmuted, Red x icon means Mic is Muted.
- **Audio feedback** – Windows notification sounds
- **Multi-monitor** – Works across all displays
- **Startup support** – Auto-start with Windows


## Installation

1. Download `iAlturki-MicMute.exe` from [Releases](../../releases)
2. Run it  
3. Press F8 to mute/unmute your microphone


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
