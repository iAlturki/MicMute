// settings.c - registry persistence. One table drives both load and save;
// loaded values are validated so a corrupted registry can never break the UI.
// Key and value names are unchanged from v1.x, so settings carry over.
#include "micmute.h"

#define REGISTRY_KEY L"SOFTWARE\\iAlturki\\MicMute"
#define STARTUP_KEY  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"

typedef struct { const wchar_t* name; void* ptr; DWORD type; DWORD size; } RegEntry;

static void RemoveLegacyRunEntry(void);

#define S g_appState.settings
static const RegEntry kRegMap[] = {
    {L"OverlaySize",           &S.overlaySize,            REG_DWORD,  sizeof(DWORD)},
    {L"OverlayPosition",       &S.overlayPosition,        REG_DWORD,  sizeof(DWORD)},
    {L"MultiMonitorMode",      &S.multiMonitorMode,       REG_DWORD,  sizeof(DWORD)},
    {L"AudioFeedbackEnabled",  &S.audioFeedbackEnabled,   REG_DWORD,  sizeof(DWORD)},
    {L"ReduceVolumeWhenMuted", &S.reduceVolumeWhenMuted,  REG_DWORD,  sizeof(DWORD)},
    {L"CurrentHotkey",         &S.currentHotkey,          REG_DWORD,  sizeof(DWORD)},
    {L"CustomHotkeyModifiers", &S.customHotkeyModifiers,  REG_DWORD,  sizeof(DWORD)},
    {L"CustomHotkeyVK",        &S.customHotkeyVK,         REG_DWORD,  sizeof(DWORD)},
    {L"VolumeLockEnabled",     &S.volumeLockEnabled,      REG_DWORD,  sizeof(DWORD)},
    {L"LastMuteState",         &S.lastMuteState,          REG_DWORD,  sizeof(DWORD)},
    {L"StartupEnabled",        &S.startupEnabled,         REG_DWORD,  sizeof(DWORD)},
    {L"LockedVolume",          &S.lockedVolume,           REG_BINARY, sizeof(float)},
};

static void Defaults(void)
{
    S.overlaySize = 64;
    S.overlayPosition = POS_TOP_RIGHT;
    S.multiMonitorMode = TRUE;
    S.audioFeedbackEnabled = TRUE;
    S.reduceVolumeWhenMuted = FALSE;
    S.currentHotkey = VK_F8;
    S.customHotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    S.customHotkeyVK = VK_F12;
    S.startupEnabled = FALSE;
    S.volumeLockEnabled = FALSE;
    S.lockedVolume = 1.0f;
    S.lastMuteState = FALSE;
}

static void Validate(void)
{
    BOOL sizeOk = FALSE;
    for (int i = 0; i < ARRAYSIZE(kOverlaySizes); i++)
        if (S.overlaySize == kOverlaySizes[i]) sizeOk = TRUE;
    if (!sizeOk) S.overlaySize = 64;

    if ((DWORD)S.overlayPosition >= POS_COUNT) S.overlayPosition = POS_TOP_RIGHT;

    S.customHotkeyModifiers &= MOD_ALT | MOD_CONTROL | MOD_SHIFT | MOD_WIN;
    BOOL customOk = S.customHotkeyVK >= 0x08 && S.customHotkeyVK <= 0xFE;
    BOOL hkOk = (S.currentHotkey >= VK_F1 && S.currentHotkey <= VK_F12)
                || S.currentHotkey == HOTKEY_CTRL_M
                || (S.currentHotkey == HOTKEY_CUSTOM && customOk);
    if (!hkOk) S.currentHotkey = VK_F8;

    if (!(S.lockedVolume >= 0.0f && S.lockedVolume <= 1.0f)) S.lockedVolume = 1.0f;
}

void LoadSettings(void)
{
    Defaults();
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        for (int i = 0; i < ARRAYSIZE(kRegMap); i++) {
            DWORD size = kRegMap[i].size;
            RegQueryValueExW(key, kRegMap[i].name, NULL, NULL, (LPBYTE)kRegMap[i].ptr, &size);
        }
        RegCloseKey(key);
    }
    Validate();
    // v1 used the Run key, which silently refuses elevated exes - drop it.
    // startupEnabled keeps its persisted value here (no schtasks spawn on
    // the startup path); the tray menu re-queries the task when opened.
    RemoveLegacyRunEntry();
}

void SaveSettings(void)
{
    HKEY key;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, REGISTRY_KEY, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL) != ERROR_SUCCESS)
        return;
    for (int i = 0; i < ARRAYSIZE(kRegMap); i++)
        RegSetValueExW(key, kRegMap[i].name, 0, kRegMap[i].type, (const BYTE*)kRegMap[i].ptr, kRegMap[i].size);
    RegCloseKey(key);
}

// The exe requires elevation, and the Run key silently refuses to launch
// elevated programs at logon - so autostart uses a Task Scheduler task with
// highest privileges instead. The legacy v1 Run entry is cleaned up.
static DWORD RunHidden(wchar_t* cmd)
{
    STARTUPINFOW si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    DWORD code = (DWORD)-1;
    if (CreateProcessW(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        WaitForSingleObject(pi.hProcess, 5000);
        GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    return code;
}

static void RemoveLegacyRunEntry(void)
{
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, STARTUP_KEY, 0, KEY_WRITE, &key) == ERROR_SUCCESS) {
        RegDeleteValueW(key, APP_NAME);
        RegCloseKey(key);
    }
}

BOOL IsStartupEnabled(void)
{
    wchar_t cmd[128];
    wcscpy_s(cmd, ARRAYSIZE(cmd), L"schtasks /Query /TN \"" APP_NAME L"\"");
    return RunHidden(cmd) == 0;
}

void EnableStartup(BOOL enable)
{
    wchar_t cmd[MAX_PATH * 2];
    if (enable) {
        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(NULL, exe, MAX_PATH);
        wsprintfW(cmd, L"schtasks /Create /F /RL HIGHEST /SC ONLOGON /TN \"%s\" /TR \"\\\"%s\\\"\"",
                  APP_NAME, exe);
    } else {
        wsprintfW(cmd, L"schtasks /Delete /F /TN \"%s\"", APP_NAME);
    }
    if (RunHidden(cmd) != 0 && enable) {
        TrayBalloon(APP_NAME, L"Could not create the startup task.");
        return;
    }
    RemoveLegacyRunEntry();
    S.startupEnabled = enable;
    SaveSettings();
}
