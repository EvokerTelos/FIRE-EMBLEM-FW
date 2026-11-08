# Fortune's Weave native port — handoff for Claude Code (local)

Read this first, then `docs/FEFW_BRINGUP.md`. Repository: EvokerTelos/FIRE-EMBLEM-FW, branch `fefw`, clone at `C:\FEFW-port`
(flat layout: `game/`, `work/` inside the clone). Base: drakolordx7/switch2-recomp (Pokopia, SDK 20.5); this game is SDK 22.2.

## Never commit
Game files or anything derived from them: `game/`, `work/`, `refs/` (Ryubing checkout), shader caches/dumps, NSOs, RomFS. GPL-3.0.
Commits: author EvokerTelos <lepatoh@gmail.com>, AI disclosure + Co-Authored-By trailer.

## State (2026-11-08: gameplay at native 4K, ~55-58 fps, real 60 fps logic)
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

## Fixed 2026-11-08: real 60 fps and the white-out
- 60 fps: [fefw] fps_target (default 60) keeps the frame-rate target at [main+0x21E60D8]+0x1DC at 60; the game steps its logic by
  60/target, so it is one step per frame at real speed. Without it the gameplay ran at 2x (the game asks present interval 2 with its
  30, and the host ignores the interval). Runs now reach the battle map at real speed (~75 s on a save that continues the tutorial).
- White-out / cyan: NVN2 compute push constants live in c[0] + 0x10..0x80 (head of the 0x80-byte compute c[0] table); the runtime
  now binds a per-dispatch c[0] with them, the table offsets come from its size (+0xba), and TEX/TXQ on an image slot read that
  image's texture (from_image). Sky cube -> probe cubes -> auto-exposure all work; battle map and dialogue scenes look right.
