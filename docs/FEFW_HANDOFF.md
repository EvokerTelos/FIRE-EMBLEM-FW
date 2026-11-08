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
- Shader translation (stage 4): every block the game uses so far translates. The boot screens (autosave notice) draw; gameplay
  was not reached in the 90 s test runs, so the material / light-probe shaders are not checked on screen yet.
- On-demand translator built: `tools\shaders\build\bin\Release\net10.0\spirv_harness.exe` (fefw_shaders_setup.ps1;
  Ryubing at `refs\ryubing`, staged to `tools\shaders\third_party\ryubing`, patches in `tools\shaders\harness\translator\*.patch`).
- Cache `work\shader_cache\spirv\results.tsv`: 638 blocks, 638 ok (was 375 ok / 227 exception / 36 unsupported).
  `NX_HARNESS_STACK=1` makes the harness print a translator exception's whole stack.

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

## Done: the 36 unsupported (was task 2)
- Bindless TEX.B / TXQ.B / TLD4.B (AmpereEmit.cs `IsBindlessTex`, by opcode: TLD4's `.B` is also the blue component). The .SCR
  parameter list is `[array, coords..., handle, dc, lod, offsets, ms]`, split two in Ra and the rest in Rb; TXQ.B Ra = [handle, lod];
  opcode 0xf60 takes the handle from a uniform register. They become Ryujinx bindless ops: a handle loaded from a uniform block with
  a dynamic index turns into a texture array (`texTextureCube[c6:0x256]` = c[6], 256 handles 8 bytes apart from word 0), which the
  runtime now fills element by element (`resolve_texture_element`, nvn_replay.cpp).
- IMAD.WIDE.U32 carry out, TEX.F16.RN (fp16 packed results), ATOMS.CAST.SPIN (0x38d with bits 87..88 = 3, returns 1 on success).
  The 0xf60 and CAST.SPIN decodes are inferred from how the code uses them, not checked against nvdisasm.

## Reaching gameplay (2026-11-07)
Runs with `NXRT_MEDIA_MOVIE_SKIP=1 NXRT_APPLET_SWKBD_PROMPT=0 NXRT_APPLET_SWKBD_TEXT=Tester` and scripted presses
(`NXRT_BOOT_PRESS`, entries are processed in list order, so sort them by time): A at the autosave screen, PLUS to skip the scenes
whose movie was skipped (date card, a black scene at ~60 s), A for dialogue, RIGHT then A through the character creation.
It now gets through the prologue, character creation, name entry and into the next 3D scene (~190 s), then:
- crash at ~265 s on a game worker thread (guest-84): `f_71005bca40+0x1d84` writes an unmapped heap address (0x79f0d4cae0),
  frames 0x71005bcabc <- 0x71005bc8b0 <- 0x71005b9b00 ... (main is stripped). Not investigated yet.
Graphics problems seen on the way (to fix next):
- characters drawn cyan in the character creation (the 2D portrait has white hair and normal skin, the 3D model is cyan).
  Likely the bindless samples that read 0 (task 1 below: per-material handle tables read with LDG), not checked yet.
- black screens: the scenes whose AV1 movie is skipped stay black until PLUS (no AV1 decoder; the movie player samples the
  decoder's images as NVN textures through nvnNativeImage*, not implemented). [media] av1_decoder = black is the start of that.
- `sampled texture ... (RG8, target 4 cube, 16x16x6) has no view of the shader's type 3` on the bindless material shaders.
Fixed on the way: PAC register forms and `br x30` returns in the lifter (setjmp trap, hand-written routines), SDK 22 nn::codec
names, ILibraryAppletAccessor cmd 170. Symbolize sdk addresses with the .dynsym of game\dump\exefs\sdk (NSO header: dynstr/dynsym
extents at 0x90/0x98 relative to .rodata, segments LZ4).

## Task 1 (next): bindless samples that read 0
51 blocks (the material shaders with TXQ.B, and 1e1e3f3b) have bindless samples Ryujinx cannot resolve, so they read 0
(translator message "Failed to find handle source for bindless access"): (a) the handle is the constant 0 (`MOV Rb, RZ` before
`TEX.SCR.B`, 1e1e3f3b: no patch table in the control section, so 0 is the real value: find what texture/sampler id 0 is in the
game's pools), (b) the handle comes from memory (`LDG.E.64.CONSTANT R114, [R114.U32+UR4]`, 23b2a51d family: a per-material handle
table). (b) needs texture/sampler pool descriptor arrays in the runtime (Ryujinx `GenerateBindlessAccess`, enabled by
`QueryHostSupportsSeparateSampler` in tools\shaders\harness\StaticGpuAccessor.cs, currently false). Check on screen first, once
gameplay is reached, whether the missing terms are visible (reflections / probes look most likely).

## Later
- 73 NVN procs not in the runtime table (ray tracing, mesh tasks, cooperative vectors, AcquireTimeout, WriteDescriptorTableData...):
  see `UNKNOWN NVN PROC` lines in nxrt.log; implement the ones the game actually calls.
- DLSS (TaskDLSS / nvnDlss*) -> FSR or a passthrough; HDR (vi queries currently report SDR).
- 60 fps gameplay: frame-rate target at `[main+0x21E60D8]+0x1DC` (game writes 30 in gameplay, logic steps 60/target).

## Fixes so far (for orientation)
lift fp16 vector unary (neon.py), TLS+0x110 thread handle (kernel/thread.cpp), nvnBootstrapLoader override and vi
HDR/display queries (core/switch2.cpp), IAudioDevice 15-20 (services/audio.cpp), zbic_plain tool, vswhere/PATH fixes.
Symbolizing guest frames: sdk exports names (`.dynsym`); main is stripped (use the call sites / PLT GOT relocations).
