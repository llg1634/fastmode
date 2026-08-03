#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

typedef DWORD (WINAPI *RemoteEntry)(void *parameter);
typedef DWORD (WINAPI *GetBits)(void);
typedef ULONGLONG (WINAPI *FakeGetTickCount64)(void);

static ULONGLONG filetime_ticks(void) {
    FILETIME value;
    ULARGE_INTEGER ticks;
    GetSystemTimePreciseAsFileTime(&value);
    ticks.LowPart = value.dwLowDateTime;
    ticks.HighPart = value.dwHighDateTime;
    return ticks.QuadPart;
}

int wmain(int argc, wchar_t **argv) {
    HMODULE module;
    RemoteEntry initialize;
    RemoteEntry setSpeed;
    GetBits getSpeedBits;
    FakeGetTickCount64 fakeGetTickCount64;
    float speed = 2.0f;
    ULONGLONG realStart;
    ULONGLONG realEnd;
    ULONGLONG tickStart;
    ULONGLONG tickEnd;
    ULONGLONG directStart;
    ULONGLONG directEnd;
    LARGE_INTEGER qpcStart;
    LARGE_INTEGER qpcEnd;
    LARGE_INTEGER qpcFrequency;
    double realMs;
    double fakeMs;
    double directMs;
    double qpcMs;
    double ratio;
    DWORD speedBits;

    if (argc != 2) {
        fwprintf(stderr, L"usage: timing_smoke.exe <speedhack_x64.dll>\n");
        return 2;
    }

    module = LoadLibraryW(argv[1]);
    if (!module) {
        fwprintf(stderr, L"LoadLibraryW failed: %lu\n", GetLastError());
        return 3;
    }

    initialize = (RemoteEntry)GetProcAddress(module, "Speedhack_InitThread");
    setSpeed = (RemoteEntry)GetProcAddress(module, "Speedhack_SetSpeedThread");
    getSpeedBits = (GetBits)GetProcAddress(module, "audioSpeedhackGetSpeedBits");
    fakeGetTickCount64 = (FakeGetTickCount64)GetProcAddress(module, "speedhackversion_GetTickCount64");
    if (!initialize || !setSpeed || !getSpeedBits || !fakeGetTickCount64) return 4;
    if (!initialize(NULL)) return 5;
    if (!setSpeed(&speed)) return 6;
    speedBits = getSpeedBits();

    realStart = filetime_ticks();
    tickStart = GetTickCount64();
    directStart = fakeGetTickCount64();
    QueryPerformanceFrequency(&qpcFrequency);
    QueryPerformanceCounter(&qpcStart);
    Sleep(400);
    QueryPerformanceCounter(&qpcEnd);
    directEnd = fakeGetTickCount64();
    tickEnd = GetTickCount64();
    realEnd = filetime_ticks();

    speed = 1.0f;
    setSpeed(&speed);

    realMs = (double)(realEnd - realStart) / 10000.0;
    fakeMs = (double)(tickEnd - tickStart);
    directMs = (double)(directEnd - directStart);
    qpcMs = (double)(qpcEnd.QuadPart - qpcStart.QuadPart) * 1000.0 /
            (double)qpcFrequency.QuadPart;
    ratio = fakeMs / realMs;
    printf("speed_bits=0x%08lx real_ms=%.2f api_tick_ms=%.2f direct_tick_ms=%.2f qpc_ms=%.2f ratio=%.3f\n",
           speedBits, realMs, fakeMs, directMs, qpcMs, ratio);

    return speedBits == 0x40000000u && ratio >= 1.7 && ratio <= 2.3 &&
           directMs / realMs >= 1.7 && qpcMs / realMs >= 1.7 ? 0 : 7;
}
