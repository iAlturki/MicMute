#include "micmute.h"

void RegisterCurrentHotkey(void)
{
    UINT modifiers = 0;
    UINT vk = g_appState.settings.currentHotkey;
    
    // Handle special cases
    if (g_appState.settings.currentHotkey == HOTKEY_CTRL_M) {
        modifiers = MOD_CONTROL;
        vk = 0x4D; // 'M' key
    } else if (g_appState.settings.currentHotkey == HOTKEY_CUSTOM) {
        modifiers = g_appState.settings.customHotkeyModifiers;
        vk = g_appState.settings.customHotkeyVK;
    }
    
    RegisterHotKey(g_appState.hWnd, HOTKEY_ID, modifiers, vk);
}

void UnregisterCurrentHotkey(void)
{
    UnregisterHotKey(g_appState.hWnd, HOTKEY_ID);
}

void ChangeHotkey(HotkeyType newHotkey)
{
    // Unregister current hotkey
    UnregisterCurrentHotkey();
    
    // Update setting
    g_appState.settings.currentHotkey = newHotkey;
    
    // Register new hotkey
    RegisterCurrentHotkey();
    
    // Save settings
    SaveSettings();
}

void ShowCustomHotkeyDialog(void)
{
    MessageBoxW(g_appState.hWnd, 
        L"To set a custom hotkey:\n\n"
        L"1. Hold the modifier keys (Ctrl, Alt, Shift, Win)\n"
        L"2. Press the desired key\n"
        L"3. Click OK when ready\n\n"
        L"Example combinations:\n"
        L"- Ctrl+Shift+M\n"
        L"- Alt+F1\n"
        L"- Win+Space\n\n"
        L"Press the keys now and then click OK:",
        L"Custom Hotkey Setup", 
        MB_OK | MB_ICONINFORMATION);
    
    // For now, set to a default custom combination
    // This could be enhanced with a proper key capture dialog
    g_appState.settings.customHotkeyModifiers = MOD_CONTROL | MOD_SHIFT;
    g_appState.settings.customHotkeyVK = VK_F12;
    ChangeHotkey(HOTKEY_CUSTOM);
}
