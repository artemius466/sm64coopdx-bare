# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

sm64coopdx: a PC port of the Super Mario 64 decompilation with online multiplayer, a Lua modding API (SMLua), and a custom UI framework (DJUI). Only the US ROM (`VERSION=us`) is supported; `Makefile` validates `VERSION` to `us` and `src/pc/rom_checker.cpp` only accepts the US ROM's MD5.

## Build

One-time setup (assets are not committed; they are extracted from a ROM):

```bash
# baserom.us.z64 (vanilla US ROM) must be in the repo root
python3 extract_assets.py us      # not run automatically by make
```

Then:

```bash
make -j$(sysctl -n hw.ncpu)        # default: release, US, native arch
make DEBUG=1 DEVELOPMENT=1 -j      # dev build (see developer/compile.sh)
make clean                         # removes build/
make distclean                     # also cleans tools and extracted assets
```

- **macOS requires GNU make 4.x** (`brew install make`), invoked as `gmake`. Apple's bundled make 3.81 fails. Other required tools: SDL2, GLEW, libcurl, zlib, `pkg-config`, `gcp` (coreutils), and gcc's `cpp-N` (the Makefile shells out to `cpp-<gcc version>`). CI installs: `brew install make mingw-w64 pkg-config glfw3 coreutils` and sets `PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig`.
- On macOS `USE_APP=1` by default, so `all` wraps the binary into `build/us_pc/sm64coopdx.app`. Use `make USE_APP=0` to get a plain `build/us_pc/sm64coopdx`.
- `make` builds `tools/` first (n64graphics, mio0, audiofile, ido recomp, etc.) — do not clean that directory casually, it is slow to rebuild.
- Asset data is split into generated `.inc.c` files under `levels/`, `actors/`, `bin/`, `textures/`, `sound/`. Files with `custom` in the name are committed and hand-maintained; the rest come from `extract_assets.py` and are gitignored.

Relevant build knobs: `DEBUG`, `DEVELOPMENT`, `STRICT` (adds `-Werror`), `LUA_UNSAFE` (dev-mode only), `ASAN`, `PROFILE`, `DISCORD_SDK`, `COOPNET`, `UPDATER`, `HANDHELD`, `TARGET_RPI`, `TARGET_RK3588`.

## Running

The executable looks for a valid vanilla US ROM in its save directory (`~/Library/Application Support/sm64coopdx` on macOS) and shows a drag-and-drop screen if missing. Useful flags (`src/pc/cliopts.c`):

```bash
./build/us_pc/sm64coopdx --console            # Lua errors/logs appear in the terminal
./build/us_pc/sm64coopdx --console --server 7777
./build/us_pc/sm64coopdx --client <ip> <port>
./build/us_pc/sm64coopdx --savepath '!/save'  # '!' = executable directory
```

`--headless`, `--enable-mod`, `--disable-mods`, `--skip-update-check`, `--no-discord`, `--hide-loading-screen` also exist. `developer/compile.sh` (and `debug.sh`, which uses cgdb) build a dev binary and launch it.

## Tests and linting

There is no unit-test suite. `make test` is an N64-emulator target, not a test runner. Verification is manual play (run with `--console` and watch logs) plus:

```bash
./format.sh src/pc/lua/smlua.c      # clang-format with the repo's .clang-format; no args = format everything
tools/clang-tidy.sh <files>         # auto-fixes readability-braces-around-statements; needs a compile_commands.json (-p .)
developer/cpp-check.sh              # cppcheck over src/
developer/static-analysis.sh        # scan-build on a debug build
```

## Code conventions

From `docs/c/c.md`: `camelCase` variables, `snake_case` functions and filenames, `PascalCase` structs, `g` prefix for globals, `s` prefix for file-static variables, and pointer style `Type *value`. `4`-space indent, `104`-column limit.

## Architecture

### Layering

- `src/game/`, `src/engine/`, `src/audio/`, `src/goddard/`, `src/menu/`, `lib/src/` — the decompiled N64 game (Mario actions, objects/behaviors, camera, HUD, audio engine, level scripts, math, collision). Mostly upstream SM64 code; port-specific edits are guarded by `TARGET_N64 == 0`.
- `src/pc/` — everything PC-specific: windowing, input, filesystem, networking, Lua API, DJUI, config, mod loading.
- `include/` — shared decomp headers (`types.h`, `behavior_table.h`, `object_constants.h`, `macros.h`).
- Assets (`levels/`, `actors/`, `bin/`, `textures/`, `sound/`) are compiled into C sources, but most vtx/texture/collision/anim data is **loaded at runtime from the user's ROM** through `ROM_ASSET_LOAD_*` macros (`src/pc/rom_assets.c`), which queue loads via constructors and read segments with `rom_assets_load()`. `assets.json` maps file → ROM offset and is what `extract_assets.py` uses; if you add a new vanilla-derived asset, it must be registered there / in the generated `*.inc.c`, whereas `custom*` assets are compiled directly.

### Main loop

`src/pc/pc_main.c:main()` → `main_game_init()` (load ROM assets, init mods, audio, network player) → `thread5_game_loop()` (`src/game/game_init.c`) → per-frame `produce_one_frame()`:

1. `network_update()` — receive/process packets, sync objects, players
2. `patch_interpolations_before()`
3. `game_loop_one_iteration()` — controllers, audio tick, level script execute, vsync
4. `smlua_update()` — Lua hooks (`HOOK_UPDATE`), live reload
5. audio buffering, then render (`produce_interpolation_frames_and_delay`)

Anything that must run every frame belongs in one of these, not in a window callback.

### SMLua (Lua modding API) and autogen

`src/pc/lua/` exposes C to Lua. Files named `*_autogen.*` are **generated, tracked in git, and must be regenerated** — never hand-edit them:

```bash
./autogen/autogen.sh        # runs gen_math, convert_functions, convert_structs, gen_hooks, convert_constants, extract_display_lists
```

Run it after changing any exposed function signature/description, struct field, enum/constant, or hook. The scanners read the file lists (`in_files`) in `autogen/convert_*.py`; a new function is only exposed if its header is listed there and not excluded by the `override_*` rules. Documentation strings live in the C header as `/* |description|...|descriptionEnd| */` above the declaration; the same run regenerates `docs/lua/functions*.md`, `docs/lua/structs.md`, `docs/lua/constants.md`, and editor stubs in `autogen/lua_definitions/`.

Hooks: add the event to `enum LuaHookedEventType` in `src/pc/lua/smlua_hooks.h` (before `HOOK_MAX`), add an `SMLUA_EVENT_HOOK(...)` line to `src/pc/lua/smlua_hook_events.inl`, then rerun autogen (generates `smlua_hook_events_autogen.inl`). Call sites use `smlua_call_event_hooks(HOOK_X, ...)`; the `OUTPUT` parameters are the Lua-returned overrides and keep their C default when Lua returns nothing. Return modes (`HOOK_RETURN_NEVER`, `..._ON_SUCCESSFUL_CALL`, `..._ON_OUTPUT_SET`) decide whether later mods still get the event.

The Lua layer is split into `smlua_functions.c` (hand-written bindings), `smlua_cobject.c` (struct wrappers/memory tracking), `smlua_sync_table.c` (networked per-player/global tables), hooks, and per-domain helpers in `src/pc/lua/utils/`.

### Networking

`src/pc/network/network.c` drives everything; transport is either raw UDP (`socket/`) or CoopNet relayed hosting (`coopnet/`, prebuilt `lib/coopnet`).

- Add a packet type to `enum PacketType` (`packets/packet.h`), add a `network_receive_<x>` handler and its `PACKET_*` case in `packet_process()` (`packets/packet.c`). Packets are hand-serialized with `packet_write`/`packet_read` into a fixed `PACKET_LENGTH` buffer, with reliable and ordered variants.
- Object replication is done through `sync_object.c`: each synced object gets a sync ID and an owner; `owned` (or `override_ownership` callback) decides who authoritatively sends. Fields registered with `sync_object_init_field*` are replicated. Lua mods opt in via `network_init_object()`.
- **Player indices:** `localIndex` (per-client ordering of `gMarioStates`, `gNetworkPlayers`) is not the same across clients; `globalIndex` is. `gPlayerSyncTable` translates indices automatically — do not index shared arrays by another player's local index.
- `MAX_PLAYERS` is 16 (`include/types.h`).

### DJUI

`src/pc/djui/`: every element embeds `struct DjuiBase` as its first field. Elements are never allocated directly — each has a `djui_<element>_create(parent, ...)` that allocates and registers itself as a child of the parent base; properties are set through `djui_<element>_set_*` functions. Menus build a `DjuiThreePanel` (header/body/footer) and push it with `djui_panel_add`. Layout is `DjuiFlowLayout` (auto-stacked children) or a rect container (children position themselves). UI strings go through `DLANG(HEADER, KEY)` with entries in `lang/*.ini`. `docs/c/sections/djui.md` has the walkthrough.

### Config

To add a setting: declare the variable in `configfile.h`, define it with a default in `configfile.c`, and append an entry (key, `ConfigOptionType`, pointer to the variable) to the `options` array in the same file. `#ifdef DEVELOPMENT` sections hold dev-only settings.

### Mods, DynOS, built-in content

- A mod is a directory with `main.lua` (plus optional `actors/`, `data/`, `textures/`, `levels/`, `sound/`) or a single `.lua` file. Mods in the repo's `mods/` directory are built-in and copied into `build/us_pc/mods`; user mods live in the save directory's `mods/` folder. Lua mods get a sandboxed zip-backed filesystem (`mod_fs.cpp`, docs in `docs/lua/guides/modfs.md`), storage, and a cache.
- `dynos/packs/` holds DynOS data (`.bin`/`.tex` models, animations, geo) consumed by `data/dynos_*.cpp`; mods may ship DynOS content too.
- Dev builds enable `/warp`, `/lua <code>`, `/luaf <file>` chat commands (`src/pc/dev/chat.c`) and Lua live reload. Dev builds are not for online play (`developer/README.txt`).

### Behaviors

Object behavior scripts live in `src/game/behaviors/*.inc.c` (included into `src/game/behavior_actions.c`). The behavior ID enum is hand-maintained and ordered alphabetically in `include/behavior_table.h`, with matching entries in `data/behavior_table.c` and behavior definitions in `data/behavior_data.c`; all three must stay in sync, and `id_bhv_max_count` must remain last. Lua can register new behaviors at runtime with `hook_behavior`.
