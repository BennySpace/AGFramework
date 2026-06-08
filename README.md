![AGFramework Banner](Assets/readme/banner.jpg)

**A DirectX 12 rendering framework** for experimenting with modern real-time rendering techniques and graphics architecture on Windows.

## Screenshots

![Screenshot 1](Assets/readme/screenshots/01.png)

![Screenshot 2](Assets/readme/screenshots/02.png)

![Screenshot 3](Assets/readme/screenshots/03.png)

## Goal

AGFramework is organized as a small rendering framework rather than a full game engine. Its current architecture aims to keep rendering responsibilities separated into practical modules while staying simple enough to evolve incrementally.

## Architectural Style

The framework does not follow one strict high-level pattern such as MVC or ECS. Instead, it currently uses a combination of:
- layered architecture
- subsystem decomposition
- facade-style entry points
- composition over inheritance