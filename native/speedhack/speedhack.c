#include <windows.h>
#include <objbase.h>
#include <mmsystem.h>
#include <dsound.h>
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "MinHook.h"
#ifdef CE_USE_SOUNDTOUCH
#include "soundtouch_bridge.h"
#endif

#pragma comment(lib, "winmm.lib")

typedef DWORD (WINAPI *GetTickCount_t)(void);
typedef ULONGLONG (WINAPI *GetTickCount64_t)(void);
typedef BOOL (WINAPI *QueryPerformanceCounter_t)(LARGE_INTEGER *);
typedef DWORD (WINAPI *timeGetTime_t)(void);
typedef HRESULT (WINAPI *XAudio2Create_t)(void **ppXAudio2, UINT32 flags, UINT32 processor);
typedef HRESULT (WINAPI *DirectSoundCreate_t)(void *guid, void **ppDS, void *unk);
typedef HRESULT (WINAPI *DirectSoundCreate8_t)(void *guid, void **ppDS8, void *unk);
typedef MMRESULT (WINAPI *waveOutOpen_t)(LPHWAVEOUT, UINT, LPCWAVEFORMATEX, DWORD_PTR, DWORD_PTR, DWORD);
typedef MMRESULT (WINAPI *waveOutClose_t)(HWAVEOUT);
typedef MMRESULT (WINAPI *waveOutWrite_t)(HWAVEOUT, LPWAVEHDR, UINT);
typedef MMRESULT (WINAPI *waveOutSetPlaybackRate_t)(HWAVEOUT, DWORD);
typedef MMRESULT (WINAPI *waveOutSetPitch_t)(HWAVEOUT, DWORD);
typedef HMODULE (WINAPI *LoadLibraryExW_t)(LPCWSTR, HANDLE, DWORD);
typedef HMODULE (WINAPI *LoadLibraryW_t)(LPCWSTR);
typedef HMODULE (WINAPI *LoadLibraryA_t)(LPCSTR);
typedef HRESULT (STDMETHODCALLTYPE *MM_GetDefaultAudioEndpoint_t)(void *this, int dataFlow, int role, void **device);
typedef HRESULT (STDMETHODCALLTYPE *MM_Activate_t)(void *this, REFIID iid, DWORD clsctx, void *activationParams, void **interfaceOut);
typedef HRESULT (STDMETHODCALLTYPE *AC_GetMixFormat_t)(void *this, WAVEFORMATEX **format);
typedef HRESULT (STDMETHODCALLTYPE *AC_Initialize_t)(
    void *this, int shareMode, DWORD streamFlags, LONGLONG bufferDuration,
    LONGLONG periodicity, const WAVEFORMATEX *format, LPCGUID sessionGuid);
typedef HRESULT (STDMETHODCALLTYPE *AC_GetService_t)(void *this, REFIID iid, void **service);
typedef HRESULT (STDMETHODCALLTYPE *RC_GetBuffer_t)(void *this, UINT32 requestedFrames, BYTE **data);
typedef HRESULT (STDMETHODCALLTYPE *RC_ReleaseBuffer_t)(void *this, UINT32 writtenFrames, DWORD flags);
typedef HRESULT (STDMETHODCALLTYPE *Unknown_QueryInterface_t)(void *this, REFIID iid, void **interfaceOut);
typedef HRESULT (STDMETHODCALLTYPE *SA_ActivateStream_t)(void *this, const void *params, REFIID iid, void **stream);
typedef HRESULT (STDMETHODCALLTYPE *SA_BeginUpdate_t)(void *this, UINT32 *availableObjects, UINT32 *frames);
typedef HRESULT (STDMETHODCALLTYPE *SA_EndUpdate_t)(void *this);
typedef HRESULT (STDMETHODCALLTYPE *SA_ActivateObject_t)(void *this, UINT32 objectType, void **object);
typedef HRESULT (STDMETHODCALLTYPE *SA_ObjectGetBuffer_t)(void *this, BYTE **buffer, UINT32 *bytes);

__declspec(dllexport) GetTickCount_t realGetTickCount = NULL;
__declspec(dllexport) GetTickCount64_t realGetTickCount64 = NULL;
__declspec(dllexport) QueryPerformanceCounter_t realQueryPerformanceCounter = NULL;
__declspec(dllexport) timeGetTime_t realgettime = NULL;
__declspec(dllexport) GetTickCount_t realgettickcount = NULL;
__declspec(dllexport) GetTickCount64_t realgettickcount64 = NULL;
__declspec(dllexport) QueryPerformanceCounter_t realqueryperformancecounter = NULL;

static volatile float g_speed = 1.0f;
static CRITICAL_SECTION g_lock;
static CRITICAL_SECTION g_audio_lock;
static int g_cs_inited = 0;
static int g_mh_inited = 0;
static int g_timing_hooks_ready = 0;
static int g_audio_hooks_ready = 0;
static DWORD g_initial_offset_gtc = 0;
static DWORD g_initial_time_gtc = 0;
static ULONGLONG g_initial_offset_gtc64 = 0;
static ULONGLONG g_initial_time_gtc64 = 0;
static LONGLONG g_initial_offset_qpc = 0;
static LONGLONG g_initial_time_qpc = 0;

#define MAX_XA_VOICES 512
#define MAX_DS_BUFFERS 512
#define MAX_WAVE_OUT 64

typedef HRESULT (STDMETHODCALLTYPE *XA_SetFrequencyRatio_t)(void *this, float ratio, UINT32 operationSet);
typedef void (STDMETHODCALLTYPE *XA_GetFrequencyRatio_t)(void *this, float *ratio);
typedef void (STDMETHODCALLTYPE *XA_DestroyVoice_t)(void *this);
typedef HRESULT (STDMETHODCALLTYPE *XA_Start_t)(void *this, UINT32 flags, UINT32 operationSet);
typedef HRESULT (STDMETHODCALLTYPE *XA_SubmitSourceBuffer_t)(void *this, const void *buffer, const void *bufferWma);
typedef HRESULT (STDMETHODCALLTYPE *XA_CreateSourceVoice_t)(
    void *this, void **ppVoice, const void *pFormat, UINT32 flags,
    float maxFreqRatio, void *pCallback, void *pSendList, void *pEffectChain);
typedef HRESULT (STDMETHODCALLTYPE *XA_CreateMasteringVoiceModern_t)(
    void *this, void **voice, UINT32 channels, UINT32 sampleRate, UINT32 flags,
    LPCWSTR deviceId, const void *effectChain, int streamCategory);
typedef HRESULT (STDMETHODCALLTYPE *XA_CreateMasteringVoice27_t)(
    void *this, void **voice, UINT32 channels, UINT32 sampleRate, UINT32 flags,
    UINT32 deviceIndex, const void *effectChain);
typedef ULONG (STDMETHODCALLTYPE *XA_Release_t)(void *this);
typedef HRESULT (STDMETHODCALLTYPE *DS_SetFrequency_t)(void *this, DWORD freq);
typedef ULONG (STDMETHODCALLTYPE *DS_Release_t)(void *this);
typedef HRESULT (STDMETHODCALLTYPE *DS_CreateSoundBuffer_t)(void *this, LPCDSBUFFERDESC desc, void **ppBuffer, void *pUnkOuter);

static XA_SetFrequencyRatio_t real_XA_SetFrequencyRatio = NULL;
static XA_GetFrequencyRatio_t real_XA_GetFrequencyRatio = NULL;
static XA_DestroyVoice_t real_XA_DestroyVoice = NULL;
static XA_Start_t real_XA_Start = NULL;
static XA_SubmitSourceBuffer_t real_XA_SubmitSourceBuffer = NULL;
static XA_CreateSourceVoice_t real_XA_CreateSourceVoice_v28 = NULL;
static XA_CreateSourceVoice_t real_XA_CreateSourceVoice_v27 = NULL;
static DS_SetFrequency_t real_DS_SetFrequency = NULL;
static DS_Release_t real_DS_Release = NULL;
static DS_CreateSoundBuffer_t real_DS_CreateSoundBuffer = NULL;
static XAudio2Create_t real_XAudio2Create = NULL;
static DirectSoundCreate_t real_DirectSoundCreate = NULL;
static DirectSoundCreate8_t real_DirectSoundCreate8 = NULL;
static waveOutOpen_t real_waveOutOpen = NULL;
static waveOutClose_t real_waveOutClose = NULL;
static waveOutWrite_t real_waveOutWrite = NULL;
static waveOutSetPlaybackRate_t real_waveOutSetPlaybackRate = NULL;
static waveOutSetPitch_t real_waveOutSetPitch = NULL;
static LoadLibraryExW_t real_LoadLibraryExW = NULL;
static LoadLibraryW_t real_LoadLibraryW = NULL;
static LoadLibraryA_t real_LoadLibraryA = NULL;
static RC_GetBuffer_t real_RC_GetBuffer = NULL;
static RC_ReleaseBuffer_t real_RC_ReleaseBuffer = NULL;
static MM_Activate_t real_MM_Activate = NULL;
static AC_Initialize_t real_AC_Initialize = NULL;
static AC_GetService_t real_AC_GetService = NULL;
static SA_ActivateStream_t real_SA_ActivateStream = NULL;
static SA_BeginUpdate_t real_SA_BeginUpdate = NULL;
static SA_EndUpdate_t real_SA_EndUpdate = NULL;
static SA_ActivateObject_t real_SA_ActivateObject = NULL;
static SA_ObjectGetBuffer_t real_SA_ObjectGetBuffer = NULL;

typedef struct { void *voice; float base_ratio; float max_ratio; int used; } XAVoiceRec;
typedef struct { void *buffer; DWORD base_freq; int used; } DSBufferRec;
typedef struct { HWAVEOUT handle; DWORD base_rate; DWORD base_pitch; int used; } WaveOutRec;
typedef struct {
    void *render_client;
    BYTE *buffer;
    UINT32 requested_frames;
    UINT32 block_align;
    int used;
} WASAPIRenderRec;
typedef struct {
    void *object;
    BYTE *actual_buffer;
    UINT32 actual_bytes;
    BYTE *shadow_buffer;
    SIZE_T shadow_capacity;
    UINT32 shadow_bytes;
    LONG generation;
    int used;
} SpatialObjectRec;

static XAVoiceRec g_xa_voices[MAX_XA_VOICES];
static DSBufferRec g_ds_buffers[MAX_DS_BUFFERS];
static WaveOutRec g_wave_outs[MAX_WAVE_OUT];
static int g_hooked_xaudio2 = 0, g_hooked_dsound = 0, g_hooked_winmm = 0;
static int g_hooked_wasapi = 0;
static int g_xaudio_is_modern = 1;
static volatile LONG g_scan_thread_started = 0;
static UINT32 g_wasapi_block_align = 0;
#define MAX_WASAPI_RENDERS 64
static WASAPIRenderRec g_wasapi_renders[MAX_WASAPI_RENDERS];
#define MAX_SPATIAL_OBJECTS 64
static SpatialObjectRec g_spatial_objects[MAX_SPATIAL_OBJECTS];
static volatile LONG g_spatial_generation = 0;
static UINT32 g_spatial_actual_frames = 0;
static UINT32 g_spatial_fake_frames = 0;
#ifdef CE_USE_SOUNDTOUCH
static void *g_spatial_soundtouch = NULL;
static float *g_spatial_interleaved_input = NULL;
static float *g_spatial_interleaved_output = NULL;
static SIZE_T g_spatial_input_capacity = 0;
static SIZE_T g_spatial_output_capacity = 0;
static UINT32 g_spatial_soundtouch_channels = 0;
static float g_spatial_soundtouch_last_speed = 1.0f;
#endif
static volatile LONG g_audio_status = 0;
static volatile LONG g_last_xaudio_result = 0;

