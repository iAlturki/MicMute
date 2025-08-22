#include "micmute.h"

// GUIDs for COM interfaces
const GUID CLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
const GUID IID_IMMDeviceEnumerator = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
const GUID IID_IAudioEndpointVolume = {0x5CDF2C82, 0x841E, 0x4546, {0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A}};

// Global variables for system volume control
static IMMDeviceEnumerator* g_pSystemEnumerator = NULL;
static IMMDevice* g_pSystemDevice = NULL;
static IAudioEndpointVolume* g_pSystemVolume = NULL;
static float g_originalSystemVolume = 1.0f;

BOOL InitializeAudio(void)
{
    HRESULT hr;
    
    // Create device enumerator
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                         &IID_IMMDeviceEnumerator, (void**)&g_appState.pEnumerator);
    if (FAILED(hr)) return FALSE;
    
    // Get default audio input device
    hr = g_appState.pEnumerator->lpVtbl->GetDefaultAudioEndpoint(
        g_appState.pEnumerator, eCapture, eConsole, &g_appState.pDevice);
    if (FAILED(hr)) return FALSE;
    
    // Get endpoint volume interface
    hr = g_appState.pDevice->lpVtbl->Activate(g_appState.pDevice,
        &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&g_appState.pEndpointVolume);
    if (FAILED(hr)) return FALSE;
    
    // Initialize system audio for volume control
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                         &IID_IMMDeviceEnumerator, (void**)&g_pSystemEnumerator);
    if (SUCCEEDED(hr)) {
        hr = g_pSystemEnumerator->lpVtbl->GetDefaultAudioEndpoint(
            g_pSystemEnumerator, eRender, eConsole, &g_pSystemDevice);
        if (SUCCEEDED(hr)) {
            hr = g_pSystemDevice->lpVtbl->Activate(g_pSystemDevice,
                &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&g_pSystemVolume);
            if (SUCCEEDED(hr)) {
                // Store original system volume
                g_pSystemVolume->lpVtbl->GetMasterVolumeLevelScalar(g_pSystemVolume, &g_originalSystemVolume);
            }
        }
    }
    
    // Get current mute state
    BOOL muted;
    hr = g_appState.pEndpointVolume->lpVtbl->GetMute(g_appState.pEndpointVolume, &muted);
    if (SUCCEEDED(hr)) {
        g_appState.isMuted = muted;
        UpdateTrayIcon(muted);
    }
    
    return TRUE;
}

void ToggleMicrophone(void)
{
    if (!g_appState.pEndpointVolume) return;
    
    BOOL newMute = !g_appState.isMuted;
    HRESULT hr = g_appState.pEndpointVolume->lpVtbl->SetMute(g_appState.pEndpointVolume, newMute, NULL);
    
    if (SUCCEEDED(hr)) {
        g_appState.isMuted = newMute;
        
        // Save the new state immediately
        g_appState.settings.lastMuteState = newMute;
        SaveSettings();
        
        UpdateTrayIcon(newMute);
        
        // Show overlay when muted, hide when unmuted
        ShowOverlay(newMute);
        
        // Apply system volume reduction if enabled
        ReduceSystemVolume(newMute);
        
        // Play beep feedback if enabled
        if (g_appState.settings.audioFeedbackEnabled) {
            PlayBeep(newMute);
        }
    }
}

void PlayBeep(BOOL isMuting)
{
    if (!g_appState.settings.audioFeedbackEnabled) return;
    
    // Use Windows 11 system notification sounds
    if (isMuting) {
        // Play "New Text Message Notification" sound for mute
        PlaySoundW(L"Windows Notify Messaging.wav", NULL, SND_ALIAS | SND_ASYNC | SND_NOWAIT);
    } else {
        // Play "New Mail Notification" sound for unmute  
        PlaySoundW(L"Windows Notify Email.wav", NULL, SND_ALIAS | SND_ASYNC | SND_NOWAIT);
    }
}

void LockCurrentVolume(void)
{
    if (!g_appState.pEndpointVolume) return;
    
    float currentVolume;
    HRESULT hr = g_appState.pEndpointVolume->lpVtbl->GetMasterVolumeLevelScalar(g_appState.pEndpointVolume, &currentVolume);
    
    if (SUCCEEDED(hr)) {
        g_appState.settings.lockedVolume = currentVolume;
        g_appState.settings.volumeLockEnabled = TRUE;
        SaveSettings();
        
        // Start monitoring timer with improved interval
        SetTimer(g_appState.hWnd, VOLUME_MONITOR_TIMER_ID, g_appState.settings.volumeCheckInterval, NULL);
    }
}

void RestoreLockedVolume(void)
{
    if (!g_appState.pEndpointVolume || !g_appState.settings.volumeLockEnabled) return;
    
    g_appState.pEndpointVolume->lpVtbl->SetMasterVolumeLevelScalar(g_appState.pEndpointVolume, g_appState.settings.lockedVolume, NULL);
}

void MonitorVolumeLevel(void)
{
    if (!g_appState.pEndpointVolume || !g_appState.settings.volumeLockEnabled) return;
    
    float currentVolume;
    HRESULT hr = g_appState.pEndpointVolume->lpVtbl->GetMasterVolumeLevelScalar(g_appState.pEndpointVolume, &currentVolume);
    
    if (SUCCEEDED(hr)) {
        // Check if volume has changed significantly (more than 0.5% for better reliability)
        if (fabs(currentVolume - g_appState.settings.lockedVolume) > 0.005f) {
            RestoreLockedVolume();
        }
    }
}

void ReduceSystemVolume(BOOL reduce)
{
    if (!g_pSystemVolume) return;
    
    if (reduce && g_appState.settings.reduceVolumeWhenMuted) {
        // Store current volume and reduce to 25%
        g_pSystemVolume->lpVtbl->GetMasterVolumeLevelScalar(g_pSystemVolume, &g_originalSystemVolume);
        g_pSystemVolume->lpVtbl->SetMasterVolumeLevelScalar(g_pSystemVolume, 0.25f, NULL);
    } else if (!reduce) {
        // Restore original volume (always restore when requested)
        g_pSystemVolume->lpVtbl->SetMasterVolumeLevelScalar(g_pSystemVolume, g_originalSystemVolume, NULL);
    }
}

void CleanupAudio(void)
{
    // Restore system volume if it was reduced
    if (g_appState.settings.reduceVolumeWhenMuted && g_appState.isMuted) {
        ReduceSystemVolume(FALSE);
    }
    
    if (g_pSystemVolume) {
        g_pSystemVolume->lpVtbl->Release(g_pSystemVolume);
        g_pSystemVolume = NULL;
    }
    
    if (g_pSystemDevice) {
        g_pSystemDevice->lpVtbl->Release(g_pSystemDevice);
        g_pSystemDevice = NULL;
    }
    
    if (g_pSystemEnumerator) {
        g_pSystemEnumerator->lpVtbl->Release(g_pSystemEnumerator);
        g_pSystemEnumerator = NULL;
    }
    
    if (g_appState.pEndpointVolume) {
        g_appState.pEndpointVolume->lpVtbl->Release(g_appState.pEndpointVolume);
        g_appState.pEndpointVolume = NULL;
    }
    
    if (g_appState.pDevice) {
        g_appState.pDevice->lpVtbl->Release(g_appState.pDevice);
        g_appState.pDevice = NULL;
    }
    
    if (g_appState.pEnumerator) {
        g_appState.pEnumerator->lpVtbl->Release(g_appState.pEnumerator);
        g_appState.pEnumerator = NULL;
    }
}
