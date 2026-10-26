// Native library for the menu-example mod. Its exports back the buttons on the
// mod's custom page (see roguesq_menu.json). Each export has the recomp signature
// void(uint8_t* rdram, recomp_context* ctx); the menu system calls it with a zeroed
// ctx and marshals via registers: a toggle getter returns state in r2.
//
// Built with this folder's CMakeLists.txt into menu_example.dll.

#include <cstdint>
#include <cstdio>

#ifdef _WIN32
#define MOD_EXPORT extern "C" __declspec(dllexport)
#else
#define MOD_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// Prefix of recomp_context: the 32 general-purpose registers as 64-bit values.
struct RecompRegs { uint64_t r[32]; };

// librecomp checks this on load; must be 1.
MOD_EXPORT uint32_t recomp_api_version = 1;

// A custom action button ("@example_hello").
MOD_EXPORT void example_hello(uint8_t* rdram, void* ctx) {
    (void)rdram; (void)ctx;
    std::fprintf(stderr, "[menu-example] hello from the custom page\n");
    std::fflush(stderr);
}

// A custom toggle ("@example_flag" flips, "example_flag_get" reports state in r2).
static int s_flag = 0;
MOD_EXPORT void example_flag(uint8_t* rdram, void* ctx) {
    (void)rdram; (void)ctx;
    s_flag = !s_flag;
}
MOD_EXPORT void example_flag_get(uint8_t* rdram, void* ctx) {
    (void)rdram;
    ((RecompRegs*)ctx)->r[2] = (uint64_t)s_flag;
}
