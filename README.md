<h1 align="center">Physara</h1>

<p align="center">
    A physically based renderer with a lightweight scene editor.
</p>

<p align="center">
    <a href="#build">Build</a> &nbsp;·&nbsp;
    <a href="#gallery">Gallery</a> &nbsp;·&nbsp;
    <a href="Docs/Architecture.md">Architecture</a> &nbsp;·&nbsp;
    <a href="Docs/Developments.md">Development</a>
</p>

Physara is a personal C++20 rendering project focused on light, materials, and static scenes.
The aim is to create convincing images through rendering algorithms that remain clear enough to study, refine, and maintain.

The design draws on [Filament](Docs/Reference/Filament_Rendering_Pipeline_Analysis.md)
and [NVRHI](Docs/Reference/NVRHI_RHI_Vulkan_Analysis.md), with simplicity guiding the architecture and editing workflow.

## Build

Requires **Windows x64**, **Visual Studio 2022 / MSVC** with the C++ desktop workload and a Windows SDK, and **CMake 3.24+**.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --parallel
cmake --build build --config Release --parallel
```

Dependencies are included in `ThirdParty`; configuration does not download packages.
Build artifacts stay in `build`.
See [dependency sources and licenses](ThirdParty/README.md) and [development guidelines](AGENTS.md) for details.

The current first-party targets are `PhysaraCore` and `PhysaraPlatform` static libraries.
Core provides typed identities, owned bounded errors, monotonic CPU timestamps, and a bounded diagnostic log.
Platform creates and verifies a real OpenGL 4.6 Core context and reports its actual startup properties.
The Runtime and editor executable, input snapshots, and RHI device arrive in later work packages.

The host API is declared in [DeviceHost.hpp](Platform/DeviceHost.hpp).
Create Diagnostics first, call `DeviceHost::Create` from the process main thread, and keep Diagnostics alive until the host closes.
The host exclusively owns the process GLFW lifecycle; use the same thread for host operations and destruction.
Creation returns either a complete owner or a contextual error. `GetInfo` returns an owned snapshot;
explicit `Shutdown` is idempotent and reports cleanup failures. Hidden windows use the same native creation path and still require a window system and a capable driver.

## Gallery

Renders and editor captures.

<!-- Add images from Assets/Gallery below, using ![Caption](Assets/Gallery/Filename.png). -->
