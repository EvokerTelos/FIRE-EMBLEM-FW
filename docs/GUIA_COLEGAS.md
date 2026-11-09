# Fire Emblem: Fortune's Weave nativo para PC: guía de instalación

Rama `fefw-goldoire`. Es el port de [Goldoire/fefw-recomp](https://github.com/Goldoire/fefw-recomp) (GPL-3.0) con nuestros cambios
encima: FSR 3.1 funcionando, render 4K real (pool de render targets de 3 GB), modo mouse desactivado, y precompilación completa de
shaders y texturas para que no haya tirones la primera vez.

**Este repositorio no trae nada del juego.** Cada uno necesita su propio dump (update 131072) y genera en su PC los shaders y las
texturas convertidas. No compartas archivos del juego ni nada generado a partir de ellos (shaders, cachés, texturas): ver
[LEGAL.md](../LEGAL.md).

## Requisitos
- Windows 10/11 64-bit, GPU con Vulkan 1.3 y driver al día, 16 GB de RAM, ~60 GB libres (el caché de texturas ocupa ~15 GB).
- Tu dump de Fire Emblem: Fortune's Weave **update 131072** (carpeta con `exefs\` y `romfs\`).
- Herramientas de compilación: ver [BUILDING.md](BUILDING.md) (Visual Studio Build Tools con C++, LLVM 23+, CMake, Ninja, Git,
  uv, .NET 10 SDK, PowerShell 7).

## 1. Compilar
Desde la raíz del repo, en una consola normal (`D:\dumps\fefw` = tu dump):

```
uv venv tools\.venv --python 3.13
uv pip install --python tools\.venv -r recomp\requirements.txt
tools\.venv\Scripts\python -I tools\nso\prepare_exefs.py --exefs D:\dumps\fefw\exefs
recomp\scripts\build_game.cmd fefw
pwsh -File tools\textures\scripts\fetch_third_party.ps1
pwsh -File game-build\scripts\build_fefw.ps1
```

El paso `build_game.cmd` (recompilar el código del juego) es el largo: 15-30 min.

## 2. Traductor de shaders y shaders del juego
```
git clone --filter=blob:none --no-checkout https://git.ryujinx.app/ryubing/ryujinx.git work\refs\ryubing
git -C work\refs\ryubing checkout a82350bb774f70fcbd41c9987bf67a3775409963
pwsh -File tools\shaders\scripts\setup_ryujinx.ps1 -Ryubing work\refs\ryubing
dotnet build tools\shaders\harness\SpirvHarness.csproj -c Release
tools\.venv\Scripts\python -I tools\shaders\ampere\extract_corpus.py --romfs D:\dumps\fefw\romfs --main game\dump\exefs\main --exefs game\dump\exefs --out work\shaders\corpus --jobs 8
dotnet tools\shaders\build\bin\Release\net10.0\spirv_harness.dll --batch work\shaders\corpus --out work\shaders\spirv --jobs 8
tools\.venv\Scripts\python -I tools\shaders\ampere\programs_bin.py work\shaders\corpus\programs.tsv work\shaders\programs.bin
```

## 3. Carpeta de juego
Crea `work\play\fefw.ini` (ajusta las rutas):

```ini
[log]
path = fefw.log
[fs]
exefs = C:/ruta/al/repo/game/dump/exefs
romfs = D:/dumps/fefw/romfs
save_dir = save
[nvn]
shader_cache = C:/ruta/al/repo/work/shaders/spirv
bc7_cache = C:/ruta/al/repo/work/bc7_cache
[dlss]
upscaler = fsr3
sharpness = 1
[upscale]
fsr_dll = C:/ruta/al/repo/work/play/upscalers/fsr3/amd_fidelityfx_vk.dll
[fefw]
output = 2160p
render_ratio = 0.67
quality = docked
shadow_size = 4096
fps60 = 1
[input]
jc2_mouse = off
```

- `output`: la resolución de salida (2160p = 4K, 1440p, 1080p). `render_ratio`: 0.67 = calidad FSR, 1.0 = nativo.
- **FSR 3**: pon `amd_fidelityfx_vk.dll` (FidelityFX SDK 1.1.x, la Vulkan que traen muchos juegos de PC, por ejemplo Palworld en
  `Pal\Binaries\Win64\`) en `work\play\upscalers\fsr3\`. Sin ella el juego usa un escalador propio.

## 4. Precompilar shaders y texturas (una vez, evita los tirones)
Desde `work\play`:

```
..\..\game-build\fefw\build\release\fefw.exe --ini fefw.ini --prepare-shaders --programs ..\shaders\programs.bin
..\..\game-build\fefw\build\release\fefw.exe --ini fefw.ini --prepare-textures
```

Shaders: ~4 min (10 106 programas). Texturas: ~80 min (10 188 texturas ASTC a BC7, calidad alta, ~15 GB). Si actualizas el driver
de la GPU, repite solo el de shaders.

## 5. Jugar
Desde `work\play`: `..\..\game-build\fefw\build\release\fefw.exe --ini fefw.ini`

- **F1**: menú de opciones (resolución, FSR, fps, ventana, sonido, controles).
- Los controles de teclado se configuran en `work\play\controls.ini` (sección `[bindings.all]`) o desde F1.
- Las partidas quedan en `work\play\save`. Si se cierra o se congela, mira `work\play\fefw.log`.

Probado en una RX 9070 XT: 4K (2560x1440 + FSR 3) con sombras 4096 a 60 fps estables.
