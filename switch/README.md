# Nintendo Switch build

Homebrew (.nro) port of [Rogue Squadron 64 Recompiled](https://github.com/MikeSemicolonD/RogueSquadron64Recomp)
(Star Wars: Rogue Squadron, N64). The game code, hooks, patches and MusyX audio are the PC project's; the PC frontend
(RT64, ImGui panels, touch, file dialogs) is replaced by the files in this folder, shared with the Castlevania 64 and
Legacy of Darkness Switch ports and adapted from [DK64Recomp-NX](https://github.com/Trinalord777/DK64Recomp-NX) by
Trinalord777.

- **Graphics:** RT64 needs Vulkan, D3D12 or Metal, which Switch homebrew lacks. This build draws with
  [GLideN64](https://github.com/gonetz/GLideN64) on OpenGL (Mesa nouveau), using GLideN64's own HLE of
  Factor 5's Rogue Squadron microcode instead of the RT64 fork's `GBI_F3DFACTOR5` profile. The RT64-side
  renderer hooks (lighting map, tow cable, F5 counters) are no-ops (`rs64_stubs.cpp`). GLideN64's copy of each
  frame back to RDRAM is off: the game does not need it, and it cost more than half of every frame at stock clocks.
- **Runtime:** `patches/n64modernruntime.patch` adds the libnx threading, memory size, file and VI changes to
  the N64ModernRuntime fork; `patches/gliden64.patch` and `patches/n64recomp.patch` as in DK64Recomp-NX.

## Status

Version 1.0. On a V1 Switch with Atmosphère the game boots and plays with music, sound effects and voices.
The first mission holds the game's full 30 fps in handheld at the stock 1020 MHz CPU clock; later missions have been
played with the CPU overclocked and not yet measured at stock.

Known issues:
- 4:3 is the default. Widescreen (Aspect Ratio 16:9 in NX Options) shows glitches at the edges of the terrain,
  because the game only draws the terrain a 4:3 view can see.
- Brief stutter the first time an effect appears while its shader compiles; this mostly goes away on later launches.
- The PC version's extras (online co-op, mods, the RT64 lighting and effects enhancements) are not in this build.
- Title takeover only: launch through a game, not the Album applet.

## Download

Get the latest `.zip` from the [Releases](../../../releases) page. It contains the compiled port only; the ROM is not
included and is needed to play.

## Install

1. Extract the release zip to the root of your SD card, so that `rs64recomp.nro` ends up in `sdmc:/switch/rs64recomp/`.
2. Copy your Star Wars: Rogue Squadron (USA v1.0) ROM into the same folder (any `.z64`, `.n64` or `.v64` name).
   SHA-1 `ed42eed1ee2db646ff7ef94ba8c5421d164a4f0d`.
3. Launch from the Homebrew Menu through title takeover (hold R while opening a game), not the Album applet.

Minus + Plus opens the NX Options menu (resolution, aspect ratio, frame rate, controls, performance, volume, quit).

## Reporting problems

Open an issue with the console model, Atmosphère version, where in the game it happened and the logs from
`sdmc:/switch/rs64recomp/`: `rs64recomp.log`, `rs64recomp.err.log`, and `rs64recomp.crash.log` if there is one.

## Build

Needs devkitA64 with `switch-sdl2`, `switch-mesa`, `switch-libdrm_nouveau`, `switch-glad`, plus the inputs the PC
build generates from your ROM:

1. Decomp ELF: `python3 tools/make_elf_from_syms.py` (with `../rogue_squadron64` and its `rogue_squadron.z64`).
2. `N64Recomp rogue_squadron.toml`, then `RSPRecomp` on both `rsp/*.toml`.
3. `python3 switch/tools/rs_ucode_fixup.py build/factor5_ucode/*.c` (same edits as `tools/fixup_factor5_ucode.cpp`).
4. `python3 switch/tools/rs_gen_state_table.py tools/state/state_model.toml src/game/state_table.inl`.
5. Patches ELF with a MIPS GCC (flags in `tools/mods/code_mod.cmake`), then `N64Recomp patches.toml`.
6. `switch/build.sh`.

## Credits and license

Rogue Squadron 64 Recompiled (MikeSemicolonD and contributors), the rogue_squadron64 decomp, N64Recomp and
N64ModernRuntime (Wiseguy and contributors), GLideN64 (gonetz and contributors), the DK64Recomp-NX Switch layer
(Trinalord777), devkitPro, libnx, Mesa and SDL.

The files in this folder are under the GNU GPL v3 like DK64Recomp-NX, which they are adapted from (`switch/LICENSE`).
GLideN64 is under the GNU GPL v2 (`lib/GLideN64/LICENSE`). The upstream PC project does not state a license.

This project is in no way associated with, sponsored or endorsed by Nintendo, Disney, Lucasfilm Games or Factor 5. It
contains no ROM data and requires a legally obtained copy of the game.
