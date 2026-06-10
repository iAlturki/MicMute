// audio.c - Core Audio, fully event-driven. A volume callback pushes external
// mute/volume changes to the UI thread (no polling timers), and a device
// notification client survives default-microphone hot-swaps. Changes made by
// the app itself carry GUID_MicMuteEvent so its own events are ignored.
#include "micmute.h"

const GUID CLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E}};
const GUID IID_IMMDeviceEnumerator  = {0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6}};
const GUID IID_IAudioEndpointVolume = {0x5CDF2C82, 0x841E, 0x4546, {0x97, 0x22, 0x0C, 0xF7, 0x40, 0x78, 0x22, 0x9A}};
const GUID IID_IAudioEndpointVolumeCallback = {0x657804FA, 0xD6AD, 0x4496, {0x8A, 0x60, 0x35, 0x27, 0x52, 0xAF, 0x4F, 0x89}};
const GUID IID_IMMNotificationClient = {0x7991EEC9, 0x7E89, 0x4D85, {0x83, 0x90, 0x6C, 0x70, 0x3C, 0xEC, 0x60, 0xC0}};
const GUID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
static const GUID GUID_MicMuteEvent = {0x8E1C9A5B, 0x3F2D, 0x4C7E, {0x9B, 0x6A, 0xD0, 0xE5, 0x4F, 0x7A, 0x21, 0xC3}};

static IMMDeviceEnumerator*  g_enum;
static IMMDevice*            g_micDev;
static IAudioEndpointVolume* g_micVol;
static IMMDevice*            g_sysDev;
static IAudioEndpointVolume* g_sysVol;
static float g_origSysVol = 1.0f;
static BOOL  g_ducked;

// ---- IAudioEndpointVolumeCallback (mic mute/volume changed by anyone) ----
static HRESULT STDMETHODCALLTYPE VC_QueryInterface(IAudioEndpointVolumeCallback* This, REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IAudioEndpointVolumeCallback)) {
        *ppv = This;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE VC_AddRef(IAudioEndpointVolumeCallback* This)  { return 1; }  // static lifetime
static ULONG STDMETHODCALLTYPE VC_Release(IAudioEndpointVolumeCallback* This) { return 1; }

static HRESULT STDMETHODCALLTYPE VC_OnNotify(IAudioEndpointVolumeCallback* This, PAUDIO_VOLUME_NOTIFICATION_DATA n)
{
    if (!n) return E_POINTER;
    if (IsEqualGUID(&n->guidEventContext, &GUID_MicMuteEvent)) return S_OK;
    // marshal to the UI thread
    PostMessageW(g_appState.hWnd, WM_APP_AUDIO, (WPARAM)n->bMuted, (LPARAM)(int)(n->fMasterVolume * 10000.0f));
    return S_OK;
}

static IAudioEndpointVolumeCallbackVtbl g_volCbVtbl = {VC_QueryInterface, VC_AddRef, VC_Release, VC_OnNotify};
static IAudioEndpointVolumeCallback g_volCb = {&g_volCbVtbl};

// ---- IMMNotificationClient (default capture device changed) ----
static HRESULT STDMETHODCALLTYPE NC_QueryInterface(IMMNotificationClient* This, REFIID riid, void** ppv)
{
    if (!ppv) return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMMNotificationClient)) {
        *ppv = This;
        return S_OK;
    }
    *ppv = NULL;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE NC_AddRef(IMMNotificationClient* This)  { return 1; }
static ULONG STDMETHODCALLTYPE NC_Release(IMMNotificationClient* This) { return 1; }
static HRESULT STDMETHODCALLTYPE NC_OnDeviceStateChanged(IMMNotificationClient* This, LPCWSTR id, DWORD state) { return S_OK; }
static HRESULT STDMETHODCALLTYPE NC_OnDeviceAdded(IMMNotificationClient* This, LPCWSTR id) { return S_OK; }
static HRESULT STDMETHODCALLTYPE NC_OnDeviceRemoved(IMMNotificationClient* This, LPCWSTR id) { return S_OK; }
static HRESULT STDMETHODCALLTYPE NC_OnPropertyValueChanged(IMMNotificationClient* This, LPCWSTR id, const PROPERTYKEY key) { return S_OK; }

