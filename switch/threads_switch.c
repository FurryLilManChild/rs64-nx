// Threading fixes for libnx, which the runtime's std::thread usage runs into:
// - 128 KiB default stacks (newlib.c, __syscall_thread_create) vs 8 MiB on Linux, 512 KiB on macOS;
//   recompiled N64 code uses large frames. std::thread cannot pass attributes, so it is linked with
//   -Wl,--wrap=pthread_create,--wrap=pthread_detach and lands here.
// - pthread_detach is unsupported (ENOSYS), which made std::thread::detach throw.
// - every thread starts on core 0: ultramodern's thread names/priorities are honoured below.
#include <pthread.h>

#include <stdio.h>
#include <string.h>

#include <switch.h>

// Twice what the macOS build runs with.
#define CVNX_THREAD_STACK_SIZE (1024 * 1024)

int __real_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg);

int __wrap_pthread_create(pthread_t* thread, const pthread_attr_t* attr, void* (*start)(void*), void* arg) {
    int result;
    if (attr != NULL) {
        result = __real_pthread_create(thread, attr, start, arg);
    } else {
        pthread_attr_t sized;
        pthread_attr_init(&sized);
        pthread_attr_setstacksize(&sized, CVNX_THREAD_STACK_SIZE);
        result = __real_pthread_create(thread, &sized, start, arg);
        pthread_attr_destroy(&sized);
    }
    return result;
}

// libnx leaves __syscall_thread_detach unimplemented ("Unsupported" in newlib.c), so
// pthread_detach returns ENOSYS and std::thread::detach throws (ultramodern's timer thread).
// ponytail: the thread is simply never joined; its handle lives until the process exits,
// which is the lifetime of every thread the runtime detaches.
int __wrap_pthread_detach(pthread_t thread) {
    (void)thread;
    return 0;
}

// libnx puts every thread on the process' default core (0). SDL's audio thread sharing a core with
// GLideN64 got about half of real time (25.6k of 48k frames/s, sometimes none: music cut out).
// ultramodern names its threads, so place them by role and leave core 0 to the main thread and
// SDL's audio thread:
//   core 2: "Gfx Thread" (GLideN64, presents with vsync)
//   core 1: game threads (only one runs at a time), VI, timer and RSP task threads
#define GFX_CORE 2
#define GAME_CORE 1

static int core_for(const char* name) {
    if (strcmp(name, "Gfx Thread") == 0) {
        return GFX_CORE;
    }
    static const char* const game_side[] = {"VI Thread", "Timer Thread", "SP Task Thread", "Game"};
    for (size_t i = 0; i < sizeof(game_side) / sizeof(game_side[0]); i++) {
        if (strncmp(name, game_side[i], strlen(game_side[i])) == 0) {
            return GAME_CORE;
        }
    }
    return -1;
}

// HOS never time-slices between priorities: a game thread polling at 0x30 (Rogue Squadron's waitForMusyXAudioTaskDone
// yields in a loop until the synth finishes) kept the 0x3B RSP task thread on its core from ever running. The RSP
// thread runs just above the game threads instead; they wait on its results anyway.
#define RSP_TASK_PRIORITY 0x2F
static __thread int is_rsp_task_thread;

#ifdef CVNX_REMOTE
void cvnx_remote_track_thread(const char* name, Handle handle);
#endif

// CPU time per thread role for the perf line in the log (render_gliden64.cpp): kernel run time of each named
// thread, summed by role, as a share of wall time since the previous report.
enum { ROLE_GAME, ROLE_GFX, ROLE_RSP, ROLE_VI, ROLE_TIMER, ROLE_MAIN, ROLE_COUNT };
static const char* const role_names[ROLE_COUNT] = {"game", "gfx", "rsp", "vi", "timer", "main"};
#define MAX_TRACKED 48
static struct { Handle handle; int role; u64 last; } tracked[MAX_TRACKED];
static int tracked_count;
static Mutex tracked_lock;

