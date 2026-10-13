# MicMute icon source

Project: **iAlturki**.

The README and setup-page icons reuse the app's own repository assets.
No replacement design, extra icon background, or new color palette is used.

- Source: `src/render.c: RenderTrayIcon, DrawMic, DrawSlash`.
- Source SHA-256: `08787aa39e54cfead8eab2bd4dc247d2a781f21bb68ba01a5ef8d3dea3a3d0de`.
- Export: actual GDI+ renderer; live/dark, live/light, and muted variants at 256 × 256.

`logo.png` is the original live icon for a dark taskbar; `logo-light.png`
is the original light-taskbar version. The page and README select the matching
theme. `logo-muted.png` is the original muted state. All three are exported by
the actual `RenderTrayIcon` GDI+ code, without launching MicMute or changing
microphone state, tray entries, or settings. The source file is unchanged.
The older `Mic_Muted_icon.ico` remains untouched.

Original iAlturki material in this revision: CC BY-NC-SA 4.0; see the repository license and NOTICE. Earlier releases retain their original terms.
