# AGENTS.md

## Repo shape
- Single C++ executable: `reaction_diffusion` built from `src/main.cpp` (see `CMakeLists.txt`).

## Build/run
- Configure with CMake; build type defaults to Debug if not set.
- Debug build: `cmake -S . -B build/debug -DCMAKE_BUILD_TYPE=Debug` then `cmake --build build/debug`.
- Release build: `cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release` then `cmake --build build/release`.

## Dependencies
- CMake requires system `raylib` and `OpenMP` (`find_package` in `CMakeLists.txt`); configure fails if they are missing.

## Generated files
- `build/**` is CMake/Ninja output; do not edit or commit generated files unless explicitly asked.
