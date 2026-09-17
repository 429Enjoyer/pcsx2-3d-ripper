# PCSX2 3D Ripper

An unofficial, experimental fork of **PCSX2 v2.8.2** that captures PS2 scenes
as OBJ models with MTL materials and PNG textures.

## Features

- Single-frame 3D capture using the Software renderer.
- OBJ geometry with vertex colors and Normal/Culled groups.
- RGBA texture export with corrected pixel reads, cropping, and palette updates.
- Filtering of invalid coordinates and zero-area triangles.
- File-write error handling and bounded single-frame capture.

## Usage

1. Run PCSX2 and configure your BIOS and controller.
2. Select **Settings > Graphics > Renderer > Software**.
3. Start a game and display the scene you want to capture.
4. Press **Shift+F8** while emulation is running, or select
   **Tools > Save Single Frame GS Dump**.
5. Find the OBJ, MTL, and PNG files under
   `snaps/screenshots_3d/<game_title_serial>/` in the PCSX2 data directory.

Keep the OBJ, MTL, and textures together when importing them into a 3D editor.
Enable Vertex Groups in Blender to retain the Normal and Culled groups.
A standard GS dump is saved alongside the 3D capture.

## Limitations

This build exports reconstructed geometry from GS draw data. It does not
recover the original camera projection or automatically correct model
proportions. Depth and vertical scales may need adjustment in a 3D editor.

Face orientation, unwanted culled geometry, vertex alpha, and PS2 shading
are not fully recovered. Capture remains experimental and may affect
rendering while culled geometry is being collected. Hardware renderers
are available for emulation, but 3D capture requires Software rendering.

No post-processing or calibration scripts are included.

## Building

Follow the [PCSX2 build guide](https://pcsx2.net/docs/advanced/building/) for
platform prerequisites and matching dependencies. For an x64 Windows developer
environment with clang-cl, CMake, Ninja, and the Windows SDK available:

```powershell
cmake -S . -B build -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_C_COMPILER=clang-cl `
  -DCMAKE_CXX_COMPILER=clang-cl `
  -DCMAKE_PREFIX_PATH="C:/PCSX2-Dependencies" `
  -DDISABLE_ADVANCE_SIMD=ON
cmake --build build --target pcsx2-qt --parallel 8
```

Replace the dependency path with your local dependency directory. Run the
resulting executable with the matching runtime DLLs, Qt plugins, and resources.

The upstream PCSX2 test suite is retained. Windows x64 capture has been checked
with Ultraman Fighting Evolution 3, including OBJ/texture validation and Blender
import. Broader game and platform compatibility has not been established.

## Credits and license

- [PCSX2](https://github.com/PCSX2/pcsx2): emulator and stable release base.
- [scurest](https://github.com/scurest/pcsx2): original 3D screenshot feature.

Base release: [v2.8.2](https://github.com/PCSX2/pcsx2/releases/tag/v2.8.2),
commit `fd9d310ccbb6b8b62c976da8886a3c8fd3a10ff3`.
This is not an official PCSX2 binary. See [COPYING.GPLv3](COPYING.GPLv3) for the license.
