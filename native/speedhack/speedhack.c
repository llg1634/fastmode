#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <stdint.h>
#include <math.h>
#include "MinHook.h"

#pragma comment(lib, "winmm.lib")

typedef DWORD (WINAPI *GetTickCount_t)(void);
typedef ULONGLONG (WINAPI *GetTickCount64_t)(void);
typedef BOOL (WINAPI *QueryPerformanceCounter_t)(LARGE_INTEGER *lpPerformanceCount);
typedef DWORD (WINAPI *timeGetTime_t)(void);

static GetTickCount_t real_GetTickCount = NULL;
static GetTickCount64_t real_GetTickCount64 = NULL;
static QueryPerformanceCounter_t real_QueryPerformanceCounter = NULL;
static timeGetTime_t real_timeGetTime = NULL;

static volatile float g_speed = 1.0f;
static CRITICAL_SECTION g_lock;
static int g_inited = 0;
static int g_hooks = 0;

static DWORD g_initial_offset_gtc = 0;
static DWORD g_initial_time_gtc = 0;
static ULONGLONG g_initial_offset_gtc64 = 0;
static ULONGLONG g_initial_time_gtc64 = 0;
static LONGLONG g_initial_offset_qpc = 0;
static LONGLONG g_initial_time_qpc = 0;

static void rebaseline(void) {
    EnterCriticalSection(&g_lock);
    if (real_GetTickCount) {
        g_initial_offset_gtc = real_GetTickCount();
        /* after hooks, GetTickCount may be hooked; use real_* */
        g_initial_time_gtc = real_GetTickCount();
        g_initial_offset_gtc = g_initial_time_gtc; /* first baseline equal */
    }
    if (real_GetTickCount64) {
        g_initial_time_gtc64 = real_GetTickCount64();
        g_initial_offset_gtc64 = g_initial_time_gtc64;
    }
    if (real_QueryPerformanceCounter) {
        LARGE_INTEGER li;
        real_QueryPerformanceCounter(&li);
        g_initial_time_qpc = li.QuadPart;
        g_initial_offset_qpc = li.QuadPart;
    }
    /* After enabling, apparent time should continue from last apparent value.
       For first init, equal bases are fine. When changing speed we must capture
       current apparent as new offset and real as new time. */
    LeaveCriticalSection(&g_lock);
}

static void rebaseline_with_current_fake(void) {
    EnterCriticalSection(&g_lock);
    float sp = g_speed;
    if (sp <= 0.f || !isfinite(sp)) sp = 1.f;

    if (real_GetTickCount) {
        DWORD now_real = real_GetTickCount();
        DWORD now_fake = (DWORD)((double)(now_real - g_initial_time_gtc) * sp) + g_initial_offset_gtc;
        g_initial_offset_gtc = now_fake;
        g_initial_time_gtc = now_real;
    }
    if (real_GetTickCount64) {
        ULONGLONG now_real = real_GetTickCount64();
        ULONGLONG now_fake = (ULONGLONG)((double)(now_real - g_initial_time_gtc64) * sp) + g_initial_offset_gtc64;
        g_initial_offset_gtc64 = now_fake;
        g_initial_time_gtc64 = now_real;
    }
    if (real_QueryPerformanceCounter) {
        LARGE_INTEGER li;
        real_QueryPerformanceCounter(&li);
        LONGLONG now_real = li.QuadPart;
        LONGLONG now_fake = (LONGLONG)((double)(now_real - g_initial_time_qpc) * sp) + g_initial_offset_qpc;
        g_initial_offset_qpc = now_fake;
        g_initial_time_qpc = now_real;
    }
    LeaveCriticalSection(&g_lock);
}

static DWORD WINAPI hook_GetTickCount(void) {
    EnterCriticalSection(&g_lock);
    DWORD now = real_GetTickCount();
    float sp = g_speed;
    DWORD result = (DWORD)((double)(now - g_initial_time_gtc) * sp) + g_initial_offset_gtc;
    LeaveCriticalSection(&g_lock);
    return result;
}

static ULONGLONG WINAPI hook_GetTickCount64(void) {
    EnterCriticalSection(&g_lock);
    ULONGLONG now = real_GetTickCount64();
    float sp = g_speed;
    ULONGLONG result = (ULONGLONG)((double)(now - g_initial_time_gtc64) * sp) + g_initial_offset_gtc64;
    LeaveCriticalSection(&g_lock);
    return result;
}

