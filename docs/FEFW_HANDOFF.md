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
- Cache `work\shader_cache\spirv\results.tsv`: 638 blocks, 602 ok, 36 unsupported, 0 exception (was 375 ok / 227 exception
  before the c[24] fix). The menu frame and the save indicator draw.

## Dev loop
`powershell -ExecutionPolicy Bypass -File fefw_test.ps1 [-Seconds 90] [-DumpShaders] [-Lift]`
Logs: `nxrt.log` (repo root, runtime log incl. crash reports with guest/host stacks), `work\logs\run_err.txt`, `work\logs\build.txt`.
Delete `work\shader_cache\spirv` to force re-translation after translator changes (or only the affected rows/.spv).

## Done: c[24] (was task 1, 227 fragment shaders threw in GetConstantBufferBinding)
They read constant bank 24 (`c[0x18]`), always a 3x3 matrix (std140 rows 0x0/0x10/0x20, a vec3 at 0x30 in a few) applied to the
final colour: the NVN2 output colour transform the driver supplies; the game binds uniform indices 0..13 only (c3..c16).
`nvn2-cbank24-color-transform.patch` gives bank 24 slot 17's binding under the name `<stage>_c24`; the runtime maps that name to
`NvnSource::ColorTransform` and binds an identity matrix (`color_transform_buffer`, nvn_pipeline.cpp). For HDR, fill it with
the gamut matrix. Workflow that worked: `sassdecode --dump code ctrl` (tools\shaders\ampere\decoder\tool) to read the SASS, then
the harness on the single block, then `--batch` on the dump. When a patch replaces a whole file, also move it from the
`<Compile Include="$(ShaderSrc)...">` Exclude list to a `$(PatchedDir)` Compile item in the csproj.

## Task 1 (next, was task 2): 36 unsupported
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
