#ifndef MICMUTE_H
#define MICMUTE_H

#include <windows.h>
#include <shellapi.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <math.h>

// Constants
#define WM_TRAYICON (WM_USER + 1)
#define WM_VOLUME_CHANGED (WM_USER + 2)
#define HOTKEY_ID 1
#define OVERLAY_TIMER_ID 1
#define FADE_TIMER_ID 2
#define VOLUME_MONITOR_TIMER_ID 3

// Menu IDs
#define ID_MENU_TOGGLE 1001
#define ID_MENU_ABOUT 1002
#define ID_MENU_EXIT 1003
#define ID_MENU_OVERLAY_SETTINGS 1100
#define ID_MENU_AUDIO_SETTINGS 1200
#define ID_MENU_HOTKEY_SETTINGS 1300
#define ID_MENU_STARTUP_SETTINGS 1400
#define ID_MENU_VOLUME_LOCK 1500

// Multi-monitor and new settings
#define ID_MENU_MULTIMONITOR_TOGGLE 1600
#define ID_MENU_REDUCE_VOLUME_TOGGLE 1601
#define ID_MENU_AUDIO_FEEDBACK_TOGGLE 1602
#define ID_MENU_FEEDBACK_LEVEL 1610

// Overlay size submenu
#define ID_MENU_SIZE_SUPER_SMALL 1101
#define ID_MENU_SIZE_SMALL 1102
#define ID_MENU_SIZE_MEDIUM 1103
#define ID_MENU_SIZE_LARGE 1104

// Overlay position submenu
#define ID_MENU_POS_TOP_LEFT 1110
#define ID_MENU_POS_TOP_CENTER 1111
#define ID_MENU_POS_TOP_RIGHT 1112
#define ID_MENU_POS_CENTER_LEFT 1113
#define ID_MENU_POS_CENTER_CENTER 1114
#define ID_MENU_POS_CENTER_RIGHT 1115
#define ID_MENU_POS_BOTTOM_LEFT 1116
#define ID_MENU_POS_BOTTOM_CENTER 1117
#define ID_MENU_POS_BOTTOM_RIGHT 1118

// Animation speed submenu
#define ID_MENU_ANIM_FAST 1120
#define ID_MENU_ANIM_NORMAL 1121
#define ID_MENU_ANIM_SLOW 1122

// Auto-hide submenu
#define ID_MENU_HIDE_1SEC 1130
#define ID_MENU_HIDE_2SEC 1131
#define ID_MENU_HIDE_3SEC 1132
#define ID_MENU_HIDE_5SEC 1133
#define ID_MENU_HIDE_NEVER 1134

// Removed audio feedback volume levels - no longer needed

// Removed audio frequency settings - no longer needed

// Hotkey settings submenu
#define ID_MENU_HOTKEY_F1 1301
#define ID_MENU_HOTKEY_F2 1302
#define ID_MENU_HOTKEY_F3 1303
#define ID_MENU_HOTKEY_F4 1304
#define ID_MENU_HOTKEY_F5 1305
#define ID_MENU_HOTKEY_F6 1306
#define ID_MENU_HOTKEY_F7 1307
#define ID_MENU_HOTKEY_F8 1308
#define ID_MENU_HOTKEY_F9 1309
#define ID_MENU_HOTKEY_F10 1310
#define ID_MENU_HOTKEY_F11 1311
#define ID_MENU_HOTKEY_F12 1312
#define ID_MENU_HOTKEY_CTRL_M 1313
#define ID_MENU_HOTKEY_CUSTOM 1314

// Startup settings
#define ID_MENU_STARTUP_ENABLE 1401
#define ID_MENU_STARTUP_DISABLE 1402

// Volume lock
#define ID_MENU_VOLUME_LOCK_TOGGLE 1501

// Enums
typedef enum {
    SIZE_SUPER_SMALL = 16,
    SIZE_SMALL = 32,
    SIZE_MEDIUM = 64,
    SIZE_LARGE = 96
} OverlaySize;