static BOOL WINAPI hook_QueryPerformanceCounter(LARGE_INTEGER *lpPerformanceCount) {
    LARGE_INTEGER cur;
    BOOL ok = real_QueryPerformanceCounter(&cur);
    if (!ok) return ok;
    EnterCriticalSection(&g_lock);
    float sp = g_speed;
    LONGLONG result = (LONGLONG)((double)(cur.QuadPart - g_initial_time_qpc) * sp) + g_initial_offset_qpc;
    LeaveCriticalSection(&g_lock);
    if (lpPerformanceCount) lpPerformanceCount->QuadPart = result;
    return ok;
}

static DWORD WINAPI hook_timeGetTime(void) {
    /* same domain as GetTickCount */
    return hook_GetTickCount();
}

static int install_hooks(void) {
    if (g_hooks) return 1;
    if (MH_Initialize() != MH_OK) return 0;

    HMODULE k32 = GetModuleHandleW(L"kernel32.dll");
    HMODULE kb = GetModuleHandleW(L"kernelbase.dll");
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    HMODULE winmm = LoadLibraryW(L"winmm.dll");

    void *pGTC = NULL;
    void *pGTC64 = NULL;
    void *pQPC = NULL;
    void *pTGT = NULL;

    if (kb) {
        pGTC = (void*)GetProcAddress(kb, "GetTickCount");
        pGTC64 = (void*)GetProcAddress(kb, "GetTickCount64");
    }
    if (!pGTC && k32) pGTC = (void*)GetProcAddress(k32, "GetTickCount");
    if (!pGTC64 && k32) pGTC64 = (void*)GetProcAddress(k32, "GetTickCount64");

    if (ntdll) pQPC = (void*)GetProcAddress(ntdll, "RtlQueryPerformanceCounter");
    if (!pQPC && k32) pQPC = (void*)GetProcAddress(k32, "QueryPerformanceCounter");
    if (winmm) pTGT = (void*)GetProcAddress(winmm, "timeGetTime");

    if (pGTC) {
        if (MH_CreateHook(pGTC, (LPVOID)hook_GetTickCount, (LPVOID*)&real_GetTickCount) != MH_OK)
            return 0;
    }
    if (pGTC64) {
        MH_CreateHook(pGTC64, (LPVOID)hook_GetTickCount64, (LPVOID*)&real_GetTickCount64);
    }
    if (pQPC) {
        if (MH_CreateHook(pQPC, (LPVOID)hook_QueryPerformanceCounter, (LPVOID*)&real_QueryPerformanceCounter) != MH_OK)
            return 0;
    }
    if (pTGT) {
        MH_CreateHook(pTGT, (LPVOID)hook_timeGetTime, (LPVOID*)&real_timeGetTime);
    }

    if (MH_EnableHook(MH_ALL_HOOKS) != MH_OK) return 0;

    /* After enabling, real_* trampolines are valid; baseline */
    rebaseline();
    g_hooks = 1;
    return 1;
}

static void set_speed_internal(float speed) {
    if (!isfinite(speed) || speed <= 0.f) speed = 1.f;
    if (!g_hooks) {
        g_speed = speed;
        return;
    }
    rebaseline_with_current_fake();
    EnterCriticalSection(&g_lock);
    g_speed = speed;
    LeaveCriticalSection(&g_lock);
}

/* CreateRemoteThread entry: LPVOID ignored */
__declspec(dllexport) DWORD WINAPI Speedhack_InitThread(LPVOID param) {
    (void)param;
    if (!g_inited) {
        InitializeCriticalSection(&g_lock);
        g_inited = 1;
    }
    install_hooks();
    set_speed_internal(1.0f);
    return 1;
}

/* CreateRemoteThread entry: LPVOID points to float */
__declspec(dllexport) DWORD WINAPI Speedhack_SetSpeedThread(LPVOID param) {
    if (!param) return 0;
    if (!g_inited) {
        InitializeCriticalSection(&g_lock);
        g_inited = 1;
    }
    if (!g_hooks) install_hooks();
    float speed = *(float*)param;
    set_speed_internal(speed);
    return 1;
}

__declspec(dllexport) void InitializeSpeedhack(float speed) {
    if (!g_inited) {
        InitializeCriticalSection(&g_lock);
        g_inited = 1;
    }
    if (!g_hooks) install_hooks();
    set_speed_internal(speed);
}

BOOL WINAPI DllMain(HINSTANCE h, DWORD reason, LPVOID reserved) {
    (void)h; (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        if (!g_inited) {
            InitializeCriticalSection(&g_lock);
            g_inited = 1;
        }
    }
    return TRUE;
}

