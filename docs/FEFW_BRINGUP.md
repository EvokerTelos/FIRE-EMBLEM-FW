# Fire Emblem: Fortune's Weave — native port bring-up

Module `fefw`: `recomp/games/fefw/` (lifter manifest) and `runtime/games/fefw/` (runtime module). No game files live in
this repository; you need your own dump of the game (update v131072, main build id `DB93B0125F485060…`).

## Status

| Stage | What | State |
|---|---|---|
| 0 | Toolchain installed (VS Build Tools 17/18, LLVM clang-cl, CMake, Ninja, Python 3.13 + uv) | user side |
| 1 | Module skeleton, manifest, hashes | done |
| 2 | Lift all 6 modules, link, boot to the first log lines | next |
| 3 | Missing services / NVN2 procs (73 unregistered, 10 new vs Pokopia: Cooperative Vector, WriteDescriptorTableData, Window AcquireTimeout, BootstrapLoader) | |
| 4 | Graphics: shaders, DLSS path (TaskDLSS / nvnDlss*) → FSR or native | |
| 5 | 60 fps: target at `[main+0x21E60D8]+0x1DC`, logic steps by 60/target | |

## Dev flow (Windows, from the repo root)

1. Put the exefs files (`main`, `sdk`, `subsdk0..2`, `rtld`, `main.npdm`) in `game\dump\exefs\`.
2. `main` is ZBIC-compressed (NSO flags 0xbf) and the lifter refuses it. Build `tools\extract` and run
   `nx_extract --plain-nso game\dump\exefs game\dump\plain` (the launcher's setup does the same).
3. `recomp\scripts\build_game.cmd fefw` — lifts and compiles into `work\lift`, `work\build\fefw`.
4. `powershell -File game-build\scripts\build.ps1 -Manifest recomp\games\fefw\game.toml`
5. Run `fefw.exe`; send `fefw.log` back for stage 2/3.

Module bases: main 0x7100000000, sdk 0x7500000000, subsdk0 0x7600000000, subsdk1 0x7610000000,
subsdk2 0x7620000000, rtld 0x7700000000 (entry). Unit counts in game.toml are first guesses; raise them if a unit is too big to compile.
