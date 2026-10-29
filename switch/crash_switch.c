// libnx user exception handler: on a crash, write the faulting context to sdmc:/switch/rs64recomp/rs64recomp.crash.log
// as offsets from the module start, which are ELF addresses: `aarch64-none-elf-addr2line -fCe rs64recomp.elf <offset>`.
// It writes through the file system service directly, with no malloc and no stdio locks: a crash inside malloc or
// while a thread held the stdout lock would otherwise crash or hang the handler too (an Atmosphère fatal).
#include <stdalign.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <switch.h>

#define MAX_FRAMES 32
#define STACK_SCAN_LIMIT (8u * 1024 * 1024)

alignas(16) u8 __nx_exception_stack[0x4000];
u64 __nx_exception_stack_size = sizeof(__nx_exception_stack);

static FsFile crash_file;
static bool crash_file_open = false;
static s64 crash_offset = 0;

void cvnx_crash_log_open(const char* path) {
    FsFileSystem* fs = fsdevGetDeviceFileSystem("sdmc");
    if (fs == NULL) return;
    char fs_path[FS_MAX_PATH];
    strncpy(fs_path, path, sizeof(fs_path) - 1);
    fs_path[sizeof(fs_path) - 1] = '\0';
    fsFsDeleteFile(fs, fs_path);
    if (R_FAILED(fsFsCreateFile(fs, fs_path, 0, 0))) return;
    if (R_FAILED(fsFsOpenFile(fs, fs_path, FsOpenMode_Write | FsOpenMode_Append, &crash_file))) return;
    crash_file_open = true;
}

static void out(const char* format, ...) {
    char line[256];
    va_list args;
    va_start(args, format);
    int length = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if (length <= 0) return;
    if (length > (int)sizeof(line) - 1) length = sizeof(line) - 1;
    if (crash_file_open && R_SUCCEEDED(fsFileWrite(&crash_file, crash_offset, line, length, FsWriteOption_Flush))) {
        crash_offset += length;
    }
}

// The start of the mapping that holds this code: __start__ is an absolute symbol (0), not the load address.
static u64 module_base(void) {
    MemoryInfo info;
    u32 page_info;
    if (R_FAILED(svcQueryMemory(&info, &page_info, (u64)(uintptr_t)&module_base))) return 0;
    return info.addr;
}

static u64 module_offset(u64 address) {
    return address - module_base();
}

// Frame records (fp -> {previous fp, lr}) only exist where code keeps frame pointers.
static void print_backtrace(const ThreadExceptionDump* ctx) {
    u64 fp = ctx->fp.x;
    const u64 sp = ctx->sp.x;
    for (int i = 0; i < MAX_FRAMES; i++) {
        if (fp == 0 || (fp & 0xF) != 0 || fp < sp || fp - sp > STACK_SCAN_LIMIT) {
            break;
        }
        const u64* record = (const u64*)(uintptr_t)fp;
        out("frame %02d lr=0x%010lx\n", i, module_offset(record[1]));
        fp = record[0];
    }
}

// Set by ports that place RDRAM themselves (heap_switch.c); null here, where the runtime allocates it.
__attribute__((weak)) u8* cvnx_rdram_base = NULL;

void __libnx_exception_handler(ThreadExceptionDump* ctx) {
    out("CRASH desc=0x%x esr=0x%x far=0x%lx (rdram %p, far-rdram 0x%lx)\n", ctx->error_desc, ctx->esr, ctx->far.x,
        (void*)cvnx_rdram_base, ctx->far.x - (u64)(uintptr_t)cvnx_rdram_base);
    out("pc=0x%010lx lr=0x%010lx (module offsets) sp=0x%lx fp=0x%lx\n",
        module_offset(ctx->pc.x), module_offset(ctx->lr.x), ctx->sp.x, ctx->fp.x);
    for (int i = 0; i < 29; i++) {
        out("x%02d=%016lx%s", i, ctx->cpu_gprs[i].x, (i % 4 == 3 || i == 28) ? "\n" : " ");
    }
    print_backtrace(ctx);
    if (crash_file_open) fsFileClose(&crash_file);
    svcExitProcess();
}
