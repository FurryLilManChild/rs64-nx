# Nintendo Switch build

Homebrew (.nro) port of Rogue Squadron 64 Recompiled. The game code, hooks, patches and MusyX audio are the
PC project's; the PC frontend (RT64, ImGui panels, touch, file dialogs) is replaced by the files in this folder,
adapted from [DK64Recomp-NX](https://github.com/Trinalord777/DK64Recomp-NX) by Trinalord777.

- **Graphics:** RT64 needs Vulkan, D3D12 or Metal, which Switch homebrew lacks. This build draws with
  [GLideN64](https://github.com/gonetz/GLideN64) on OpenGL (Mesa nouveau), using GLideN64's own HLE of
  Factor 5's Rogue Squadron microcode instead of the RT64 fork's `GBI_F3DFACTOR5` profile. The RT64-side
  renderer hooks (lighting map, tow cable, F5 counters) are no-ops (`rs64_stubs.cpp`).
- **Runtime:** `patches/n64modernruntime.patch` adds the libnx threading, memory size, file and VI changes to
  the N64ModernRuntime fork; `patches/gliden64.patch` and `patches/n64recomp.patch` as in DK64Recomp-NX.

## Install

1. Copy `rs64recomp.nro` to `sdmc:/switch/rs64recomp/`.
2. Copy your Star Wars: Rogue Squadron (USA v1.0) ROM into the same folder (any `.z64`, `.n64` or `.v64` name).
3. Launch from the Homebrew Menu through title takeover (hold R while opening a game), not the Album applet.

Minus + Plus opens the NX Options menu. Log: `sdmc:/switch/rs64recomp/rs64recomp.log`.

## Build

Needs devkitA64 with `switch-sdl2`, `switch-mesa`, `switch-libdrm_nouveau`, `switch-glad`, plus the inputs the PC
build generates from your ROM:

1. Decomp ELF: `python3 tools/make_elf_from_syms.py` (with `../rogue_squadron64` and its `rogue_squadron.z64`).
2. `N64Recomp rogue_squadron.toml`, then `RSPRecomp` on both `rsp/*.toml`.
3. `python3 switch/tools/rs_ucode_fixup.py build/factor5_ucode/*.c` (same edits as `tools/fixup_factor5_ucode.cpp`).
4. `python3 switch/tools/rs_gen_state_table.py tools/state/state_model.toml src/game/state_table.inl`.
5. Patches ELF with a MIPS GCC (flags in `tools/mods/code_mod.cmake`), then `N64Recomp patches.toml`.
6. `switch/build.sh`.