enum {
    AUDIO_STATUS_MINHOOK_READY = 1 << 0,
    AUDIO_STATUS_XAUDIO_FOUND = 1 << 1,
    AUDIO_STATUS_XAUDIO_EXPORT_HOOKED = 1 << 2,
    AUDIO_STATUS_XAUDIO_ENGINE_CREATED = 1 << 3,
    AUDIO_STATUS_XAUDIO_MASTER_CREATED = 1 << 4,
    AUDIO_STATUS_XAUDIO_TEMP_VOICE_CREATED = 1 << 5,
    AUDIO_STATUS_XAUDIO_ENGINE_HOOKED = 1 << 6,
    AUDIO_STATUS_XAUDIO_VOICE_HOOKED = 1 << 7,
    AUDIO_STATUS_XAUDIO_GAME_VOICE_SEEN = 1 << 8,
    AUDIO_STATUS_XAUDIO_RATIO_APPLIED = 1 << 9,
    AUDIO_STATUS_WASAPI_FOUND = 1 << 10,
    AUDIO_STATUS_WASAPI_HOOKED = 1 << 11,
    AUDIO_STATUS_WASAPI_BUFFER_SEEN = 1 << 12,
    AUDIO_STATUS_WASAPI_BUFFER_SCALED = 1 << 13,
    AUDIO_STATUS_SPATIAL_FOUND = 1 << 14,
    AUDIO_STATUS_SPATIAL_HOOKED = 1 << 15,
    AUDIO_STATUS_SPATIAL_BUFFER_SEEN = 1 << 16,
    AUDIO_STATUS_SPATIAL_BUFFER_SCALED = 1 << 17,
    AUDIO_STATUS_SPATIAL_TIMESTRETCHED = 1 << 18
};

static const GUID ce_clsid_mmdevice_enumerator =
    {0xbcde0395, 0xe52f, 0x467c, {0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e}};
static const GUID ce_iid_mmdevice_enumerator =
    {0xa95664d2, 0x9614, 0x4f35, {0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6}};
static const GUID ce_iid_audio_client =
    {0x1cb9ad4c, 0xdbfa, 0x4c32, {0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2}};
static const GUID ce_iid_audio_render_client =
    {0xf294acfc, 0x3146, 0x4483, {0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2}};
static const GUID ce_iid_spatial_audio_client =
    {0xbbf8e066, 0xaaaa, 0x49be, {0x9a, 0x4d, 0xfd, 0x2a, 0x85, 0x8e, 0xa2, 0x7f}};

enum {
    MM_VT_GET_DEFAULT_AUDIO_ENDPOINT = 4,
    DEVICE_VT_ACTIVATE = 3,
    AC_VT_INITIALIZE = 3,
    AC_VT_GET_MIX_FORMAT = 8,
    AC_VT_GET_SERVICE = 14,
    RC_VT_GET_BUFFER = 3,
    RC_VT_RELEASE_BUFFER = 4,
    SA_CLIENT_VT_ACTIVATE_STREAM = 10,
    SA_STREAM_VT_BEGIN_UPDATE = 8,
    SA_STREAM_VT_END_UPDATE = 9,
    SA_STREAM_VT_ACTIVATE_OBJECT = 10,
    SA_OBJECT_VT_GET_BUFFER = 3
};

static void ensure_cs(void) {
    if (!g_cs_inited) {
        InitializeCriticalSection(&g_lock);
        InitializeCriticalSection(&g_audio_lock);
        g_cs_inited = 1;
    }
}
static float clamp_speed(float s) {
    if (!isfinite(s) || s <= 0.f) return 1.f;
    if (s < 0.01f) return 0.01f;
    if (s > 100.f) return 100.f;
    return s;
}
static float current_speed(void) { return clamp_speed(g_speed); }

static int hook_vtable_slot(void **slot, void *hook, void **original) {
    DWORD oldProtect;
    if (!slot || !*slot || !hook || !original) return 0;
    if (*slot == hook) return 1;
    if (!*original) *original = *slot;
    if (!VirtualProtect(slot, sizeof(void *), PAGE_READWRITE, &oldProtect)) return 0;
    InterlockedExchangePointer((PVOID volatile *)slot, hook);
    VirtualProtect(slot, sizeof(void *), oldProtect, &oldProtect);
    return *slot == hook;
}

static WASAPIRenderRec *wasapi_find_render(void *renderClient, int create) {
    int i;
    WASAPIRenderRec *empty = NULL;
    for (i = 0; i < MAX_WASAPI_RENDERS; i++) {
        if (g_wasapi_renders[i].used && g_wasapi_renders[i].render_client == renderClient)
            return &g_wasapi_renders[i];
        if (!g_wasapi_renders[i].used && !empty) empty = &g_wasapi_renders[i];
    }
    if (create && empty) {
        ZeroMemory(empty, sizeof(*empty));
        empty->used = 1;
        empty->render_client = renderClient;
        empty->block_align = g_wasapi_block_align;
        return empty;
    }
    return NULL;
}

