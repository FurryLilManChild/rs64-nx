// Linux stand-ins for the few libnx calls the shared Switch files make (switch/host test harness only).
#pragma once
#include <stdint.h>
#include <time.h>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;
typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t s8;
typedef uint32_t Result;
typedef uint32_t Handle;
#define R_SUCCEEDED(rc) ((rc) == 0)
#define R_FAILED(rc) ((rc) != 0)
#define CUR_PROCESS_HANDLE 0xFFFF8001
enum { InfoType_TotalMemorySize = 6, InfoType_UsedMemorySize = 7 };
typedef enum { AppletOperationMode_Handheld = 0, AppletOperationMode_Console = 1 } AppletOperationMode;

static inline u64 armGetSystemTick(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (u64)ts.tv_sec * 19200000ull + (u64)ts.tv_nsec * 192ull / 10000ull;
}
static inline u64 armTicksToNs(u64 ticks) { return ticks * 10000ull / 192ull; }
static inline Result svcGetInfo(u64* out, u32, Handle, u64) { *out = 0; return 0; }
static inline AppletOperationMode appletGetOperationMode(void) { return AppletOperationMode_Console; }