static HRESULT STDMETHODCALLTYPE NC_OnDefaultDeviceChanged(IMMNotificationClient* This, EDataFlow flow, ERole role, LPCWSTR id)
{
    if (role == eConsole && (flow == eCapture || flow == eRender))
        PostMessageW(g_appState.hWnd, WM_APP_DEVICE, flow == eRender, 0);
    return S_OK;
}

static IMMNotificationClientVtbl g_notifVtbl = {
    NC_QueryInterface, NC_AddRef, NC_Release,
    NC_OnDeviceStateChanged, NC_OnDeviceAdded, NC_OnDeviceRemoved,
    NC_OnDefaultDeviceChanged, NC_OnPropertyValueChanged
};
static IMMNotificationClient g_notifClient = {&g_notifVtbl};

// ---- endpoint management ----
#define RELEASE(p) do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)

static void ReleaseMicEndpoint(void)
{
    if (g_micVol) g_micVol->lpVtbl->UnregisterControlChangeNotify(g_micVol, &g_volCb);
    RELEASE(g_micVol);
    RELEASE(g_micDev);
}

static BOOL InitMicEndpoint(void)
{
    ReleaseMicEndpoint();
    if (FAILED(g_enum->lpVtbl->GetDefaultAudioEndpoint(g_enum, eCapture, eConsole, &g_micDev)))
        return FALSE;
    if (FAILED(g_micDev->lpVtbl->Activate(g_micDev, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&g_micVol))) {
        RELEASE(g_micDev);
        return FALSE;
    }
    g_micVol->lpVtbl->RegisterControlChangeNotify(g_micVol, &g_volCb);
    return TRUE;
}

static void InitSystemEndpoint(void)
{
    if (SUCCEEDED(g_enum->lpVtbl->GetDefaultAudioEndpoint(g_enum, eRender, eConsole, &g_sysDev)))
        if (SUCCEEDED(g_sysDev->lpVtbl->Activate(g_sysDev, &IID_IAudioEndpointVolume, CLSCTX_ALL, NULL, (void**)&g_sysVol)))
            g_sysVol->lpVtbl->GetMasterVolumeLevelScalar(g_sysVol, &g_origSysVol);
}

BOOL AudioInit(void)
{
    if (FAILED(CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                &IID_IMMDeviceEnumerator, (void**)&g_enum)))
        return FALSE;
    g_enum->lpVtbl->RegisterEndpointNotificationCallback(g_enum, &g_notifClient);
    InitMicEndpoint();      // may fail (no mic yet); device events will retry
    InitSystemEndpoint();
    return TRUE;
}

// Default output device changed: restore the old endpoint's volume while it
// is still reachable, rebind, and re-duck on the new device if muted.
void AudioReinitSystemEndpoint(void)
{
    DuckSystemVolume(FALSE);
    RELEASE(g_sysVol);
    RELEASE(g_sysDev);
    InitSystemEndpoint();
    DuckSystemVolume(g_appState.isMuted);
}

void AudioReinitDevice(void)
{
    BOOL hadMic = g_micVol != NULL;
    if (InitMicEndpoint()) {
        // Privacy first: the app's mute state follows onto the new device.
        g_micVol->lpVtbl->SetMute(g_micVol, g_appState.isMuted, &GUID_MicMuteEvent);
        if (g_appState.settings.volumeLockEnabled) RestoreLockedVolume();
        if (!hadMic) TrayBalloon(APP_NAME, L"Microphone connected.");
    } else if (hadMic) {
        TrayBalloon(APP_NAME, L"No microphone detected.");
    }
    TrayUpdate();
}

BOOL AudioGetMute(BOOL* muted)
{
    return g_micVol && SUCCEEDED(g_micVol->lpVtbl->GetMute(g_micVol, muted));
}