static HRESULT STDMETHODCALLTYPE hook_RC_GetBuffer(void *this, UINT32 requestedFrames, BYTE **data) {
    HRESULT result;
    WASAPIRenderRec *record;
    if (!real_RC_GetBuffer) return E_FAIL;
    result = real_RC_GetBuffer(this, requestedFrames, data);
    if (SUCCEEDED(result) && data && *data) {
        EnterCriticalSection(&g_audio_lock);
        record = wasapi_find_render(this, 1);
        if (record) {
            record->buffer = *data;
            record->requested_frames = requestedFrames;
            if (!record->block_align) record->block_align = g_wasapi_block_align;
        }
        LeaveCriticalSection(&g_audio_lock);
        InterlockedOr(&g_audio_status, AUDIO_STATUS_WASAPI_BUFFER_SEEN);
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE hook_RC_ReleaseBuffer(void *this, UINT32 writtenFrames, DWORD flags) {
    WASAPIRenderRec snapshot;
    WASAPIRenderRec *record;
    float speed = current_speed();
    UINT32 outputFrames = writtenFrames;
    UINT32 i;
    ZeroMemory(&snapshot, sizeof(snapshot));
    EnterCriticalSection(&g_audio_lock);
    record = wasapi_find_render(this, 0);
    if (record) {
        snapshot = *record;
        record->buffer = NULL;
        record->requested_frames = 0;
    }
    LeaveCriticalSection(&g_audio_lock);

    if (speed > 1.01f && writtenFrames > 1 && snapshot.buffer && snapshot.block_align) {
        outputFrames = (UINT32)((float)writtenFrames / speed);
        if (outputFrames < 1) outputFrames = 1;
        if (outputFrames > writtenFrames) outputFrames = writtenFrames;
        if (!(flags & 0x2)) {
            for (i = 0; i < outputFrames; i++) {
                UINT32 sourceFrame = (UINT32)((float)i * speed);
                if (sourceFrame >= writtenFrames) sourceFrame = writtenFrames - 1;
                if (sourceFrame != i) {
                    memmove(snapshot.buffer + ((SIZE_T)i * snapshot.block_align),
                            snapshot.buffer + ((SIZE_T)sourceFrame * snapshot.block_align),
                            snapshot.block_align);
                }
            }
        }
        InterlockedOr(&g_audio_status, AUDIO_STATUS_WASAPI_BUFFER_SCALED);
    }

    if (!real_RC_ReleaseBuffer) return E_FAIL;
    return real_RC_ReleaseBuffer(this, outputFrames, flags);
}

static SpatialObjectRec *spatial_find_object(void *object, int create) {
    int i;
    SpatialObjectRec *empty = NULL;
    for (i = 0; i < MAX_SPATIAL_OBJECTS; i++) {
        if (g_spatial_objects[i].used && g_spatial_objects[i].object == object)
            return &g_spatial_objects[i];
        if (!g_spatial_objects[i].used && !empty) empty = &g_spatial_objects[i];
    }
    if (create && empty) {
        ZeroMemory(empty, sizeof(*empty));
        empty->used = 1;
        empty->object = object;
        return empty;
    }
    return NULL;
}

static void spatial_scale_nearest(SpatialObjectRec *record) {
    UINT32 outputFrame;
    UINT32 sourceFrame;
    UINT32 blockAlign;
    if (!record || !g_spatial_actual_frames || !g_spatial_fake_frames) return;
    blockAlign = record->actual_bytes / g_spatial_actual_frames;
    if (!blockAlign || record->shadow_bytes < blockAlign) return;
    for (outputFrame = 0; outputFrame < g_spatial_actual_frames; outputFrame++) {
        sourceFrame = (UINT32)(((uint64_t)outputFrame * g_spatial_fake_frames) /
                               g_spatial_actual_frames);
        if (sourceFrame >= g_spatial_fake_frames) sourceFrame = g_spatial_fake_frames - 1;
        memcpy(record->actual_buffer + ((SIZE_T)outputFrame * blockAlign),
               record->shadow_buffer + ((SIZE_T)sourceFrame * blockAlign),
               blockAlign);
    }
}

#ifdef CE_USE_SOUNDTOUCH
static int spatial_ensure_interleaved_capacity(SIZE_T inputSamples, SIZE_T outputSamples) {
    float *newBuffer;
    SIZE_T inputBytes = inputSamples * sizeof(float);
    SIZE_T outputBytes = outputSamples * sizeof(float);
    if (g_spatial_input_capacity < inputSamples) {
        if (g_spatial_interleaved_input) {
            newBuffer = (float *)HeapReAlloc(GetProcessHeap(), 0,
                                             g_spatial_interleaved_input, inputBytes);
        } else {
            newBuffer = (float *)HeapAlloc(GetProcessHeap(), 0, inputBytes);
        }
        if (!newBuffer) return 0;
        g_spatial_interleaved_input = newBuffer;
        g_spatial_input_capacity = inputSamples;
    }
    if (g_spatial_output_capacity < outputSamples) {
        if (g_spatial_interleaved_output) {
            newBuffer = (float *)HeapReAlloc(GetProcessHeap(), 0,
                                             g_spatial_interleaved_output, outputBytes);
        } else {
            newBuffer = (float *)HeapAlloc(GetProcessHeap(), 0, outputBytes);
        }
        if (!newBuffer) return 0;
        g_spatial_interleaved_output = newBuffer;
        g_spatial_output_capacity = outputSamples;
    }
    return 1;
}
#endif

static void spatial_scale_pending_buffers(void) {
    SpatialObjectRec *active[MAX_SPATIAL_OBJECTS];
    int activeCount = 0;
    int i;
    int processed = 0;
    LONG generation = InterlockedCompareExchange(&g_spatial_generation, 0, 0);

    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_SPATIAL_OBJECTS; i++) {
        SpatialObjectRec *record = &g_spatial_objects[i];
        if (record->used && record->generation == generation &&
            record->actual_buffer && record->actual_bytes &&
            record->shadow_buffer && record->shadow_bytes &&
            g_spatial_actual_frames && g_spatial_fake_frames &&
            g_spatial_actual_frames != g_spatial_fake_frames) {
            active[activeCount++] = record;
        }
    }

#ifdef CE_USE_SOUNDTOUCH
    if (activeCount > 0) {
        SIZE_T inputSamples = (SIZE_T)g_spatial_fake_frames * activeCount;
        SIZE_T outputSamples = (SIZE_T)g_spatial_actual_frames * activeCount;
        UINT32 frame;
        UINT32 channel;
        UINT32 received;
        float tempo = (float)g_spatial_fake_frames / (float)g_spatial_actual_frames;
        int compatible = 1;

        for (i = 0; i < activeCount; i++) {
            if (active[i]->actual_bytes != g_spatial_actual_frames * sizeof(float) ||
                active[i]->shadow_bytes != g_spatial_fake_frames * sizeof(float)) {
                compatible = 0;
                break;
            }
        }
        if (compatible && spatial_ensure_interleaved_capacity(inputSamples, outputSamples)) {
            if (!g_spatial_soundtouch || g_spatial_soundtouch_channels != (UINT32)activeCount) {
                if (g_spatial_soundtouch) ce_soundtouch_destroy(g_spatial_soundtouch);
                g_spatial_soundtouch = ce_soundtouch_create(48000, (UINT32)activeCount);
                g_spatial_soundtouch_channels = g_spatial_soundtouch ? (UINT32)activeCount : 0;
            }
            if (g_spatial_soundtouch) {
                for (frame = 0; frame < g_spatial_fake_frames; frame++) {
                    for (channel = 0; channel < (UINT32)activeCount; channel++) {
                        g_spatial_interleaved_input[(SIZE_T)frame * activeCount + channel] =
                            ((float *)active[channel]->shadow_buffer)[frame];
                    }
                }
                ZeroMemory(g_spatial_interleaved_output, outputSamples * sizeof(float));
                received = ce_soundtouch_process(
                    g_spatial_soundtouch,
                    g_spatial_interleaved_input,
                    g_spatial_fake_frames,
                    tempo,
                    g_spatial_interleaved_output,
                    g_spatial_actual_frames);
                for (channel = 0; channel < (UINT32)activeCount; channel++) {
                    float *destination = (float *)active[channel]->actual_buffer;
                    ZeroMemory(destination, active[channel]->actual_bytes);
                    for (frame = 0; frame < received; frame++) {
                        destination[frame] =
                            g_spatial_interleaved_output[(SIZE_T)frame * activeCount + channel];
                    }
                }
                processed = 1;
                if (received > 0)
                    InterlockedOr(&g_audio_status, AUDIO_STATUS_SPATIAL_TIMESTRETCHED);
            }
        }
    }
#endif

    for (i = 0; i < activeCount; i++) {
        if (!processed) spatial_scale_nearest(active[i]);
        active[i]->actual_buffer = NULL;
        active[i]->actual_bytes = 0;
        active[i]->shadow_bytes = 0;
    }
    if (activeCount > 0) InterlockedOr(&g_audio_status, AUDIO_STATUS_SPATIAL_BUFFER_SCALED);
    LeaveCriticalSection(&g_audio_lock);
}

static HRESULT STDMETHODCALLTYPE hook_SA_ObjectGetBuffer(void *this, BYTE **buffer, UINT32 *bytes) {
    HRESULT result;
    SpatialObjectRec *record;
    SIZE_T shadowBytes;
    BYTE *newBuffer;

    if (!real_SA_ObjectGetBuffer) return E_FAIL;
    result = real_SA_ObjectGetBuffer(this, buffer, bytes);
    if (FAILED(result) || !buffer || !*buffer || !bytes || !*bytes) return result;
    InterlockedOr(&g_audio_status, AUDIO_STATUS_SPATIAL_BUFFER_SEEN);
    if (!g_spatial_actual_frames || !g_spatial_fake_frames ||
        g_spatial_actual_frames == g_spatial_fake_frames) {
        return result;
    }

    shadowBytes = ((SIZE_T)(*bytes) * g_spatial_fake_frames) / g_spatial_actual_frames;
    if (!shadowBytes) shadowBytes = 1;
    EnterCriticalSection(&g_audio_lock);
    record = spatial_find_object(this, 1);
    if (record) {
        if (record->shadow_capacity < shadowBytes) {
            if (record->shadow_buffer) {
                newBuffer = (BYTE *)HeapReAlloc(GetProcessHeap(), 0,
                                                record->shadow_buffer, shadowBytes);
            } else {
                newBuffer = (BYTE *)HeapAlloc(GetProcessHeap(), 0, shadowBytes);
            }
            if (newBuffer) {
                record->shadow_buffer = newBuffer;
                record->shadow_capacity = shadowBytes;
            }
        }
        if (record->shadow_buffer && record->shadow_capacity >= shadowBytes) {
            record->actual_buffer = *buffer;
            record->actual_bytes = *bytes;
            record->shadow_bytes = (UINT32)shadowBytes;
            record->generation = InterlockedCompareExchange(&g_spatial_generation, 0, 0);
            ZeroMemory(record->shadow_buffer, shadowBytes);
            *buffer = record->shadow_buffer;
            *bytes = (UINT32)shadowBytes;
        }
    }
    LeaveCriticalSection(&g_audio_lock);
    return result;
}

static HRESULT STDMETHODCALLTYPE hook_SA_BeginUpdate(
    void *this, UINT32 *availableObjects, UINT32 *frames) {
    HRESULT result;
    UINT32 actualFrames;
    UINT32 fakeFrames;
    float speed = current_speed();
    if (!real_SA_BeginUpdate) return E_FAIL;
    result = real_SA_BeginUpdate(this, availableObjects, frames);
    if (FAILED(result) || !frames || !*frames) return result;
    actualFrames = *frames;
    fakeFrames = (UINT32)((float)actualFrames * speed + 0.5f);
    if (fakeFrames < 1) fakeFrames = 1;
    if (fakeFrames > actualFrames * 100u) fakeFrames = actualFrames * 100u;
    EnterCriticalSection(&g_audio_lock);
#ifdef CE_USE_SOUNDTOUCH
    if (fabsf(speed - g_spatial_soundtouch_last_speed) > 0.001f) {
        if (g_spatial_soundtouch) ce_soundtouch_clear(g_spatial_soundtouch);
        g_spatial_soundtouch_last_speed = speed;
    }
#endif
    g_spatial_actual_frames = actualFrames;
    g_spatial_fake_frames = fakeFrames;
    InterlockedIncrement(&g_spatial_generation);
    LeaveCriticalSection(&g_audio_lock);
    *frames = fakeFrames;
    return result;
}

static HRESULT STDMETHODCALLTYPE hook_SA_EndUpdate(void *this) {
    if (!real_SA_EndUpdate) return E_FAIL;
    spatial_scale_pending_buffers();
    return real_SA_EndUpdate(this);
}

static void hook_spatial_object_methods(void *object) {
    void **vtable;
    if (!object) return;
    vtable = *(void ***)object;
    if (!vtable) return;
    hook_vtable_slot(&vtable[SA_OBJECT_VT_GET_BUFFER],
                     (void *)hook_SA_ObjectGetBuffer,
                     (void **)&real_SA_ObjectGetBuffer);
}

static HRESULT STDMETHODCALLTYPE hook_SA_ActivateObject(
    void *this, UINT32 objectType, void **object) {
    HRESULT result;
    if (!real_SA_ActivateObject) return E_FAIL;
    result = real_SA_ActivateObject(this, objectType, object);
    if (SUCCEEDED(result) && object && *object) hook_spatial_object_methods(*object);
    return result;
}

static void hook_spatial_stream_methods(void *stream) {
    void **vtable;
    if (!stream) return;
    vtable = *(void ***)stream;
    if (!vtable) return;
    hook_vtable_slot(&vtable[SA_STREAM_VT_BEGIN_UPDATE],
                     (void *)hook_SA_BeginUpdate, (void **)&real_SA_BeginUpdate);
    hook_vtable_slot(&vtable[SA_STREAM_VT_END_UPDATE],
                     (void *)hook_SA_EndUpdate, (void **)&real_SA_EndUpdate);
    hook_vtable_slot(&vtable[SA_STREAM_VT_ACTIVATE_OBJECT],
                     (void *)hook_SA_ActivateObject, (void **)&real_SA_ActivateObject);
}

static HRESULT STDMETHODCALLTYPE hook_SA_ActivateStream(
    void *this, const void *params, REFIID iid, void **stream) {
    HRESULT result;
    if (!real_SA_ActivateStream) return E_FAIL;
    result = real_SA_ActivateStream(this, params, iid, stream);
    if (SUCCEEDED(result) && stream && *stream) hook_spatial_stream_methods(*stream);
    return result;
}

static void hook_spatial_client_methods(void *client) {
    void **vtable;
    if (!client) return;
    vtable = *(void ***)client;
    if (!vtable) return;
    hook_vtable_slot(&vtable[SA_CLIENT_VT_ACTIVATE_STREAM],
                     (void *)hook_SA_ActivateStream, (void **)&real_SA_ActivateStream);
}

static int enable_method_hook(void *target, void *hook, void **original) {
    MH_STATUS createStatus;
    MH_STATUS enableStatus;
    if (!target || !hook || !original || !g_mh_inited) return 0;
    createStatus = MH_CreateHook(target, hook, original);
    if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) return 0;
    enableStatus = MH_EnableHook(target);
    return enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED;
}

static void install_spatial_fixed_hooks(HMODULE audioSesModule) {
#ifdef _WIN64
    int hooked = 0;
    PIMAGE_DOS_HEADER dosHeader;
    PIMAGE_NT_HEADERS ntHeaders;
    if (!audioSesModule || !g_mh_inited) return;
    InterlockedOr(&g_audio_status, AUDIO_STATUS_SPATIAL_FOUND);
    dosHeader = (PIMAGE_DOS_HEADER)audioSesModule;
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) return;
    ntHeaders = (PIMAGE_NT_HEADERS)((BYTE *)audioSesModule + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE ||
        ntHeaders->OptionalHeader.SizeOfImage != 0x1c2000 ||
        ntHeaders->FileHeader.TimeDateStamp != 0x160d43dc) {
        return;
    }
    /* Late-injection RVAs validated against AudioSes 10.0.26100.7705. */
    hooked += enable_method_hook((BYTE *)audioSesModule + 0x10bbd0,
                                 (void *)hook_SA_ActivateStream,
                                 (void **)&real_SA_ActivateStream);
    hooked += enable_method_hook((BYTE *)audioSesModule + 0x31060,
                                 (void *)hook_SA_BeginUpdate,
                                 (void **)&real_SA_BeginUpdate);
    hooked += enable_method_hook((BYTE *)audioSesModule + 0x5f330,
                                 (void *)hook_SA_EndUpdate,
                                 (void **)&real_SA_EndUpdate);
    hooked += enable_method_hook((BYTE *)audioSesModule + 0x119da0,
                                 (void *)hook_SA_ActivateObject,
                                 (void **)&real_SA_ActivateObject);
    hooked += enable_method_hook((BYTE *)audioSesModule + 0x54600,
                                 (void *)hook_SA_ObjectGetBuffer,
                                 (void **)&real_SA_ObjectGetBuffer);
    if (hooked == 5) InterlockedOr(&g_audio_status, AUDIO_STATUS_SPATIAL_HOOKED);
