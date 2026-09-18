# ![AGFramework Banner](Assets/readme/banner.jpg)

A small **DirectX 12** rendering framework for experimenting with modern real-time rendering techniques on Windows.

Implemented features:

- WinAPI window, message loop, and raw-input handling;
- DirectX 12 device, swap chain, command queue, and descriptor heaps;
- OBJ model loading with textures and materials;
- deferred rendering with a GBuffer;
- directional, point, and spot lights;
- cascaded shadow maps with PCF filtering;
- physically based rendering (PBR) with image-based lighting (IBL);
- a Sponza demo scene and a Dear ImGui debug overlay for inspecting and adjusting renderer settings.

## Screenshots

![Screenshot 1](Assets/readme/screenshots/01.png)
![Screenshot 2](Assets/readme/screenshots/02.png)
![Screenshot 3](Assets/readme/screenshots/03.png)

## Requirements

- Windows 10 or later with DirectX 12 support;
- Visual Studio with the MSVC v145 toolset and a Windows SDK installed;
- NuGet package restore for the native dependencies: Assimp and DirectXTK12.

## Build and Run

1. Open [AGFramework.slnx](AGFramework/AGFramework.slnx) in Visual Studio.
2. Restore the NuGet packages if Visual Studio does not do so automatically.
3. Select the `x64` platform and either the `Debug` or `Release` configuration.
4. Build and run the `AGFramework` project.

To perform a one-frame rendering smoke test without showing a window, run the built executable with `--smoke-test`.

## Controls

- `W`, `A`, `S`, `D` - move the camera;
- `Shift` - move faster;
- hold the right mouse button inside the window - capture the mouse and look around.

## Architecture

The framework is organized around a small orchestration layer and focused rendering subsystems:

- `AGFramework` owns the window, input device, frame loop, and renderer lifetime.
- `DirectX12App` coordinates per-frame updates, GPU synchronization, scene state, and rendering passes.
- `DirectX12Context` encapsulates DirectX 12 device, command-list, swap-chain, and descriptor-heap management.
- `DeferredRenderer` owns GPU resources and pipeline states for shadow, geometry, deferred-lighting, and transparent passes.
- Scene, asset, lighting, material, camera, and debug-overlay modules provide data and controls to the renderer.

## Project Structure

- `AGFramework/src/App` — application startup, frame loop, and subsystem lifetime.
- `AGFramework/src/Core` — WinAPI window, message dispatch, timer, and raw input.
- `AGFramework/src/Graphics/dx12` — DirectX 12 device, swap chain, command resources, descriptor heaps, and DX12 utilities.
- `AGFramework/src/Graphics` — rendering systems and shared rendering state.
  - `Assets`, `Resources`, `Scene` — asset paths, loading, GPU upload, and scene data.
  - `Demo` — Sponza demo composition and runtime controls.
  - `Overlay` — Dear ImGui debug UI and render diagnostics.
- `AGFramework/shaders` — HLSL shaders for geometry, lighting, and shadow passes.
- `AGFramework/external/imgui` — bundled Dear ImGui sources and platform/rendering backends.
- `Assets` — runtime assets: Sponza model, textures, IBL maps, shared materials, and icons.

## Academic Attribution

This project is developed as part of the Computer Graphics course laboratory works at the School of Video Game Development, ITMO University.
