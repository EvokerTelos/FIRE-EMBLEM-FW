# Fortune's Weave native port — handoff for Claude Code (local)

Read this first, then `docs/FEFW_BRINGUP.md`. Repository: EvokerTelos/FIRE-EMBLEM-FW, branch `fefw`, clone at `C:\FEFW-port`
(flat layout: `game/`, `work/` inside the clone). Base: drakolordx7/switch2-recomp (Pokopia, SDK 20.5); this game is SDK 22.2.

## Never commit
Game files or anything derived from them: `game/`, `work/`, `refs/` (Ryubing checkout), shader caches/dumps, NSOs, RomFS. GPL-3.0.
Commits: author EvokerTelos <lepatoh@gmail.com>, AI disclosure + Co-Authored-By trailer.

## State (2026-11-07)
- All 6 modules lifted and compiled (`recomp\scripts\build_game.cmd fefw`), `fefw.exe` links (`game-build\scripts\build.ps1`).
- The game boots into its main loop: 60 fps steady, audio (XAudio2), input, RomFS read in place from
  `C:\Users\Patricio\Desktop\SWITCH 2\FEFW ROM\FEFW ROM\131072\romfs`.
- Screen is mostly black: shader translation is the current work (stage 4).
- On-demand translator built: `tools\shaders\build\bin\Release\net10.0\spirv_harness.exe` (fefw_shaders_setup.ps1;
  Ryubing at `refs\ryubing`, staged to `tools\shaders\third_party\ryubing`, patches in `tools\shaders\harness\translator\*.patch`).
- Cache `work\shader_cache\spirv\results.tsv` after the first run: 638 blocks, 375 ok, 227 exception, 36 unsupported.

## Dev loop
`powershell -ExecutionPolicy Bypass -File fefw_test.ps1 [-Seconds 90] [-DumpShaders] [-Lift]`
Logs: `nxrt.log` (repo root, runtime log incl. crash reports with guest/host stacks), `work\logs\run_err.txt`, `work\logs\build.txt`.
Delete `work\shader_cache\spirv` to force re-translation after translator changes (or only the affected rows/.spv).

## Task 1 (next): 227 fragment shaders throw in the translator
`IndexOutOfRangeException @ Ryujinx.Graphics.Shader.Translation.ResourceManager.GetConstantBufferBinding(Int32 slot)`;
`_cbSlotToBindingMap` has 18 entries (hardware c0..c17). All 227 are fragment stage. Example block: `740f64bad0b3435a`.
Hypotheses: an LDC with a register-indexed or out-of-range cbuf slot on Ampere (NVN2), or an Ampere decode issue
(see `ampere-uniform-registers.patch`, `tools\shaders\ampere\decoder`). Steps:
1. `fefw_test.ps1 -Seconds 60 -DumpShaders` -> `work\shader_dump\{manifest.tsv,code_*.bin,ctrl_*.bin}`.
2. `dotnet tools\shaders\build\bin\Release\net10.0\spirv_harness.dll work\shader_dump\code_<h>.bin work\shader_dump\ctrl_<h>.bin -o x.spv`
   and find which instruction asks for which slot (add a guard/log in a new translator patch, never edit refs\ or third_party\ directly).
3. Fix as a `.patch` + `<ShaderPatch>` entry in `tools\shaders\harness\translator\Ryujinx.Graphics.Shader.csproj`, rebuild
   (`dotnet build tools\shaders\harness\SpirvHarness.csproj -c Release`), batch-check the dump
   (`spirv_harness.dll --batch work\shader_dump --out work\shader_batch --jobs 8`), then clear the cache and rerun the game.
4. Make sure the runtime binds whatever slot mapping the fix produces (`runtime/src/gfx/nvn/nvn_replay.cpp`, ubo[6][16]).

## Task 2: 36 unsupported
Mostly `TEX.SCR.B` / `TXQ.B` bindless texture forms, plus `IMAD.WIDE.U32` carry out, `TEX.SCR.F16.RN`, `ATOMS.CAS`, `.UNKNOWN_f60`.

## Later
- 73 NVN procs not in the runtime table (ray tracing, mesh tasks, cooperative vectors, AcquireTimeout, WriteDescriptorTableData...):
  see `UNKNOWN NVN PROC` lines in nxrt.log; implement the ones the game actually calls.
- DLSS (TaskDLSS / nvnDlss*) -> FSR or a passthrough; HDR (vi queries currently report SDR).
- 60 fps gameplay: frame-rate target at `[main+0x21E60D8]+0x1DC` (game writes 30 in gameplay, logic steps 60/target).

## Fixes so far (for orientation)
lift fp16 vector unary (neon.py), TLS+0x110 thread handle (kernel/thread.cpp), nvnBootstrapLoader override and vi
HDR/display queries (core/switch2.cpp), IAudioDevice 15-20 (services/audio.cpp), zbic_plain tool, vswhere/PATH fixes.
Symbolizing guest frames: sdk exports names (`.dynsym`); main is stripped (use the call sites / PLT GOT relocations).