#else
    (void)audioSesModule;
#endif
}

static HRESULT STDMETHODCALLTYPE hook_AC_Initialize(
    void *this, int shareMode, DWORD streamFlags, LONGLONG bufferDuration,
    LONGLONG periodicity, const WAVEFORMATEX *format, LPCGUID sessionGuid);
static HRESULT STDMETHODCALLTYPE hook_AC_GetService(void *this, REFIID iid, void **service);

static void hook_audio_client_methods(void *audioClient) {
    void **vtable;
    if (!audioClient) return;
    vtable = *(void ***)audioClient;
    if (!vtable) return;
    hook_vtable_slot(&vtable[AC_VT_INITIALIZE], (void *)hook_AC_Initialize, (void **)&real_AC_Initialize);
    hook_vtable_slot(&vtable[AC_VT_GET_SERVICE], (void *)hook_AC_GetService, (void **)&real_AC_GetService);
}

static HRESULT STDMETHODCALLTYPE hook_AC_Initialize(
    void *this, int shareMode, DWORD streamFlags, LONGLONG bufferDuration,
    LONGLONG periodicity, const WAVEFORMATEX *format, LPCGUID sessionGuid) {
    if (format && format->nBlockAlign) g_wasapi_block_align = format->nBlockAlign;
    if (!real_AC_Initialize) return E_FAIL;
    return real_AC_Initialize(this, shareMode, streamFlags, bufferDuration,
                              periodicity, format, sessionGuid);
}

static HRESULT STDMETHODCALLTYPE hook_AC_GetService(void *this, REFIID iid, void **service) {
    HRESULT result;
    void **vtable;
    if (!real_AC_GetService) return E_FAIL;
    result = real_AC_GetService(this, iid, service);
    if (SUCCEEDED(result) && service && *service && IsEqualGUID(iid, &ce_iid_audio_render_client)) {
        vtable = *(void ***)*service;
        if (vtable) {
            hook_vtable_slot(&vtable[RC_VT_GET_BUFFER], (void *)hook_RC_GetBuffer, (void **)&real_RC_GetBuffer);
            hook_vtable_slot(&vtable[RC_VT_RELEASE_BUFFER], (void *)hook_RC_ReleaseBuffer, (void **)&real_RC_ReleaseBuffer);
            if (vtable[RC_VT_GET_BUFFER] == (void *)hook_RC_GetBuffer &&
                vtable[RC_VT_RELEASE_BUFFER] == (void *)hook_RC_ReleaseBuffer) {
                g_hooked_wasapi = 1;
                InterlockedOr(&g_audio_status, AUDIO_STATUS_WASAPI_HOOKED);
            }
        }
    }
    return result;
}

static HRESULT STDMETHODCALLTYPE hook_MM_Activate(
    void *this, REFIID iid, DWORD clsctx, void *activationParams, void **interfaceOut) {
    HRESULT result;
    if (!real_MM_Activate) return E_FAIL;
    result = real_MM_Activate(this, iid, clsctx, activationParams, interfaceOut);
    if (SUCCEEDED(result) && interfaceOut && *interfaceOut) {
        if (IsEqualGUID(iid, &ce_iid_audio_client))
            hook_audio_client_methods(*interfaceOut);
        else if (IsEqualGUID(iid, &ce_iid_spatial_audio_client))
            hook_spatial_client_methods(*interfaceOut);
    }
    return result;
}

static void bootstrap_wasapi(void) {
    HRESULT coResult;
    HRESULT result;
    int uninitializeCom;
    void *enumerator = NULL;
    void *device = NULL;
    void *audioClient = NULL;
    void *renderClient = NULL;
    void **vtable;
    WAVEFORMATEX *mixFormat = NULL;
    MM_GetDefaultAudioEndpoint_t getDefaultEndpoint;
    MM_Activate_t activate;
    AC_GetMixFormat_t getMixFormat;
    AC_Initialize_t initialize;
    AC_GetService_t getService;
    XA_Release_t releaseObject;
    HMODULE methodModule = NULL;
    HMODULE audioSesModule;
    void *getBufferTarget;
    void *releaseBufferTarget;
    MH_STATUS getHookStatus;
    MH_STATUS releaseHookStatus;

    InterlockedOr(&g_audio_status, AUDIO_STATUS_WASAPI_FOUND);
    audioSesModule = GetModuleHandleW(L"audioses.dll");
    if (!g_mh_inited && MH_Initialize() == MH_OK) g_mh_inited = 1;
    if (audioSesModule) install_spatial_fixed_hooks(audioSesModule);
    if (g_hooked_wasapi) return;
    coResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    uninitializeCom = SUCCEEDED(coResult);
    result = CoCreateInstance(&ce_clsid_mmdevice_enumerator, NULL, 23,
                              &ce_iid_mmdevice_enumerator, &enumerator);
    if (FAILED(result) || !enumerator) goto cleanup;
    vtable = *(void ***)enumerator;
    getDefaultEndpoint = (MM_GetDefaultAudioEndpoint_t)vtable[MM_VT_GET_DEFAULT_AUDIO_ENDPOINT];
    result = getDefaultEndpoint(enumerator, 0, 0, &device);
    if (FAILED(result) || !device) goto cleanup;
    vtable = *(void ***)device;
    hook_vtable_slot(&vtable[DEVICE_VT_ACTIVATE], (void *)hook_MM_Activate, (void **)&real_MM_Activate);
    activate = real_MM_Activate ? real_MM_Activate : (MM_Activate_t)vtable[DEVICE_VT_ACTIVATE];
    result = activate(device, &ce_iid_audio_client, 23, NULL, &audioClient);
    if (FAILED(result) || !audioClient) goto cleanup;
    hook_audio_client_methods(audioClient);
    vtable = *(void ***)audioClient;
    getMixFormat = (AC_GetMixFormat_t)vtable[AC_VT_GET_MIX_FORMAT];
    initialize = (AC_Initialize_t)vtable[AC_VT_INITIALIZE];
    getService = (AC_GetService_t)vtable[AC_VT_GET_SERVICE];
    result = getMixFormat(audioClient, &mixFormat);
    if (FAILED(result) || !mixFormat) goto cleanup;
    g_wasapi_block_align = mixFormat->nBlockAlign;
    result = initialize(audioClient, 0, 0, 1000000, 0, mixFormat, NULL);
    if (FAILED(result)) goto cleanup;
    result = getService(audioClient, &ce_iid_audio_render_client, &renderClient);
    if (FAILED(result) || !renderClient) goto cleanup;
    vtable = *(void ***)renderClient;
    getBufferTarget = vtable[RC_VT_GET_BUFFER];
    releaseBufferTarget = vtable[RC_VT_RELEASE_BUFFER];
    audioSesModule = GetModuleHandleW(L"audioses.dll");
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                       GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                       (LPCWSTR)getBufferTarget, &methodModule);
#ifdef _WIN64
    if (audioSesModule && methodModule != audioSesModule) {
        getBufferTarget = (BYTE *)audioSesModule + 0x69460;
        releaseBufferTarget = (BYTE *)audioSesModule + 0x156a0;
    }
#endif
    if (!g_mh_inited && MH_Initialize() == MH_OK) g_mh_inited = 1;
    if (audioSesModule) install_spatial_fixed_hooks(audioSesModule);
    getHookStatus = g_mh_inited
        ? MH_CreateHook(getBufferTarget, (void *)hook_RC_GetBuffer, (void **)&real_RC_GetBuffer)
        : MH_ERROR_NOT_INITIALIZED;
    releaseHookStatus = g_mh_inited
        ? MH_CreateHook(releaseBufferTarget, (void *)hook_RC_ReleaseBuffer, (void **)&real_RC_ReleaseBuffer)
        : MH_ERROR_NOT_INITIALIZED;
    if (getHookStatus == MH_OK) getHookStatus = MH_EnableHook(getBufferTarget);
    if (releaseHookStatus == MH_OK) releaseHookStatus = MH_EnableHook(releaseBufferTarget);
    if (getHookStatus == MH_OK && releaseHookStatus == MH_OK) {
        g_hooked_wasapi = 1;
        InterlockedOr(&g_audio_status, AUDIO_STATUS_WASAPI_HOOKED);
    } else {
        hook_vtable_slot(&vtable[RC_VT_GET_BUFFER], (void *)hook_RC_GetBuffer, (void **)&real_RC_GetBuffer);
        hook_vtable_slot(&vtable[RC_VT_RELEASE_BUFFER], (void *)hook_RC_ReleaseBuffer, (void **)&real_RC_ReleaseBuffer);
        if (vtable[RC_VT_GET_BUFFER] == (void *)hook_RC_GetBuffer &&
            vtable[RC_VT_RELEASE_BUFFER] == (void *)hook_RC_ReleaseBuffer) {
            g_hooked_wasapi = 1;
            InterlockedOr(&g_audio_status, AUDIO_STATUS_WASAPI_HOOKED);
        }
    }

cleanup:
    if (renderClient) {
        vtable = *(void ***)renderClient;
        releaseObject = (XA_Release_t)vtable[2];
        releaseObject(renderClient);
    }
    if (audioClient) {
        vtable = *(void ***)audioClient;
        releaseObject = (XA_Release_t)vtable[2];
        releaseObject(audioClient);
    }
    if (device) {
        vtable = *(void ***)device;
        releaseObject = (XA_Release_t)vtable[2];
        releaseObject(device);
    }
    if (enumerator) {
        vtable = *(void ***)enumerator;
        releaseObject = (XA_Release_t)vtable[2];
        releaseObject(enumerator);
    }
    if (mixFormat) CoTaskMemFree(mixFormat);
    if (uninitializeCom) CoUninitialize();
}

static GetTickCount_t resolve_real_gtc(void) {
    if (realGetTickCount) return realGetTickCount;
    if (realgettickcount) return realgettickcount;
    return (GetTickCount_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetTickCount");
}
static GetTickCount64_t resolve_real_gtc64(void) {
    if (realGetTickCount64) return realGetTickCount64;
    if (realgettickcount64) return realgettickcount64;
    HMODULE k = GetModuleHandleW(L"kernel32.dll");
    return k ? (GetTickCount64_t)GetProcAddress(k, "GetTickCount64") : NULL;
}
static QueryPerformanceCounter_t resolve_real_qpc(void) {
    if (realQueryPerformanceCounter) return realQueryPerformanceCounter;
    if (realqueryperformancecounter) return realqueryperformancecounter;
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (ntdll) {
        QueryPerformanceCounter_t p = (QueryPerformanceCounter_t)GetProcAddress(ntdll, "RtlQueryPerformanceCounter");
        if (p) return p;
    }
    return (QueryPerformanceCounter_t)GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "QueryPerformanceCounter");
}