static int role_for(const char* name) {
    if (strcmp(name, "Gfx Thread") == 0) return ROLE_GFX;
    if (strcmp(name, "SP Task Thread") == 0) return ROLE_RSP;
    if (strncmp(name, "VI Thread", 9) == 0) return ROLE_VI;
    if (strncmp(name, "Timer Thread", 12) == 0) return ROLE_TIMER;
    if (strcmp(name, "Main") == 0) return ROLE_MAIN;
    return ROLE_GAME;
}

static u64 thread_ticks(Handle handle) {
    u64 ticks = 0;
    if (R_FAILED(svcGetInfo(&ticks, InfoType_ThreadTickCount, handle, (u64)-1)) &&
        R_FAILED(svcGetInfo(&ticks, InfoType_ThreadTickCountDeprecated, handle, (u64)-1))) {
        return 0;
    }
    return ticks;
}

void cvnx_track_thread(const char* name) {
    mutexLock(&tracked_lock);
    if (tracked_count < MAX_TRACKED) {
        Handle handle = threadGetCurHandle();
        tracked[tracked_count].handle = handle;
        tracked[tracked_count].role = role_for(name);
        tracked[tracked_count].last = thread_ticks(handle);
        tracked_count++;
    }
    mutexUnlock(&tracked_lock);
}

// Writes "game 71% gfx 48% ..." for the time since the last call. Threads that ended report nothing.
void cvnx_thread_cpu_report(char* out, size_t size) {
    static u64 last_tick;
    const u64 now = armGetSystemTick();
    u64 delta[ROLE_COUNT] = {0};

    mutexLock(&tracked_lock);
    for (int i = 0; i < tracked_count; i++) {
        const u64 ticks = thread_ticks(tracked[i].handle);
        if (ticks >= tracked[i].last) {
            delta[tracked[i].role] += ticks - tracked[i].last;
        }
        tracked[i].last = ticks;
    }
    mutexUnlock(&tracked_lock);

    const u64 wall = last_tick ? now - last_tick : 0;
    last_tick = now;
    size_t used = 0;
    out[0] = '\0';
    for (int r = 0; r < ROLE_COUNT && used < size; r++) {
        const unsigned pct = wall ? (unsigned)(delta[r] * 100 / wall) : 0;
        int n = snprintf(out + used, size - used, "%s%s %u%%", used ? ", " : "", role_names[r], pct);
        if (n < 0) break;
        used += (size_t)n;
    }
}

void cvnx_place_thread(const char* name) {
#ifdef CVNX_REMOTE
    cvnx_remote_track_thread(name, threadGetCurHandle());
#endif
    cvnx_track_thread(name);
    is_rsp_task_thread = strcmp(name, "SP Task Thread") == 0;
    int core = core_for(name);
    if (core >= 0) {
        svcSetThreadCoreMask(CUR_THREAD_HANDLE, core, 1u << core);
    }
}

// ultramodern::stop_thread: waits for a std::thread to end before joining it. Its native handle is libnx's
// struct __pthread_t, which starts with the libnx Thread.
bool cvnx_wait_thread_exit(void* native_handle, u64 timeout_ns) {
    const Thread* thread = (const Thread*)native_handle;
    return R_SUCCEEDED(waitSingleHandle(thread->handle, timeout_ns));
}

// ultramodern::ThreadPriority order (Low, Normal, High, VeryHigh, Critical) to HOS priorities,
// where lower is more urgent; 0x2C is libnx's main thread, 0x3B its default for new threads.
void cvnx_set_thread_priority(int level) {
    static const u32 hos_priority[] = {0x3C, 0x3B, 0x30, 0x2D, 0x2C};
    if (is_rsp_task_thread) {
        svcSetThreadPriority(CUR_THREAD_HANDLE, RSP_TASK_PRIORITY);
    } else if (level >= 0 && level < (int)(sizeof(hos_priority) / sizeof(hos_priority[0]))) {
        svcSetThreadPriority(CUR_THREAD_HANDLE, hos_priority[level]);
    }
}
