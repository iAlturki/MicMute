<!-- AI setup by iAlturki -->
<p align="center">
  <img src="assets/logo.svg" width="76" alt="MicMute logo">
  &nbsp;&nbsp;
  <a href="https://ialturki.github.io/MicMute/agent-setup/"><img src="assets/ai-setup.svg" width="330" alt="Set up with your AI agent — open the setup prompt copier"></a>
</p>
<p align="center"><sub>Click the setup button, copy the prompt, and give it to your AI agent. <a href="docs/agent-setup/prompt.txt">Read the prompt</a>.</sub></p>
<!-- /AI setup by iAlturki -->

# iAlturki-MicMute

🎙️ A tiny, dependency-free microphone mute utility for Windows with a beautiful on-screen overlay and an event-driven audio engine.

[![Download Latest Release](https://img.shields.io/github/v/release/iAlturki/MicMute?label=Download&style=for-the-badge&color=brightgreen)](https://github.com/iAlturki/MicMute/releases/latest/download/iAlturki-MicMute.exe)
[![Downloads](https://img.shields.io/github/downloads/iAlturki/MicMute/total?label=release%20downloads&style=for-the-badge&color=555)](https://github.com/iAlturki/MicMute/releases)

<img src="assets/overlay-demo.gif" width="224" alt="Mute, background melt, and unmute flash demo">

| Muted | After 3 s | Unmute flash |
|:-:|:-:|:-:|
| <img src="assets/overlay-muted.png" width="112"> | <img src="assets/overlay-melted.png" width="112"> | <img src="assets/overlay-live.png" width="112"> |

## What's new in 2.0

- Redesigned overlay: rounded badge, soft shadow, smooth animations, background melt + auto-dim
- Tray icons drawn at runtime — red badge when muted, theme-aware glyph with green dot when live
- Instant sync with mute changes from other apps (no polling) and mic hot-swap support
- Working custom hotkeys, synthesized audio cues, per-monitor DPI awareness

## Features

- **Visual overlay** – Per-pixel-alpha layered window rendered with GDI+: a rounded dark badge with a soft drop shadow and an antialiased mic glyph (red slash when muted). Fades and slides in on mute; after 3 seconds the badge background melts away leaving just the floating glyph, which settles to 70% opacity at the 6-second mark so it stays out of your way. Flashes a green "live" indicator that fades out on unmute. 9 screen positions, 4 sizes (16/32/64/96 px), per-monitor DPI-aware, with full multi-monitor and monitor hot-plug support.
- **Event-driven engine** – No polling. Reacts instantly when mute is changed by other apps (Teams, a keyboard mic key, Windows Settings), so the overlay and tray icon are always in sync.
- **Mic hot-swap support** – If the default microphone changes (unplug/replug, new device), the mute state carries over to the new device — privacy first.
- **Hotkey toggle** – F8 by default; choose F1–F12, Ctrl+M, or capture any custom key combination.
- **Runtime-rendered tray icon** – Red rounded badge with a severed mic when muted; theme-aware mic glyph (light/dark taskbar) with a green status dot when live. Tooltip shows the current state and hotkey. Double-click toggles, right-click opens the menu, and the icon survives Explorer restarts.
- **Audio feedback** – Pleasant synthesized two-tone cues: descending when muted, ascending when live.
- **Volume controls** – Optional mic volume lock (re-asserts a locked level) and optional system-volume ducking while muted.
- **Settings that stick** – Persisted in the registry (`HKCU\SOFTWARE\iAlturki\MicMute`) and validated on load; v1 settings carry over. Start-with-Windows toggle. Single instance — launching a second copy pings the running one.

## How it works

Plain C on Win32, about 1,800 lines in `src/`.

- **Event-driven Core Audio, with COM written by hand in C.** `IAudioEndpointVolumeCallback` and `IMMNotificationClient` are implemented with hand-built vtables. Every write MicMute makes carries a private event-context GUID, and the callback drops those events before anything crosses threads, so MicMute never reacts to its own changes ([`audio.c`, lines 37 to 47](https://github.com/iAlturki/MicMute/blob/9a054c4e7bc8413cf76cac2093fd71ef5e03c3bc/src/audio.c#L37-L47)).
- **One thread owns the state.** Callbacks arrive on Core Audio worker threads and only `PostMessage` to the UI thread, which re-reads the real mute state before changing anything ([`main.c`, lines 208 to 219](https://github.com/iAlturki/MicMute/blob/9a054c4e7bc8413cf76cac2093fd71ef5e03c3bc/src/main.c#L208-L219)). No locks.
- **The overlay.** One click-through, topmost, no-activate layered window per monitor. GDI+'s flat C API is declared by hand ([`render.c`](https://github.com/iAlturki/MicMute/blob/9a054c4e7bc8413cf76cac2093fd71ef5e03c3bc/src/render.c#L18-L40)) and draws straight into the DIB that `UpdateLayeredWindow` presents. The slash is cut into the glyph with `SourceCopy` compositing, so the mic still looks severed after the plate melts away.
- **Zero CPU at rest.** The animation timer runs only during transitions and is killed when the badge settles ([`overlay.c`, line 252](https://github.com/iAlturki/MicMute/blob/9a054c4e7bc8413cf76cac2093fd71ef5e03c3bc/src/overlay.c#L252)).
- **The cues are synthesized:** a hand-written 44-byte WAV header plus 200 ms of 16-bit PCM, played from memory ([`audio.c`, lines 205 to 237](https://github.com/iAlturki/MicMute/blob/9a054c4e7bc8413cf76cac2093fd71ef5e03c3bc/src/audio.c#L205-L237)).
- **Running elevated has consequences, and each is handled:** exactly two tray messages are let through UIPI ([`tray.c`, lines 42 to 43](https://github.com/iAlturki/MicMute/blob/9a054c4e7bc8413cf76cac2093fd71ef5e03c3bc/src/tray.c#L42-L43)), autostart uses a Task Scheduler task, and the tray icon retries at logon until Explorer is up and comes back after Explorer restarts.

Write-ups with interactive demos: [444005129.xyz](https://444005129.xyz/#micmute).

## Installation

1. Download `iAlturki-MicMute.exe` from [Releases](../../releases)
2. Run it
3. Press F8 to mute/unmute your microphone

## Hotkey

The default hotkey is **F8**. To change it, right-click the tray icon and pick a hotkey from the menu — F1–F12, Ctrl+M, or **Set Custom…**, which opens a capture dialog where you simply press the combination you want.

## System Requirements

- Windows 10/11
- One 159 KB executable: no installer, no third-party libraries, no VC++ redistributable (static CRT; links only Windows system DLLs)
- Minimal CPU/RAM usage
- Runs elevated (one UAC prompt at launch) so the hotkey keeps working while
  admin apps or fullscreen games have focus; "Start with Windows" uses a
  Task Scheduler task with highest privileges

## Build from Source

Requires Visual Studio 2022. From a **VS2022 Developer Command Prompt** in the repository root:

```bat
mkdir build
rc /fo build\resources.res src\resources.rc
cl /O2 /W3 /Fe:build\iAlturki-MicMute.exe src\*.c build\resources.res user32.lib shell32.lib ole32.lib winmm.lib gdi32.lib advapi32.lib gdiplus.lib shcore.lib /link /SUBSYSTEM:WINDOWS /MANIFEST:NO
```

The result is a single `build\iAlturki-MicMute.exe`. Releases are also built automatically by GitHub Actions.

## License

CC BY-NC-SA 4.0 - see [LICENSE](LICENSE) file.


Current original material is **CC BY-NC-SA 4.0 © iAlturki**. Earlier MIT releases retain their original terms; third-party material retains its own notices. See [NOTICE](NOTICE) and [LICENSES/previous-MIT.txt](LICENSES/previous-MIT.txt).
