// settings.c - registry persistence. One table drives both load and save;
// loaded values are validated so a corrupted registry can never break the UI.
// Key and value names are unchanged from v1.x, so settings carry over.
#include "micmute.h"

#define REGISTRY_KEY L"SOFTWARE\\iAlturki\\MicMute"
#define STARTUP_KEY  L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"

typedef struct { const wchar_t* name; void* ptr; DWORD type; DWORD size; } RegEntry;

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
    S.startupEnabled = IsStartupEnabled();   // the Run key is the truth
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

BOOL IsStartupEnabled(void)
{
    HKEY key;
    BOOL enabled = FALSE;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, STARTUP_KEY, 0, KEY_READ, &key) == ERROR_SUCCESS) {
        enabled = RegQueryValueExW(key, APP_NAME, NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
        RegCloseKey(key);
    }
    return enabled;
}

void EnableStartup(BOOL enable)
{
    HKEY key;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, STARTUP_KEY, 0, KEY_WRITE, &key) == ERROR_SUCCESS) {
        if (enable) {
            wchar_t cmd[MAX_PATH + 2] = L"\"";
            GetModuleFileNameW(NULL, cmd + 1, MAX_PATH);
            wcscat_s(cmd, ARRAYSIZE(cmd), L"\"");
            RegSetValueExW(key, APP_NAME, 0, REG_SZ, (const BYTE*)cmd,
                           (DWORD)((wcslen(cmd) + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(key, APP_NAME);
        }
        RegCloseKey(key);
    }
    S.startupEnabled = enable;
    SaveSettings();
}
