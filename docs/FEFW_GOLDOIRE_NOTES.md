# fefw-goldoire branch (EvokerTelos/FIRE-EMBLEM-FW)

Base: Goldoire/fefw-recomp 1d82e42 (GPL-3.0), imported in commit 4d5fff4. Our earlier port lives on branch `fefw`
(C:\FEFW-port); its handoff is docs/FEFW_HANDOFF.md there.

## Local setup (this PC)
- Worktree C:\FEFW-gold. ExeFS: game\dump\exefs (copied from the user's 0.1.2 release folder, already prepared).
- Build: `work\build_all.cmd` (venv, lift, textures, exe). The lift + compile takes >10 min: start it detached.
- Run folder: work\play (fefw.ini, controls.ini with the user's key layout, save\ with a copy of the user's battle save,
  upscalers\fsr3\amd_fidelityfx_vk.dll from Palworld). Shaders and BC7 cache: the user's 0.1.2 release folder.
- Play: `play_fefw.ps1` in the worktree root (not in git).

## Ported from `fefw`
FSR with the single FidelityFX DLL, 3 GB render-target pool (main.sites) and full 4K render size, small-dispatch shortcut,
PATH quote fix. Already in the base: copy mirror flags, word-wise servable check, keyboard/pad pumping, movies (dav1d).

## Settings used (work\play\fefw.ini)
output 2160p, render_ratio 0.67 (2560x1440 -> FSR 3.1.4 -> 3840x2160), shadow_size 4096, fps60 1: 60.0 fps on an RX 9070 XT.

## Open
- Title text drawn as barcode bars (user screenshot from `fefw`): check on this base.
- Dark red patch along a wall in the desert battle map: check against the user's emulator.