__declspec(dllexport) DWORD WINAPI speedhackversion_GetTickCount(void) {
    GetTickCount_t real = resolve_real_gtc();
    DWORD now = real ? real() : GetTickCount();
    DWORD result;
    EnterCriticalSection(&g_lock);
    result = (DWORD)((double)(now - g_initial_time_gtc) * current_speed()) + g_initial_offset_gtc;
    LeaveCriticalSection(&g_lock);
    return result;
}
__declspec(dllexport) ULONGLONG WINAPI speedhackversion_GetTickCount64(void) {
    GetTickCount64_t real = resolve_real_gtc64();
    ULONGLONG now = real ? real() : GetTickCount64();
    ULONGLONG result;
    EnterCriticalSection(&g_lock);
    result = (ULONGLONG)((double)(now - g_initial_time_gtc64) * current_speed()) + g_initial_offset_gtc64;
    LeaveCriticalSection(&g_lock);
    return result;
}
__declspec(dllexport) BOOL WINAPI speedhackversion_QueryPerformanceCounter(LARGE_INTEGER *x) {
    QueryPerformanceCounter_t real = resolve_real_qpc();
    LARGE_INTEGER cur; BOOL ok; LONGLONG result;
    if (!real) return QueryPerformanceCounter(x);
    ok = real(&cur);
    if (!ok) return ok;
    EnterCriticalSection(&g_lock);
    result = (LONGLONG)((double)(cur.QuadPart - g_initial_time_qpc) * current_speed()) + g_initial_offset_qpc;
    LeaveCriticalSection(&g_lock);
    if (x) x->QuadPart = result;
    return ok;
}

static int create_timing_hook(void *target, void *hook, void **original) {
    MH_STATUS status;
    if (!target) return 0;
    status = MH_CreateHook(target, hook, original);
    if (status != MH_OK) return 0;
    status = MH_EnableHook(target);
    return status == MH_OK || status == MH_ERROR_ENABLED;
}

static int install_timing_hooks(void) {
    HMODULE kernel32;
    HMODULE kernelbase;
    HMODULE ntdll;
    HMODULE winmm;
    void *getTickCount = NULL;
    void *getTickCount64 = NULL;
    void *queryPerformanceCounter = NULL;
    void *timeGetTimeAddress = NULL;
    int hooked = 0;

    ensure_cs();
    if (g_timing_hooks_ready) return 1;
    if (!g_mh_inited) {
        if (MH_Initialize() != MH_OK) return 0;
        g_mh_inited = 1;
    }

    kernel32 = GetModuleHandleW(L"kernel32.dll");
    kernelbase = GetModuleHandleW(L"kernelbase.dll");
    ntdll = GetModuleHandleW(L"ntdll.dll");
    winmm = GetModuleHandleW(L"winmm.dll");
    if (!winmm) winmm = LoadLibraryW(L"winmm.dll");

    if (kernel32) {
        getTickCount = (void *)GetProcAddress(kernel32, "GetTickCount");
        getTickCount64 = (void *)GetProcAddress(kernel32, "GetTickCount64");
    }
    if (!getTickCount && kernelbase)
        getTickCount = (void *)GetProcAddress(kernelbase, "GetTickCount");
    if (!getTickCount64 && kernelbase)
        getTickCount64 = (void *)GetProcAddress(kernelbase, "GetTickCount64");
    if (ntdll)
        queryPerformanceCounter = (void *)GetProcAddress(ntdll, "RtlQueryPerformanceCounter");
    if (!queryPerformanceCounter && kernel32)
        queryPerformanceCounter = (void *)GetProcAddress(kernel32, "QueryPerformanceCounter");
    if (winmm)
        timeGetTimeAddress = (void *)GetProcAddress(winmm, "timeGetTime");

    hooked |= create_timing_hook(
        getTickCount, (void *)speedhackversion_GetTickCount, (void **)&realGetTickCount);
    hooked |= create_timing_hook(
        getTickCount64, (void *)speedhackversion_GetTickCount64, (void **)&realGetTickCount64);
    hooked |= create_timing_hook(
        queryPerformanceCounter,
        (void *)speedhackversion_QueryPerformanceCounter,
        (void **)&realQueryPerformanceCounter);
    hooked |= create_timing_hook(
        timeGetTimeAddress, (void *)speedhackversion_GetTickCount, (void **)&realgettime);

    g_timing_hooks_ready = hooked;
    return hooked;
}

static void xa_add_voice(void *voice, float base_ratio, float max_ratio) {
    int i; if (!voice) return;
    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_XA_VOICES; i++) {
        if (g_xa_voices[i].used && g_xa_voices[i].voice == voice) {
            g_xa_voices[i].base_ratio = base_ratio;
            if (max_ratio > 0.0f) g_xa_voices[i].max_ratio = max_ratio;
            LeaveCriticalSection(&g_audio_lock);
            return;
        }
    }
    for (i = 0; i < MAX_XA_VOICES; i++) {
        if (!g_xa_voices[i].used) {
            g_xa_voices[i].voice = voice;
            g_xa_voices[i].base_ratio = base_ratio;
            g_xa_voices[i].max_ratio = max_ratio > 0.0f ? max_ratio : 2.0f;
            g_xa_voices[i].used = 1;
            break;
        }
    }
    LeaveCriticalSection(&g_audio_lock);
}
static void xa_remove_voice(void *voice) {
    int i; EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_XA_VOICES; i++) if (g_xa_voices[i].used && g_xa_voices[i].voice == voice) { g_xa_voices[i].used = 0; g_xa_voices[i].voice = NULL; break; }
    LeaveCriticalSection(&g_audio_lock);
}
static void xa_set_base(void *voice, float base) {
    int i; EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_XA_VOICES; i++) if (g_xa_voices[i].used && g_xa_voices[i].voice == voice) { g_xa_voices[i].base_ratio = base; break; }
    LeaveCriticalSection(&g_audio_lock);
}
static float xa_scaled_ratio(void *voice, float base, float speed) {
    int i; float max_ratio = 2.0f, result;
    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_XA_VOICES; i++) {
        if (g_xa_voices[i].used && g_xa_voices[i].voice == voice) {
            max_ratio = g_xa_voices[i].max_ratio;
            break;
        }
    }
    LeaveCriticalSection(&g_audio_lock);
    result = base * speed;
    if (result > max_ratio) result = max_ratio;
    if (result < 0.0009765625f) result = 0.0009765625f;
    return result;
}
static void ds_add_buffer(void *buffer, DWORD base_freq) {
    int i; if (!buffer) return;
    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_DS_BUFFERS; i++) {
        if (g_ds_buffers[i].used && g_ds_buffers[i].buffer == buffer) { g_ds_buffers[i].base_freq = base_freq; LeaveCriticalSection(&g_audio_lock); return; }
    }
    for (i = 0; i < MAX_DS_BUFFERS; i++) {
        if (!g_ds_buffers[i].used) { g_ds_buffers[i].buffer = buffer; g_ds_buffers[i].base_freq = base_freq ? base_freq : 44100; g_ds_buffers[i].used = 1; break; }
    }
    LeaveCriticalSection(&g_audio_lock);
}
static void ds_remove_buffer(void *buffer) {
    int i; EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_DS_BUFFERS; i++) if (g_ds_buffers[i].used && g_ds_buffers[i].buffer == buffer) { g_ds_buffers[i].used = 0; g_ds_buffers[i].buffer = NULL; break; }
    LeaveCriticalSection(&g_audio_lock);
}
static void wave_add(HWAVEOUT h, DWORD rate, DWORD pitch) {
    int i; if (!h) return;
    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_WAVE_OUT; i++) {
        if (g_wave_outs[i].used && g_wave_outs[i].handle == h) { g_wave_outs[i].base_rate = rate; g_wave_outs[i].base_pitch = pitch; LeaveCriticalSection(&g_audio_lock); return; }
    }
    for (i = 0; i < MAX_WAVE_OUT; i++) {
        if (!g_wave_outs[i].used) { g_wave_outs[i].handle = h; g_wave_outs[i].base_rate = rate ? rate : 0x10000; g_wave_outs[i].base_pitch = pitch ? pitch : 0x10000; g_wave_outs[i].used = 1; break; }
    }
    LeaveCriticalSection(&g_audio_lock);
}
static void wave_remove(HWAVEOUT h) {
    int i; EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_WAVE_OUT; i++) if (g_wave_outs[i].used && g_wave_outs[i].handle == h) { g_wave_outs[i].used = 0; g_wave_outs[i].handle = NULL; break; }
    LeaveCriticalSection(&g_audio_lock);
}
static DWORD scale_fixed16(DWORD base, float sp) {
    double v = (double)base * (double)sp; if (v < 1.0) v = 1.0; if (v > 4294967295.0) v = 4294967295.0; return (DWORD)v;
}
static DWORD scale_freq(DWORD base, float sp) {
    double v = (double)base * (double)sp; if (v < 100.0) v = 100.0; if (v > 100000.0) v = 100000.0; return (DWORD)v;
}
static void apply_audio_speed(float sp) {
    XAVoiceRec xaVoices[MAX_XA_VOICES];
    DSBufferRec dsBuffers[MAX_DS_BUFFERS];
    WaveOutRec waveOuts[MAX_WAVE_OUT];
    int i; sp = clamp_speed(sp);
    EnterCriticalSection(&g_audio_lock);
    memcpy(xaVoices, g_xa_voices, sizeof(xaVoices));
    memcpy(dsBuffers, g_ds_buffers, sizeof(dsBuffers));
    memcpy(waveOuts, g_wave_outs, sizeof(waveOuts));
    LeaveCriticalSection(&g_audio_lock);
    if (real_XA_SetFrequencyRatio) {
        for (i = 0; i < MAX_XA_VOICES; i++) if (xaVoices[i].used && xaVoices[i].voice) {
            HRESULT result;
            float ratio = xaVoices[i].base_ratio * sp;
            if (ratio > xaVoices[i].max_ratio) ratio = xaVoices[i].max_ratio;
            if (ratio < 0.0009765625f) ratio = 0.0009765625f;
            result = real_XA_SetFrequencyRatio(xaVoices[i].voice, ratio, 0);
            InterlockedExchange(&g_last_xaudio_result, result);
            if (SUCCEEDED(result)) InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_RATIO_APPLIED);
        }
    }
    if (real_DS_SetFrequency) {
        for (i = 0; i < MAX_DS_BUFFERS; i++) if (dsBuffers[i].used && dsBuffers[i].buffer)
            real_DS_SetFrequency(dsBuffers[i].buffer, scale_freq(dsBuffers[i].base_freq, sp));
    }
    for (i = 0; i < MAX_WAVE_OUT; i++) {
        if (!waveOuts[i].used || !waveOuts[i].handle) continue;
        if (real_waveOutSetPlaybackRate) real_waveOutSetPlaybackRate(waveOuts[i].handle, scale_fixed16(waveOuts[i].base_rate, sp));
        if (real_waveOutSetPitch) real_waveOutSetPitch(waveOuts[i].handle, scale_fixed16(waveOuts[i].base_pitch, sp));
    }
}

