# Winnow Project Rules
- Tech Stack: C++20, Qt 6, OpenCV 4.x
- **Framework:** Qt 6 (using signal-slot mechanisms)
- **Libraries:** OpenCV 4.x, ExifTool (via CLI)
- **Style:** PascalCase for methods.
- **Build System:** CMake (CMakeLists.txt + CMakePresets.json; migrated from QMake — winnow64.pro is legacy)
- **Core Goal:** Fast image browsing and high-performance focus stacking.
- Context: Running on Macbook Pro 24 GB.
- **Tests:** Qt Test + CTest under `tests/`. Build then `ctest --test-dir build/<preset> --output-on-failure`. See `tests/README.md`.