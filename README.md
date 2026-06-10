![AGFramework Banner](Assets/readme/banner.jpg)

# AGFramework

A small **DirectX 12** rendering framework for experimenting with modern real-time rendering techniques on Windows.

Implemented features:
- custom window, message loop, and input handling;
- core DX12 setup: device, swap chain, command queue, and descriptor heaps;
- `obj` model loading with textures and materials;
- deferred rendering with a `GBuffer`;
- directional, point, and spot lighting;
- cascaded shadow maps with PCF;
- PBR lighting with image-based lighting;
- `Sponza` demo scene.

## Screenshots

![Screenshot 1](Assets/readme/screenshots/01.png)
![Screenshot 2](Assets/readme/screenshots/02.png)
![Screenshot 3](Assets/readme/screenshots/03.png)

## Completed ITMO Computer Graphics Course Tasks

### DirectX 12 Part 1 Overview

- [x] Implemented a custom message loop.
- [x] Implemented a `WndProc` window procedure.
- [x] Created a window class that encapsulates WinAPI window management.
- [x] Refactored input into a dedicated `InputDevice` with raw input handling.

### DirectX 12 Part 2 Device and Resources

- [x] Built the application framework and timer.
- [x] Initialized the `Device`, `SwapChain`, and required base resources.
- [x] Implemented back buffer and depth buffer clearing.

### DirectX 12 Part 3 Rendering

- [x] Added HLSL shader compilation and loading.
- [x] Created constant buffers, root signatures, and PSOs.

### Textures

- [x] Added texture loading.
- [x] Added `obj` model rendering with materials.

### Rendering

- [x] Implemented the main rendering system.
- [x] Implemented the `GBuffer`.
- [x] Implemented deferred rendering.
- [x] Added `Directional`, `Point`, and `Spot` lights.
- [x] Added the `Sponza` scene with multiple light sources.

### Shadows

- [x] Added cascaded shadow maps.
- [x] Added PCF shadow filtering.

### Physically Based Rendering

- [x] Switched lighting to PBR.
- [x] Added IBL lighting and reflections.
- [x] Added support for `irradiance map`, `BRDF integration map`, and `prefiltered environment map`.

## Goal

AGFramework is organized as a small rendering framework rather than a full game engine. Its current architecture aims to keep rendering responsibilities separated into practical modules while staying simple enough to evolve incrementally.

## Architectural Style

The framework does not follow one strict high-level pattern such as MVC or ECS. Instead, it currently uses a combination of:
- layered architecture
- subsystem decomposition
- facade-style entry points
- composition over inheritance

## Project Structure

- `AGFramework/src/Core` - window, timer, and input systems.
- `AGFramework/src/Graphics/dx12` - core DirectX 12 context and helper classes.
- `AGFramework/src/Graphics` - deferred renderer, lighting, shadows, materials, and scene code.
- `AGFramework/shaders` - HLSL shaders.
- `Assets` - models, textures, and demo materials.