enum {
    XA_VT_DestroyVoice = 18,
    XA_VT_Start = 19,
    XA_VT_SubmitSourceBuffer = 21,
    XA_VT_SetFrequencyRatio = 26,
    XA_VT_GetFrequencyRatio = 27
};
enum {
    XA_VT_CreateSourceVoice28 = 5,
    XA_VT_CreateMasteringVoice28 = 7,
    XA_VT_CreateSourceVoice27 = 8,
    XA_VT_CreateMasteringVoice27 = 10
};
enum { DS_VT_CreateSoundBuffer = 3, DSBUF_VT_SetFrequency = 17, DSBUF_VT_Release = 2 };

static void xa_capture_voice(void *voice) {
    float ratio = 1.0f;
    if (!voice) return;
    InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_GAME_VOICE_SEEN);
    if (real_XA_GetFrequencyRatio) real_XA_GetFrequencyRatio(voice, &ratio);
    xa_add_voice(voice, ratio > 0.0f ? ratio : 1.0f, 2.0f);
    if (real_XA_SetFrequencyRatio)
        real_XA_SetFrequencyRatio(voice, xa_scaled_ratio(voice, ratio > 0.0f ? ratio : 1.0f, current_speed()), 0);
}
static HRESULT STDMETHODCALLTYPE hook_XA_SetFrequencyRatio(void *this, float ratio, UINT32 operationSet) {
    HRESULT result;
    xa_add_voice(this, ratio, 0.0f); xa_set_base(this, ratio);
    if (!real_XA_SetFrequencyRatio) return E_FAIL;
    result = real_XA_SetFrequencyRatio(this, xa_scaled_ratio(this, ratio, current_speed()), operationSet);
    InterlockedExchange(&g_last_xaudio_result, result);
    if (SUCCEEDED(result)) InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_RATIO_APPLIED);
    return result;
}
static void STDMETHODCALLTYPE hook_XA_DestroyVoice(void *this) {
    xa_remove_voice(this); if (real_XA_DestroyVoice) real_XA_DestroyVoice(this);
}
static HRESULT STDMETHODCALLTYPE hook_XA_Start(void *this, UINT32 flags, UINT32 operationSet) {
    xa_capture_voice(this);
    if (!real_XA_Start) return E_FAIL;
    return real_XA_Start(this, flags, operationSet);
}
static HRESULT STDMETHODCALLTYPE hook_XA_SubmitSourceBuffer(void *this, const void *buffer, const void *bufferWma) {
    xa_capture_voice(this);
    if (!real_XA_SubmitSourceBuffer) return E_FAIL;
    return real_XA_SubmitSourceBuffer(this, buffer, bufferWma);
}
static int hook_xa_voice_methods(void *voice) {
    void **vtbl; if (!voice) return 0; vtbl = *(void ***)voice; if (!vtbl) return 0;
    if (!real_XA_GetFrequencyRatio) real_XA_GetFrequencyRatio = (XA_GetFrequencyRatio_t)vtbl[XA_VT_GetFrequencyRatio];
    hook_vtable_slot(&vtbl[XA_VT_SetFrequencyRatio], (void *)hook_XA_SetFrequencyRatio, (void **)&real_XA_SetFrequencyRatio);
    hook_vtable_slot(&vtbl[XA_VT_DestroyVoice], (void *)hook_XA_DestroyVoice, (void **)&real_XA_DestroyVoice);
    hook_vtable_slot(&vtbl[XA_VT_Start], (void *)hook_XA_Start, (void **)&real_XA_Start);
    hook_vtable_slot(&vtbl[XA_VT_SubmitSourceBuffer], (void *)hook_XA_SubmitSourceBuffer, (void **)&real_XA_SubmitSourceBuffer);
    if (vtbl[XA_VT_SetFrequencyRatio] == (void *)hook_XA_SetFrequencyRatio)
        InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_VOICE_HOOKED);
    return 1;
}
static HRESULT STDMETHODCALLTYPE hook_XA_CreateSourceVoice_generic(
    XA_CreateSourceVoice_t real_fn, void *this, void **ppVoice, const void *pFormat,
    UINT32 flags, float maxFreqRatio, void *pCallback, void *pSendList, void *pEffectChain) {
    HRESULT hr; float requestedMax, formatMax = 100.0f; const WAVEFORMATEX *format = (const WAVEFORMATEX *)pFormat;
    if (!real_fn) return E_FAIL;
    InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_GAME_VOICE_SEEN);
    if (format && format->nSamplesPerSec) formatMax = 200000.0f / (float)format->nSamplesPerSec;
    if (formatMax > 100.0f) formatMax = 100.0f;
    if (formatMax < 1.0f) formatMax = 1.0f;
    requestedMax = maxFreqRatio < formatMax ? formatMax : maxFreqRatio;
    hr = real_fn(this, ppVoice, pFormat, flags, requestedMax, pCallback, pSendList, pEffectChain);
    if (SUCCEEDED(hr) && ppVoice && *ppVoice) {
        HRESULT setResult = E_FAIL;
        hook_xa_voice_methods(*ppVoice); xa_add_voice(*ppVoice, 1.0f, requestedMax);
        if (real_XA_SetFrequencyRatio)
            setResult = real_XA_SetFrequencyRatio(*ppVoice, xa_scaled_ratio(*ppVoice, 1.0f, current_speed()), 0);
        InterlockedExchange(&g_last_xaudio_result, setResult);
        if (SUCCEEDED(setResult)) InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_RATIO_APPLIED);
    }
    return hr;
}
static HRESULT STDMETHODCALLTYPE hook_XA_CreateSourceVoice28(void *this, void **ppVoice, const void *pFormat, UINT32 flags, float maxFreqRatio, void *pCallback, void *pSendList, void *pEffectChain) {
    return hook_XA_CreateSourceVoice_generic(real_XA_CreateSourceVoice_v28, this, ppVoice, pFormat, flags, maxFreqRatio, pCallback, pSendList, pEffectChain);
}
static HRESULT STDMETHODCALLTYPE hook_XA_CreateSourceVoice27(void *this, void **ppVoice, const void *pFormat, UINT32 flags, float maxFreqRatio, void *pCallback, void *pSendList, void *pEffectChain) {
    return hook_XA_CreateSourceVoice_generic(real_XA_CreateSourceVoice_v27, this, ppVoice, pFormat, flags, maxFreqRatio, pCallback, pSendList, pEffectChain);
}
static void hook_xa_engine_methods(void *xaudio2, int is_v28) {
    void **vtbl; int idx; if (!xaudio2) return; vtbl = *(void ***)xaudio2; if (!vtbl) return;
    idx = is_v28 ? XA_VT_CreateSourceVoice28 : XA_VT_CreateSourceVoice27;
    if (is_v28) {
        hook_vtable_slot(&vtbl[idx], (void *)hook_XA_CreateSourceVoice28, (void **)&real_XA_CreateSourceVoice_v28);
    } else {
        hook_vtable_slot(&vtbl[idx], (void *)hook_XA_CreateSourceVoice27, (void **)&real_XA_CreateSourceVoice_v27);
    }
    if ((is_v28 && vtbl[idx] == (void *)hook_XA_CreateSourceVoice28) ||
        (!is_v28 && vtbl[idx] == (void *)hook_XA_CreateSourceVoice27))
        InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_ENGINE_HOOKED);
}
static HRESULT WINAPI hook_XAudio2Create(void **ppXAudio2, UINT32 flags, UINT32 processor) {
    HRESULT hr; if (!real_XAudio2Create) return E_FAIL;
    hr = real_XAudio2Create(ppXAudio2, flags, processor);
    if (SUCCEEDED(hr) && ppXAudio2 && *ppXAudio2) hook_xa_engine_methods(*ppXAudio2, g_xaudio_is_modern);
    return hr;
}