typedef enum {
    POS_TOP_LEFT = 0,
    POS_TOP_CENTER = 1,
    POS_TOP_RIGHT = 2,
    POS_CENTER_LEFT = 3,
    POS_CENTER_CENTER = 4,
    POS_CENTER_RIGHT = 5,
    POS_BOTTOM_LEFT = 6,
    POS_BOTTOM_CENTER = 7,
    POS_BOTTOM_RIGHT = 8
} OverlayPosition;

typedef enum {
    ANIM_FAST = 50,
    ANIM_NORMAL = 100,
    ANIM_SLOW = 200
} AnimationSpeed;

// Removed BeepFrequency enum - no longer needed

typedef enum {
    HOTKEY_F1 = VK_F1,
    HOTKEY_F2 = VK_F2,
    HOTKEY_F3 = VK_F3,
    HOTKEY_F4 = VK_F4,
    HOTKEY_F5 = VK_F5,
    HOTKEY_F6 = VK_F6,
    HOTKEY_F7 = VK_F7,
    HOTKEY_F8 = VK_F8,
    HOTKEY_F9 = VK_F9,
    HOTKEY_F10 = VK_F10,
    HOTKEY_F11 = VK_F11,
    HOTKEY_F12 = VK_F12,
    HOTKEY_CTRL_M = 0x4D,
    HOTKEY_CUSTOM = 0xFFFF  // Special value for custom hotkey
} HotkeyType;

// Settings structure
typedef struct {
    OverlaySize overlaySize;
    OverlayPosition overlayPosition;
    BOOL multiMonitorMode;
    AnimationSpeed animationSpeed;
    int autoHideDuration; // in milliseconds
    BOOL audioFeedbackEnabled;
    BOOL reduceVolumeWhenMuted;
    HotkeyType currentHotkey;
    UINT customHotkeyModifiers;  // Custom hotkey modifiers
    UINT customHotkeyVK;         // Custom hotkey virtual key
    BOOL startupEnabled;
    BOOL volumeLockEnabled;
    float lockedVolume;
    int volumeCheckInterval; // for more reliable volume lock
} AppSettings;

// Global state
typedef struct {
    HWND hWnd;
    HWND hOverlayWnd;
    HWND* hOverlayWindows; // Array for multiple monitors
    int numMonitors;
    BOOL isMuted;
    AppSettings settings;
    IMMDeviceEnumerator* pEnumerator;
    IMMDevice* pDevice;
    IAudioEndpointVolume* pEndpointVolume;
    HICON hMutedIcon;
    BYTE overlayAlpha;
    BOOL isOverlayVisible;
    BOOL isFadingIn;
    BOOL isFadingOut;
} AppState;

extern AppState g_appState;

// Function declarations
// UI functions
BOOL CreateMainWindow(HINSTANCE hInstance);
void ShowOverlay(BOOL show);
void UpdateOverlayPosition(void);
void LoadIconFromFile(void);
void InitializeMultiMonitorOverlay(void);
void CleanupMultiMonitorOverlay(void);
BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// System tray functions
BOOL InitializeSystemTray(HWND hwnd, HINSTANCE hInstance);
void UpdateTrayIcon(BOOL isMuted);
void ShowTrayMenu(HWND hwnd, POINT pt);
void CleanupSystemTray(void);

// Audio functions
BOOL InitializeAudio(void);
void ToggleMicrophone(void);
void CleanupAudio(void);
void PlayBeep(BOOL isMuting);
void MonitorVolumeLevel(void);
void ReduceSystemVolume(BOOL reduce);

// Settings functions
void LoadSettings(void);
void SaveSettings(void);
void InitializeDefaultSettings(void);

// Hotkey functions
void RegisterCurrentHotkey(void);
void UnregisterCurrentHotkey(void);
void ChangeHotkey(HotkeyType newHotkey);
void ShowCustomHotkeyDialog(void);

// Startup functions
BOOL IsStartupEnabled(void);
void EnableStartup(BOOL enable);

// Volume lock functions
void LockCurrentVolume(void);
void RestoreLockedVolume(void);

#endif