- Debug aids that found it: `NXRT_NVN_TRACE_SEC` / `TRACE_AFTER_FS` (frame trace + PNGs of targets), `NXRT_NVN_TRACE_DUMP_IDS=45,47`
  (dump textures by id), `NXRT_NVN_TRACE_BINDINGS=<hash prefixes>` (what each sampled binding resolves to),
  `NXRT_NVN_TRACE_TEX_EVENTS=<ids>` (init / upload / storage binds with the writing shader / GPU writes), `NXRT_NVN_DUMP_MISSING`
  (also dumps blocks sent to the on-demand translator; remove a hash's results.tsv row + .spv to get its code dumped).
  Note: paths under C:\FEFW-port cannot be deleted by the agent's shell; move files to a scratch folder instead.

## ACTION PLAN (next session, 2026-11-08 evening): "fully playable, no graphics / visual / text bugs"
Goal set by the user. Work continuously, commit on fefw, do not push. Test loop: scratch play.ps1 (launch + NXRT_BOOT_PRESS +
screenshots), `NXRT_MEDIA_MOVIE_SKIP=1`; A every 3 s from 15 s reaches the tutorial map (~60 s) and the unit panel (~96..150 s).
Defaults now: 3840x2160 native, 4096 shadows, 3 GB RT pool (main.sites), fps_target 60.
1. Black falling-leaf particles (layer t500; fs 67a08fc6 / c2bc1273 / b1918ad5): trace a map frame (`NXRT_NVN_TRACE_SEC`),
   `NXRT_NVN_TRACE_BINDINGS=67a08fc6,c2bc1273,b1918ad5` to see what their samplers resolve to (likely a bindless / texture-array
   element reading 0 or a wrong view type), then dump the shader (`NXRT_NVN_DUMP_MISSING`) and read the SASS.
2. Bindless samples that read 0 (Task 1 below, 51 material blocks): check on screen which materials miss terms (reflections,
   probes); needs texture/sampler pool descriptor arrays (Ryujinx GenerateBindlessAccess, QueryHostSupportsSeparateSampler).
3. Walk more of the game (past the tutorial: real battles, battle animations, menus, shops, dialogue, the world map) with
   screenshots every 6 s; list every visual / text bug (missing glyphs, wrong text colours, garbled UI, black / cyan objects,
   upside-down images: compare with the other emulator the user has). Check unknown-op warnings in nxrt.log:
   `grep -E "unsupported|note_unsupported|not created|UNKNOWN NVN PROC .*call #"`.
4. The old crash at ~265 s (guest-84, f_71005bca40+0x1d84 writes an unmapped heap address) after character creation: re-run
   that path (new save) and see if it still happens.
5. FPS dips to ~27 fps for 10..20 s (same scene, GPU sync waits ~42 ms a frame, VRAM fine): catch one with
   `NXRT_PROF_TRACE` + `NXRT_PROF_GPU_PASSES=1` and compare the gp: zones of a slow window with a fast one.
6. AV1 movies (opening, story scenes): decode with dav1d or Media Foundation (AV1 extension installed) into the nvnNativeImage
   textures the movie player samples ([media] av1_decoder, video_codec.cpp); then drop NXRT_MEDIA_MOVIE_SKIP from the defaults.
7. Quality: the game's DLSS call is now a same-size copy; the runtime already has [upscale] mode fsr3/dlaa (do_evaluate_dlss):
   try DLAA/FSR-native AA at 4K for anti-aliasing. HDR later.
9. In-game settings menu (user's choice: own ImGui overlay like TotK Recomp's, opened with Esc; also a pad combo). Base:
   runtime/src/gfx/overlay.cpp (Dear ImGui). Tabs Screen / Game / Sound / Controls / Advanced; each option saved to fefw.ini and
   marked "after restart" when it cannot apply live. Options: game resolution ([fefw] render_width/height; live change needs the
   RT pool size as a `dynimm` site instead of the fixed `patch`), shadow size, fps target (live), upscale / AA ([upscale] mode
   fsr3 / dlaa / off, sharpness), presentation (vsync / immediate), window size / fullscreen, scaling to the window, brightness,
   performance overlay, movie skip. Pause the game while the menu is open.
8. Before shipping: remove debug env from the defaults, a clean 10-minute play run, update docs.

## Done 2026-11-08: native 4K
- The game picks one of 4 resolution profiles (main+0x223e0b0, {out w, h, render w, h, shadow size}); stock renders 1280x720 and its
  DLSS step scales to 2560x1440. fefw_game.cpp rewrites the profiles (and the renderer's pending sizes at +0x3408) to [fefw]
  render_width x render_height (default 3840x2160, native) and shadow_size (default 4096).
- The render-target pool (f_7100646b20, 0x3f000000 = 1008 MB) does not hold that: `nvnTextureInitialize: storage does not fit in the
  pool`, black screen. `recomp/games/fefw/main.sites` patches the size at 0x7100646f88 to 0xbf000000 (3056 MB); 2 GB fit 4K but not the
  4096 shadows. Result: 3840x2160 + 4096 shadows, 0 pool failures, 54..58 fps in the tutorial map (GPU ~92% busy).
- The generic `NXRT_GFX_RENDER_SCALE` is not needed (and mixes scales: storage images stay 1x).
- GPU profile at 4K (`NXRT_PROF_TRACE=<file> NXRT_PROF_GPU_PASSES=1`, sum `gp:` zones): the big ones are full-screen passes
  (c1:43, c1:122, c1:97, c5:43 ~ 80..110 ms/s each), probe filter cs 8ab94383 (73 ms/s, runs every frame), the runtime's compute
  buffer shadow copies ("shadow in"/"shadow out", ~97 ms/s) and cs a840b7d9 (53 ms/s). To hold a locked 60: cut the shadow copies
  (compute_shadows_on, nvn_replay.cpp) or skip probe refiltering when the sky did not change.

## Done 2026-11-08: locked 60 fps at 4K
- Not GPU pixels but two runtime costs: (1) the render thread spent ~20% in new_servable (out_draw_bindings, every draw's buffers
  checked page by page against the write watch): now word-wise (`ww::all_valid_or_armed`); 1440p went 57.6 -> 60. (2) a840b7d9, the
  particle emitter update, is 256 dispatches of 1x1x1 a frame over ~10 buffers of 96..1872 bytes; each range was copied to and from
  device-local memory with barriers. [nvn] compute_shadow_small (4 workgroups) / compute_shadow_small_bytes (16 KiB): such dispatches
  bind small ranges straight from the pool. 3840x2160 + 4096 shadows now 60.0 fps (1% low ~54) in the tutorial map.
- Profiling: `NXRT_PROF_SAMPLE_HZ=500 NXRT_PROF_SAMPLE_THREADS=nvn-render,main NXRT_PROF_SAMPLE_OUT=<file>` (folded stacks), GPU passes
  as above. Some runs dip to ~27 fps for 10..20 s with large GPU sync waits in the same scene (not VRAM: ~7 of 16 GB); not explained yet.

## Next for "4K 60 fps, max quality"
- Perf headroom at 4K (above). DLSS: the TaskDLSS path is now a same-size copy; FSR/TAA-style AA would be the quality step.
- Remaining visual issues: some falling-leaf particles are black (layer t500, fs 67a08fc6 / c2bc1273 / b1918ad5).
- Fixed: the custom avatar's portrait and map icon were upside down: the game fills them with CopyTextureToTexture and
  NVN_COPY_FLAGS_MIRROR_Y_BIT, which the runtime ignored (now a mirrored blit, nvn_replay.cpp do_copy_texture_texture).
- AV1 movies: decode (dav1d or Media Foundation, the AV1 extension is installed) and feed nvnNativeImage textures; until then
  `NXRT_MEDIA_MOVIE_SKIP=1` and PLUS skips the scenes waiting for them.

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