static HRESULT STDMETHODCALLTYPE hook_DS_SetFrequency(void *this, DWORD freq) {
    int i; DWORD base = freq ? freq : 44100;
    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_DS_BUFFERS; i++) if (g_ds_buffers[i].used && g_ds_buffers[i].buffer == this) { g_ds_buffers[i].base_freq = base; break; }
    LeaveCriticalSection(&g_audio_lock);
    if (!real_DS_SetFrequency) return E_FAIL;
    return real_DS_SetFrequency(this, scale_freq(base, current_speed()));
}
static ULONG STDMETHODCALLTYPE hook_DS_Release(void *this) {
    ds_remove_buffer(this); if (!real_DS_Release) return 0; return real_DS_Release(this);
}
static void hook_ds_buffer_methods(void *buffer) {
    void **vtbl; if (!buffer) return; vtbl = *(void ***)buffer; if (!vtbl) return;
    hook_vtable_slot(&vtbl[DSBUF_VT_SetFrequency], (void *)hook_DS_SetFrequency, (void **)&real_DS_SetFrequency);
    hook_vtable_slot(&vtbl[DSBUF_VT_Release], (void *)hook_DS_Release, (void **)&real_DS_Release);
}
static HRESULT STDMETHODCALLTYPE hook_DS_CreateSoundBuffer(void *this, LPCDSBUFFERDESC desc, void **ppBuffer, void *pUnkOuter) {
    DSBUFFERDESC adjusted; LPCDSBUFFERDESC useDesc = desc;
    DWORD base = 44100; HRESULT hr; if (!real_DS_CreateSoundBuffer) return E_FAIL;
    if (desc && desc->dwSize >= sizeof(DSBUFFERDESC)) {
        adjusted = *desc;
        if (!(adjusted.dwFlags & DSBCAPS_PRIMARYBUFFER)) adjusted.dwFlags |= DSBCAPS_CTRLFREQUENCY;
        if (adjusted.lpwfxFormat && adjusted.lpwfxFormat->nSamplesPerSec)
            base = adjusted.lpwfxFormat->nSamplesPerSec;
        useDesc = &adjusted;
    }
    hr = real_DS_CreateSoundBuffer(this, useDesc, ppBuffer, pUnkOuter);
    if (SUCCEEDED(hr) && ppBuffer && *ppBuffer) {
        hook_ds_buffer_methods(*ppBuffer); ds_add_buffer(*ppBuffer, base);
        if (real_DS_SetFrequency) real_DS_SetFrequency(*ppBuffer, scale_freq(base, current_speed()));
    }
    return hr;
}
static void hook_ds_device_methods(void *ds) {
    void **vtbl; if (!ds) return; vtbl = *(void ***)ds; if (!vtbl) return;
    hook_vtable_slot(&vtbl[DS_VT_CreateSoundBuffer], (void *)hook_DS_CreateSoundBuffer, (void **)&real_DS_CreateSoundBuffer);
}
static HRESULT WINAPI hook_DirectSoundCreate(void *guid, void **ppDS, void *unk) {
    HRESULT hr; if (!real_DirectSoundCreate) return E_FAIL;
    hr = real_DirectSoundCreate(guid, ppDS, unk);
    if (SUCCEEDED(hr) && ppDS && *ppDS) hook_ds_device_methods(*ppDS);
    return hr;
}
static HRESULT WINAPI hook_DirectSoundCreate8(void *guid, void **ppDS8, void *unk) {
    HRESULT hr; if (!real_DirectSoundCreate8) return E_FAIL;
    hr = real_DirectSoundCreate8(guid, ppDS8, unk);
    if (SUCCEEDED(hr) && ppDS8 && *ppDS8) hook_ds_device_methods(*ppDS8);
    return hr;
}
static MMRESULT WINAPI hook_waveOutOpen(LPHWAVEOUT phwo, UINT uDeviceID, LPCWAVEFORMATEX pwfx, DWORD_PTR dwCallback, DWORD_PTR dwInstance, DWORD fdwOpen) {
    MMRESULT r; if (!real_waveOutOpen) return MMSYSERR_ERROR;
    r = real_waveOutOpen(phwo, uDeviceID, pwfx, dwCallback, dwInstance, fdwOpen);
    if (r == MMSYSERR_NOERROR && phwo && *phwo) {
        wave_add(*phwo, 0x10000, 0x10000);
        if (real_waveOutSetPlaybackRate) real_waveOutSetPlaybackRate(*phwo, scale_fixed16(0x10000, current_speed()));
        if (real_waveOutSetPitch) real_waveOutSetPitch(*phwo, scale_fixed16(0x10000, current_speed()));
    }
    return r;
}
static MMRESULT WINAPI hook_waveOutClose(HWAVEOUT h) {
    wave_remove(h); if (!real_waveOutClose) return MMSYSERR_ERROR; return real_waveOutClose(h);
}
static MMRESULT WINAPI hook_waveOutWrite(HWAVEOUT h, LPWAVEHDR header, UINT size) {
    int i, found = 0;
    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_WAVE_OUT; i++) {
        if (g_wave_outs[i].used && g_wave_outs[i].handle == h) { found = 1; break; }
    }
    LeaveCriticalSection(&g_audio_lock);
    if (!found) {
        wave_add(h, 0x10000, 0x10000);
        if (real_waveOutSetPlaybackRate) real_waveOutSetPlaybackRate(h, scale_fixed16(0x10000, current_speed()));
        if (real_waveOutSetPitch) real_waveOutSetPitch(h, scale_fixed16(0x10000, current_speed()));
    }
    if (!real_waveOutWrite) return MMSYSERR_ERROR;
    return real_waveOutWrite(h, header, size);
}
static MMRESULT WINAPI hook_waveOutSetPlaybackRate(HWAVEOUT h, DWORD rate) {
    int i; DWORD base = rate ? rate : 0x10000;
    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_WAVE_OUT; i++) if (g_wave_outs[i].used && g_wave_outs[i].handle == h) { g_wave_outs[i].base_rate = base; break; }
    LeaveCriticalSection(&g_audio_lock);
    if (!real_waveOutSetPlaybackRate) return MMSYSERR_ERROR;
    return real_waveOutSetPlaybackRate(h, scale_fixed16(base, current_speed()));
}
static MMRESULT WINAPI hook_waveOutSetPitch(HWAVEOUT h, DWORD pitch) {
    int i; DWORD base = pitch ? pitch : 0x10000;
    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_WAVE_OUT; i++) if (g_wave_outs[i].used && g_wave_outs[i].handle == h) { g_wave_outs[i].base_pitch = base; break; }
    LeaveCriticalSection(&g_audio_lock);
    if (!real_waveOutSetPitch) return MMSYSERR_ERROR;
    return real_waveOutSetPitch(h, scale_fixed16(base, current_speed()));
}
static void bootstrap_xaudio(HMODULE mod, int is_modern) {
    void *engine = NULL, *master = NULL, *voice = NULL;
    HRESULT coResult;
    int uninitializeCom;
    WAVEFORMATEX fmt;
    void **vtbl;
    void **masterVtable;
    XA_CreateSourceVoice_t createVoice;
    XA_CreateMasteringVoiceModern_t createMasterModern;
    XA_CreateMasteringVoice27_t createMaster27;
    XA_DestroyVoice_t destroyMaster;
    XA_Release_t releaseEngine;
    coResult = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    uninitializeCom = SUCCEEDED(coResult);
    if (!real_XAudio2Create || FAILED(real_XAudio2Create(&engine, 0, 1)) || !engine) {
        if (uninitializeCom) CoUninitialize();
        return;
    }
    InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_ENGINE_CREATED);
    vtbl = *(void ***)engine;
    if (!vtbl) {
        if (uninitializeCom) CoUninitialize();
        return;
    }
    ZeroMemory(&fmt, sizeof(fmt));
    fmt.wFormatTag = WAVE_FORMAT_PCM;
    fmt.nChannels = 1;
    fmt.nSamplesPerSec = 44100;
    fmt.wBitsPerSample = 16;
    fmt.nBlockAlign = 2;
    fmt.nAvgBytesPerSec = 88200;
    if (is_modern) {
        createMasterModern = (XA_CreateMasteringVoiceModern_t)vtbl[XA_VT_CreateMasteringVoice28];
        if (!createMasterModern || FAILED(createMasterModern(engine, &master, 0, 0, 0, NULL, NULL, 0))) master = NULL;
    } else {
        createMaster27 = (XA_CreateMasteringVoice27_t)vtbl[XA_VT_CreateMasteringVoice27];
        if (!createMaster27 || FAILED(createMaster27(engine, &master, 0, 0, 0, 0, NULL))) master = NULL;
    }
    if (master) InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_MASTER_CREATED);
    createVoice = (XA_CreateSourceVoice_t)vtbl[is_modern ? XA_VT_CreateSourceVoice28 : XA_VT_CreateSourceVoice27];
    if (master && createVoice && SUCCEEDED(createVoice(engine, &voice, &fmt, 0, 4.0f, NULL, NULL, NULL)) && voice) {
        InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_TEMP_VOICE_CREATED);
        hook_xa_voice_methods(voice);
        if (real_XA_DestroyVoice) real_XA_DestroyVoice(voice);
    }
    if (master) {
        masterVtable = *(void ***)master;
        destroyMaster = masterVtable ? (XA_DestroyVoice_t)masterVtable[XA_VT_DestroyVoice] : NULL;
        if (destroyMaster) destroyMaster(master);
    }
    hook_xa_engine_methods(engine, is_modern);
    releaseEngine = (XA_Release_t)vtbl[2];
    if (releaseEngine) releaseEngine(engine);
    if (uninitializeCom) CoUninitialize();
    (void)mod;
}
static void try_hook_xaudio_module(HMODULE mod, int is_modern) {
    FARPROC create; MH_STATUS status; if (!mod || g_hooked_xaudio2) return;
    create = GetProcAddress(mod, "XAudio2Create"); if (!create) return;
    InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_FOUND);
    g_xaudio_is_modern = is_modern;
    status = MH_CreateHook((LPVOID)create, (LPVOID)hook_XAudio2Create, (LPVOID *)&real_XAudio2Create);
    if (status == MH_OK) {
        MH_EnableHook((LPVOID)create);
        InterlockedOr(&g_audio_status, AUDIO_STATUS_XAUDIO_EXPORT_HOOKED);
    } else {
        real_XAudio2Create = (XAudio2Create_t)create;
    }
    g_hooked_xaudio2 = 1;
    bootstrap_xaudio(mod, is_modern);
}
static void bootstrap_dsound(void) {
    void *device = NULL;
    void **vtbl;
    DS_Release_t releaseDevice;
    if (!real_DirectSoundCreate || FAILED(real_DirectSoundCreate(NULL, &device, NULL)) || !device) return;
    vtbl = *(void ***)device;
    hook_ds_device_methods(device);
    releaseDevice = vtbl ? (DS_Release_t)vtbl[2] : NULL;
    if (releaseDevice) releaseDevice(device);
}
static void try_hook_dsound_module(HMODULE mod) {
    FARPROC p1, p8; MH_STATUS status; if (!mod || g_hooked_dsound) return;
    p1 = GetProcAddress(mod, "DirectSoundCreate"); p8 = GetProcAddress(mod, "DirectSoundCreate8");
    if (p1) {
        status = MH_CreateHook((LPVOID)p1, (LPVOID)hook_DirectSoundCreate, (LPVOID *)&real_DirectSoundCreate);
        if (status == MH_OK) MH_EnableHook((LPVOID)p1); else real_DirectSoundCreate = (DirectSoundCreate_t)p1;
    }
    if (p8) {
        status = MH_CreateHook((LPVOID)p8, (LPVOID)hook_DirectSoundCreate8, (LPVOID *)&real_DirectSoundCreate8);
        if (status == MH_OK) MH_EnableHook((LPVOID)p8); else real_DirectSoundCreate8 = (DirectSoundCreate8_t)p8;
    }
    if (p1 || p8) { g_hooked_dsound = 1; bootstrap_dsound(); }
}
static void try_hook_winmm_module(HMODULE mod) {
    FARPROC pOpen, pClose, pWrite, pRate, pPitch; if (!mod || g_hooked_winmm) return;
    pOpen = GetProcAddress(mod, "waveOutOpen"); pClose = GetProcAddress(mod, "waveOutClose");
    pWrite = GetProcAddress(mod, "waveOutWrite");
    pRate = GetProcAddress(mod, "waveOutSetPlaybackRate"); pPitch = GetProcAddress(mod, "waveOutSetPitch");
    if (pOpen && MH_CreateHook((LPVOID)pOpen, (LPVOID)hook_waveOutOpen, (LPVOID *)&real_waveOutOpen) == MH_OK) MH_EnableHook((LPVOID)pOpen);
    if (pClose && MH_CreateHook((LPVOID)pClose, (LPVOID)hook_waveOutClose, (LPVOID *)&real_waveOutClose) == MH_OK) MH_EnableHook((LPVOID)pClose);
    if (pWrite && MH_CreateHook((LPVOID)pWrite, (LPVOID)hook_waveOutWrite, (LPVOID *)&real_waveOutWrite) == MH_OK) MH_EnableHook((LPVOID)pWrite);
    if (pRate && MH_CreateHook((LPVOID)pRate, (LPVOID)hook_waveOutSetPlaybackRate, (LPVOID *)&real_waveOutSetPlaybackRate) == MH_OK) MH_EnableHook((LPVOID)pRate);
    if (pPitch && MH_CreateHook((LPVOID)pPitch, (LPVOID)hook_waveOutSetPitch, (LPVOID *)&real_waveOutSetPitch) == MH_OK) MH_EnableHook((LPVOID)pPitch);
    g_hooked_winmm = 1;
}
static int name_has(const wchar_t *name, const wchar_t *frag) {
    size_t nlen, flen, i, j; if (!name || !frag) return 0; nlen = wcslen(name); flen = wcslen(frag);
    for (i = 0; i + flen <= nlen; i++) {
        for (j = 0; j < flen; j++) {
            wchar_t a = name[i + j], b = frag[j];
            if (a >= L'A' && a <= L'Z') a = (wchar_t)(a - L'A' + L'a');
            if (b >= L'A' && b <= L'Z') b = (wchar_t)(b - L'A' + L'a');
            if (a != b) break;
        }
        if (j == flen) return 1;
    }
    return 0;
}
static void inspect_loaded_module(HMODULE mod, const wchar_t *name) {
    if (!mod || !name) return;
    if (name_has(name, L"xaudio2")) try_hook_xaudio_module(mod, !name_has(name, L"xaudio2_7"));
    if (name_has(name, L"dsound")) try_hook_dsound_module(mod);
    if (name_has(name, L"winmm")) try_hook_winmm_module(mod);
}
static void scan_already_loaded_audio(void) {
    static const wchar_t *xa_names[] = { L"XAudio2_9.dll", L"XAudio2_9redist.dll", L"XAudio2_8.dll", L"XAudio2_7.dll", NULL };
    HMODULE m; int i;
    for (i = 0; xa_names[i]; i++) {
        m = GetModuleHandleW(xa_names[i]);
        if (m) try_hook_xaudio_module(m, !name_has(xa_names[i], L"xaudio2_7"));
    }
    m = GetModuleHandleW(L"dsound.dll"); if (m) try_hook_dsound_module(m);
    m = GetModuleHandleW(L"winmm.dll"); if (!m) m = LoadLibraryW(L"winmm.dll"); if (m) try_hook_winmm_module(m);
    if (GetModuleHandleW(L"audioses.dll") || GetModuleHandleW(L"mmdevapi.dll")) bootstrap_wasapi();
}
static DWORD WINAPI audio_scan_thread(void *unused) {
    (void)unused;
    for (;;) {
        scan_already_loaded_audio();
        Sleep(500);
    }
}
static HMODULE WINAPI hook_LoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags) {
    HMODULE mod = real_LoadLibraryExW ? real_LoadLibraryExW(lpLibFileName, hFile, dwFlags) : NULL;
    if (mod && lpLibFileName) inspect_loaded_module(mod, lpLibFileName); return mod;
}
static HMODULE WINAPI hook_LoadLibraryW(LPCWSTR lpLibFileName) {
    HMODULE mod = real_LoadLibraryW ? real_LoadLibraryW(lpLibFileName) : NULL;
    if (mod && lpLibFileName) inspect_loaded_module(mod, lpLibFileName); return mod;
}
static HMODULE WINAPI hook_LoadLibraryA(LPCSTR lpLibFileName) {
    HMODULE mod = real_LoadLibraryA ? real_LoadLibraryA(lpLibFileName) : NULL;
    if (mod && lpLibFileName) { wchar_t wname[MAX_PATH]; MultiByteToWideChar(CP_ACP, 0, lpLibFileName, -1, wname, MAX_PATH); inspect_loaded_module(mod, wname); }
    return mod;
}
static void install_loader_hooks(void) {
    HMODULE k32 = GetModuleHandleW(L"kernel32.dll"); HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    void *pEx = NULL, *pW = NULL, *pA = NULL;
    if (kb) { pEx = (void *)GetProcAddress(kb, "LoadLibraryExW"); pW = (void *)GetProcAddress(kb, "LoadLibraryW"); pA = (void *)GetProcAddress(kb, "LoadLibraryA"); }
    if (!pEx && k32) pEx = (void *)GetProcAddress(k32, "LoadLibraryExW");
    if (!pW && k32) pW = (void *)GetProcAddress(k32, "LoadLibraryW");
    if (!pA && k32) pA = (void *)GetProcAddress(k32, "LoadLibraryA");
    if (pEx && MH_CreateHook(pEx, (LPVOID)hook_LoadLibraryExW, (LPVOID *)&real_LoadLibraryExW) == MH_OK) MH_EnableHook(pEx);
    if (pW && MH_CreateHook(pW, (LPVOID)hook_LoadLibraryW, (LPVOID *)&real_LoadLibraryW) == MH_OK) MH_EnableHook(pW);
    if (pA && MH_CreateHook(pA, (LPVOID)hook_LoadLibraryA, (LPVOID *)&real_LoadLibraryA) == MH_OK) MH_EnableHook(pA);
}
static void install_audio_hooks(void) {
    HANDLE thread;
    ensure_cs();
    if (!g_mh_inited) { if (MH_Initialize() != MH_OK) return; g_mh_inited = 1; }
    InterlockedOr(&g_audio_status, AUDIO_STATUS_MINHOOK_READY);
    if (!g_audio_hooks_ready) { install_loader_hooks(); g_audio_hooks_ready = 1; }
    scan_already_loaded_audio();
    if (InterlockedCompareExchange(&g_scan_thread_started, 1, 0) == 0) {
        thread = CreateThread(NULL, 0, audio_scan_thread, NULL, 0, NULL);
        if (thread) CloseHandle(thread); else InterlockedExchange(&g_scan_thread_started, 0);
    }
}

