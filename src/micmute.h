#ifndef MICMUTE_H
#define MICMUTE_H

#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00

#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <math.h>

#define APP_NAME        L"iAlturki-MicMute"
#define APP_VERSION     L"2.1.0"
#define APP_URL         L"https://github.com/iAlturki/MicMute"

// Window messages
#define WM_APP_TRAY     (WM_APP + 1)  // tray icon callback
#define WM_APP_AUDIO    (WM_APP + 2)  // external mute/volume change (wParam=mute, lParam=vol*10000)
#define WM_APP_DEVICE   (WM_APP + 3)  // default capture device changed

#define HOTKEY_ID 1

// Menu IDs (size/position/hotkey ranges are contiguous and decoded arithmetically)
#define ID_MENU_TOGGLE          1001
#define ID_MENU_ABOUT           1002
#define ID_MENU_EXIT            1003
#define ID_MENU_VOLUME_LOCK     1501
#define ID_MENU_MULTIMONITOR    1600
#define ID_MENU_DUCK_VOLUME     1601
#define ID_MENU_SOUND_FEEDBACK  1602
#define ID_MENU_STARTUP         1401

#define ID_MENU_SIZE_FIRST      1101  // + index into kOverlaySizes
#define ID_MENU_SIZE_LAST       1104
#define ID_MENU_POS_FIRST       1110  // + OverlayPosition
#define ID_MENU_POS_LAST        1118
#define ID_MENU_HOTKEY_F1       1301  // + (n-1) for Fn
#define ID_MENU_HOTKEY_F12      1312
#define ID_MENU_HOTKEY_CTRL_M   1313
#define ID_MENU_HOTKEY_CUSTOM   1314

typedef enum {
    POS_TOP_LEFT, POS_TOP_CENTER, POS_TOP_RIGHT,
    POS_CENTER_LEFT, POS_CENTER_CENTER, POS_CENTER_RIGHT,
    POS_BOTTOM_LEFT, POS_BOTTOM_CENTER, POS_BOTTOM_RIGHT,
    POS_COUNT
} OverlayPosition;

// VK of the active hotkey; CTRL_M and CUSTOM are sentinels
#define HOTKEY_CTRL_M 0x4D
#define HOTKEY_CUSTOM 0xFFFF

typedef struct {
    DWORD overlaySize;            // badge size in px (16/32/64/96)
    OverlayPosition overlayPosition;
    BOOL multiMonitorMode;
    BOOL audioFeedbackEnabled;
    BOOL reduceVolumeWhenMuted;
    DWORD currentHotkey;          // VK_F1..VK_F12, HOTKEY_CTRL_M or HOTKEY_CUSTOM
    UINT customHotkeyModifiers;
    UINT customHotkeyVK;
    BOOL startupEnabled;
    BOOL volumeLockEnabled;
    float lockedVolume;
    BOOL lastMuteState;
} AppSettings;

typedef struct {
    HWND hWnd;
    BOOL isMuted;
    AppSettings settings;
} AppState;

extern AppState g_appState;
extern const DWORD kOverlaySizes[4];   // 16, 32, 64, 96

// main.c
void SyncMuteUI(void);                 // tray + overlay + duck after isMuted changed

// render.c (GDI+ drawing, per-pixel alpha)
BOOL  Render_Init(void);
void  Render_Shutdown(void);
HBITMAP RenderBadge(int windowPx, int badgePx, BOOL muted, float bg);  // PARGB DIB; bg 1..0 melts the plate
HICON RenderTrayIcon(int px, BOOL muted, BOOL lightTaskbar);

// overlay.c
void OverlayShowMuted(BOOL animate);
void OverlayHide(void);                // morphs into a green flash, then fades
void OverlayRebuild(void);             // monitors/DPI/size/position changed
void OverlayCleanup(void);

// tray.c
BOOL TrayInit(HWND hwnd);
void TrayUpdate(void);                 // re-render icon + tooltip from current state
void TrayRefreshTheme(void);
void TrayShowMenu(HWND hwnd, POINT pt);
void TrayBalloon(const wchar_t* title, const wchar_t* text);
void TrayCleanup(void);
UINT TrayTaskbarCreatedMsg(void);

// audio.c
BOOL AudioInit(void);                  // FALSE only if COM enumerator fails
void AudioReinitDevice(void);          // default capture device changed
void AudioReinitSystemEndpoint(void);  // default render device changed
void SetMicMute(BOOL mute);            // app-initiated; updates UI + feedback
void ToggleMicrophone(void);
BOOL AudioGetMute(BOOL* muted);        // actual device state
void LockCurrentVolume(void);
void RestoreLockedVolume(void);
void DuckSystemVolume(BOOL duck);
void PlayFeedback(BOOL isMuting);
void AudioCleanup(void);

// settings.c
void LoadSettings(void);
void SaveSettings(void);
BOOL IsStartupEnabled(void);
void EnableStartup(BOOL enable);

// hotkey.c
void RegisterCurrentHotkey(void);
void UnregisterCurrentHotkey(void);
void ChangeHotkey(DWORD newHotkey);
void BeginHotkeyCapture(void);         // interactive "press a key" window
void FormatHotkeyName(wchar_t* buf, size_t cch, DWORD hotkey, UINT mods, UINT vk);

#endif
