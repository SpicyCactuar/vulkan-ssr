# Vulkan Screen-Space Reflections

Vulkan application that showcases the capabilities of Screen-Space Reflections.
It offers two algorithms to resolve reflections:

* 3D Ray Marching - [2011, Souta et. al](https://www.advances.realtimerendering.com/s2011/SousaSchulzKazyan%20-%20CryEngine%203%20Rendering%20Secrets%20\((Siggraph%202011%20Advances%20in%20Real-Time%20Rendering%20Course).ppt)
* Perspective-correct DDA - [2014, Mara et. al](https://www.jcgt.org/published/0003/04/04/)

The Deferred Rendering pipeline consists of the following steps:

1. Shadow pass - See `shadow_map.{vert|frag}`
2. Offscreen pass - See `offscreen.{vert|frag}`
3. Fullscreen pass - See `fullscreen.{vert|frag}`

During the Offscreen pass, the G-Buffer is constructed with the material and shadowing properties.
Subsequently, this data is leverage to determine whether the pixel microfacet is reflective and a single
SSR ray is traced. The reflected colour is dynamically constructed from the G-Buffer.

![vulkan-ssr](https://github.com/user-attachments/assets/3951ec2d-4257-49b0-9aee-3cd2fbf0d74c)

## Project Structure

```plaintext
vulkan-suntemple/
├── assets-bake/           # Asset baking source code
├── assets-src/            # Static assets (to be baked)
├── third-party/           # Bundled third party libraries
├── util/glslc.lua         # Compile-time utility to compile shaders with google/shaderc 
├── ssr/                   # Application source code
    ├── shaders/           # GLSL shaders
├── vkutils/               # Application source code
├── playback/              # CSV playback files
├── premake5.lua           # Premake 5 configuration
├── premake5(.*)           # Bundled Premake 5 executables
├── third-party.md         # Third party libraries' licenses
└── README.md              # Project README
```

## Build - Make

```shell
./premake5 [enable-diagnostics={true|false}] gmake2
make [config={debug_x64|release_x64}]
```

The `enable-diagnosticts` parameters default to `true`, and `config` defaults to `debug_x64`.
This applies to all platforms.

## Build - Visual Studio

```shell
./premake5.exe [enable-diagnostics={true|false}] vs2022
```

Open generated `.sln` project file.

## Build - Xcode

```shell
./premake5.apple [enable-diagnostics={true|false}] xcode4
```

Open generated Xcode project.

## Run

```shell
bin/assets-bake-{target}.exe
bin/vksuntemple-{target}.exe <scene-name> [tag]
```

Executables have `.exe` extension for all platforms, but binaries are platform-specific.

Baking is required to be run successfully before application.

The `scene-name` parameter is required and the set of possible values is `assets/<scene_name>`.
The `tag` parameter is used as a suffix to identify output files.

## Controls

| Key(s)                  | Action                                                           |
|-------------------------|------------------------------------------------------------------|
| `Right Click`           | Toggle camera rotation with mouse                                |
| `W` `A` `S` `D` `E` `Q` | Move camera around                                               |
| `I` / `L`               | Reset camera to initial/light position                           |
| `Shift` / `Ctrl`        | Slow/fast speed modifiers for camera controls                    |
| `P`                     | Render current frame to output `.png`                            |
| `Camera / Light` UI     | Set different parameters of the camera and single light system   |
| `Shading` UI            | Control different aspects of the shading model                   |
| `SSR` UI                | Control different aspects of the Screen-Space Reflections method |
| `Benchmarks` UI         | Performs benchmarks, with optional playback file                 |
| `Utilities` UI          | `Take Screenshot` and potentially other utilities                |
| `Esc`                   | Close application                                                |

## Technologies

* **C++**: `>= C++23`
* **Premake**: `5.0.0` (Bundled)
* **Vulkan**: `>= 1.2.0` (Bundled headers)
* **Volk**: `1.3.295` (Bundled)
* **Vulkan Memory Allocator (VMA)**: `3.1.0` (Bundled)
* **GLSL**: `460`
* **GLM**: `0.9.9` (Bundled)
* **GLFW**: `3.4` (Bundled)
* **ImGui**: `1.90.8` (Bundled)
* **stb_image**: `v2.29` (Bundled)
* **stb_image_write**: `v1.16` (Bundled)
* **Native File Dialog Extended**: **v1.2.0** (Bundled)

A few additional supporting libraries are leveraged. Files were selectively bundled as needed.

See `third-party.md` for licensing and attributions.

## Assets

Attributions to third-party assets are detailed in the respective `assets-src/<scene-name>/README.txt`.

## TODOs

* [ ] Check cross-platform compatibility
* [ ] Refactor `benchmark.cpp` to use delta Δt instead of absolute frame time
* [ ] Automatically detect changes to `shade.glsl` for shader recompilation