void SetMicMute(BOOL mute)
{
    if (!g_micVol) {
        TrayBalloon(APP_NAME, L"No microphone available.");
        return;
    }
    if (SUCCEEDED(g_micVol->lpVtbl->SetMute(g_micVol, mute, &GUID_MicMuteEvent))) {
        g_appState.isMuted = mute;
        SyncMuteUI();
        PlayFeedback(mute);
    }
}

void ToggleMicrophone(void)
{
    SetMicMute(!g_appState.isMuted);
}

void LockCurrentVolume(void)
{
    float v;
    if (g_micVol && SUCCEEDED(g_micVol->lpVtbl->GetMasterVolumeLevelScalar(g_micVol, &v))) {
        g_appState.settings.lockedVolume = v;
        g_appState.settings.volumeLockEnabled = TRUE;
        SaveSettings();
    }
}

void RestoreLockedVolume(void)
{
    if (g_micVol && g_appState.settings.volumeLockEnabled)
        g_micVol->lpVtbl->SetMasterVolumeLevelScalar(g_micVol, g_appState.settings.lockedVolume, &GUID_MicMuteEvent);
}

void DuckSystemVolume(BOOL duck)
{
    if (!g_sysVol) return;
    BOOL want = duck && g_appState.settings.reduceVolumeWhenMuted;
    if (want == g_ducked) return;
    if (want) {
        g_sysVol->lpVtbl->GetMasterVolumeLevelScalar(g_sysVol, &g_origSysVol);
        g_sysVol->lpVtbl->SetMasterVolumeLevelScalar(g_sysVol, min(g_origSysVol, 0.25f), NULL);
    } else {
        g_sysVol->lpVtbl->SetMasterVolumeLevelScalar(g_sysVol, g_origSysVol, NULL);
    }
    g_ducked = want;
}

// ---- feedback blips: two short tones synthesized into in-memory WAVs ----
#define FB_RATE   22050
#define FB_SAMPLES (FB_RATE / 5)              // 200 ms
#define FB_BYTES  (44 + FB_SAMPLES * 2)

static void SynthBlip(BYTE* buf, float f1, float f2)
{
    DWORD dataBytes = FB_SAMPLES * 2;
    memcpy(buf, "RIFF", 4);  *(DWORD*)(buf + 4) = 36 + dataBytes;
    memcpy(buf + 8, "WAVEfmt ", 8);
    *(DWORD*)(buf + 16) = 16; *(WORD*)(buf + 20) = 1; *(WORD*)(buf + 22) = 1;
    *(DWORD*)(buf + 24) = FB_RATE; *(DWORD*)(buf + 28) = FB_RATE * 2;
    *(WORD*)(buf + 32) = 2; *(WORD*)(buf + 34) = 16;
    memcpy(buf + 36, "data", 4); *(DWORD*)(buf + 40) = dataBytes;

    short* s = (short*)(buf + 44);
    int half = FB_SAMPLES / 2;
    for (int i = 0; i < FB_SAMPLES; i++) {
        int tone = i < half;
        float f = tone ? f1 : f2;
        float x = (float)(tone ? i : i - half) / half;          // 0..1 in tone
        float env = x < 0.08f ? x / 0.08f : (1 - x) * (1 - x);  // attack, decay
        s[i] = (short)(sinf(6.2831853f * f * i / FB_RATE) * env * 8000.0f);
    }
}

void PlayFeedback(BOOL isMuting)
{
    if (!g_appState.settings.audioFeedbackEnabled) return;
    static BYTE wavMute[FB_BYTES], wavLive[FB_BYTES];
    static BOOL built;
    if (!built) {
        SynthBlip(wavMute, 587.33f, 440.00f);   // D5 -> A4, descending = off
        SynthBlip(wavLive, 440.00f, 659.25f);   // A4 -> E5, ascending = on
        built = TRUE;
    }
    PlaySoundW((LPCWSTR)(isMuting ? wavMute : wavLive), NULL, SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
}

void AudioCleanup(void)
{
    if (g_enum) g_enum->lpVtbl->UnregisterEndpointNotificationCallback(g_enum, &g_notifClient);
    ReleaseMicEndpoint();
    RELEASE(g_sysVol);
    RELEASE(g_sysDev);
    RELEASE(g_enum);
}