__declspec(dllexport) void __stdcall InitializeSpeedhack(float speed) {
    GetTickCount_t gtc; GetTickCount64_t gtc64; QueryPerformanceCounter_t qpc;
    float new_speed = clamp_speed(speed); float old_sp;
    ensure_cs(); install_audio_hooks();
    gtc = resolve_real_gtc(); gtc64 = resolve_real_gtc64(); qpc = resolve_real_qpc();
    EnterCriticalSection(&g_lock);
    old_sp = clamp_speed(g_speed);
    if (gtc) {
        DWORD real_now = gtc();
        DWORD fake_now = (DWORD)((double)(real_now - g_initial_time_gtc) * old_sp) + g_initial_offset_gtc;
        g_initial_offset_gtc = fake_now; g_initial_time_gtc = real_now;
    }
    if (qpc) {
        LARGE_INTEGER li; LONGLONG real_now, fake_now; qpc(&li); real_now = li.QuadPart;
        fake_now = (LONGLONG)((double)(real_now - g_initial_time_qpc) * old_sp) + g_initial_offset_qpc;
        g_initial_offset_qpc = fake_now; g_initial_time_qpc = real_now;
    }
    if (gtc64) {
        ULONGLONG real_now = gtc64();
        ULONGLONG fake_now = (ULONGLONG)((double)(real_now - g_initial_time_gtc64) * old_sp) + g_initial_offset_gtc64;
        g_initial_offset_gtc64 = fake_now; g_initial_time_gtc64 = real_now;
    }
    g_speed = new_speed;
    LeaveCriticalSection(&g_lock);
    apply_audio_speed(new_speed);
}

__declspec(dllexport) void __stdcall initdll(void) {
    GetTickCount_t gtc; GetTickCount64_t gtc64; QueryPerformanceCounter_t qpc;
    ensure_cs(); g_speed = 1.0f; install_audio_hooks();
    gtc = resolve_real_gtc(); gtc64 = resolve_real_gtc64(); qpc = resolve_real_qpc();
    EnterCriticalSection(&g_lock);
    if (gtc) { DWORD t = gtc(); g_initial_offset_gtc = t; g_initial_time_gtc = t; }
    if (gtc64) { ULONGLONG t = gtc64(); g_initial_offset_gtc64 = t; g_initial_time_gtc64 = t; }
    if (qpc) { LARGE_INTEGER li; qpc(&li); g_initial_offset_qpc = li.QuadPart; g_initial_time_qpc = li.QuadPart; }
    LeaveCriticalSection(&g_lock);
}

__declspec(dllexport) void __stdcall speedhack_initializeSpeed(float speed) { InitializeSpeedhack(speed); }

/* FastMode's injector calls these CreateRemoteThread-compatible adapters. */
__declspec(dllexport) DWORD WINAPI Speedhack_InitThread(void *unused) {
    (void)unused;
    initdll();
    return install_timing_hooks() ? 1u : 0u;
}

__declspec(dllexport) DWORD WINAPI Speedhack_SetSpeedThread(void *speedAddress) {
    float speed;
    if (!speedAddress) return 0;
    if (!g_timing_hooks_ready && !install_timing_hooks()) return 0;
    speed = *(float *)speedAddress;
    InitializeSpeedhack(speed);
    return 1;
}

__declspec(dllexport) DWORD __stdcall audioSpeedhackGetStatus(void) {
    return (DWORD)InterlockedCompareExchange(&g_audio_status, 0, 0);
}

__declspec(dllexport) DWORD __stdcall audioSpeedhackGetLastXAudioResult(void) {
    return (DWORD)InterlockedCompareExchange(&g_last_xaudio_result, 0, 0);
}

__declspec(dllexport) DWORD __stdcall audioSpeedhackGetVoiceCount(void) {
    DWORD count = 0;
    int i;
    EnterCriticalSection(&g_audio_lock);
    for (i = 0; i < MAX_XA_VOICES; i++) if (g_xa_voices[i].used && g_xa_voices[i].voice) count++;
    LeaveCriticalSection(&g_audio_lock);
    return count;
}

__declspec(dllexport) DWORD __stdcall audioSpeedhackGetSpeedBits(void) {
    DWORD bits;
    float speed = current_speed();
    memcpy(&bits, &speed, sizeof(bits));
    return bits;
}

__declspec(dllexport) DWORD WINAPI audioSpeedhackRemoteApply(void *speedBits) {
    DWORD bits = (DWORD)(uintptr_t)speedBits;
    float speed;
    ensure_cs();
    memcpy(&speed, &bits, sizeof(speed));
    g_speed = clamp_speed(speed);
    bootstrap_wasapi();
    return audioSpeedhackGetStatus();
}

__declspec(dllexport) DWORD WINAPI audioSpeedhackRemoteStatus(void *unused) {
    (void)unused;
    return audioSpeedhackGetStatus();
}

__declspec(dllexport) DWORD WINAPI audioSpeedhackRemoteProbeObject(void *candidate) {
    void **vtable;
    void *renderClient = NULL;
    Unknown_QueryInterface_t queryInterface;
    XA_Release_t releaseObject;
    HRESULT result;
    if (!candidate) return E_INVALIDARG;
    vtable = *(void ***)candidate;
    if (!vtable || !vtable[0]) return E_NOINTERFACE;
    queryInterface = (Unknown_QueryInterface_t)vtable[0];
    result = queryInterface(candidate, &ce_iid_audio_render_client, &renderClient);
    if (FAILED(result) || !renderClient) return result;
    vtable = *(void ***)renderClient;
    hook_vtable_slot(&vtable[RC_VT_GET_BUFFER], (void *)hook_RC_GetBuffer, (void **)&real_RC_GetBuffer);
    hook_vtable_slot(&vtable[RC_VT_RELEASE_BUFFER], (void *)hook_RC_ReleaseBuffer, (void **)&real_RC_ReleaseBuffer);
    if (vtable[RC_VT_GET_BUFFER] == (void *)hook_RC_GetBuffer &&
        vtable[RC_VT_RELEASE_BUFFER] == (void *)hook_RC_ReleaseBuffer) {
        g_hooked_wasapi = 1;
        InterlockedOr(&g_audio_status, AUDIO_STATUS_WASAPI_HOOKED);
    }
    releaseObject = (XA_Release_t)vtable[2];
    releaseObject(renderClient);
    return audioSpeedhackGetStatus();
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) { DisableThreadLibraryCalls(h); ensure_cs(); }
    return TRUE;
}
