# Winnow Project Rules
- Tech Stack: C++20, Qt 6, OpenCV 4.x
- **Framework:** Qt 6 (using signal-slot mechanisms)
- **Libraries:** OpenCV 4.x, ExifTool (via CLI)
- **Style:** PascalCase for methods. Use block comments `/* ... */` for multi-line comments, not stacked `//` lines (single-line comments may use `//`).
- **Build System:** CMake (CMakeLists.txt + CMakePresets.json; migrated from QMake — winnow64.pro is legacy)
- **Core Goal:** Fast image browsing and high-performance focus stacking.
- **Cross-platform:** All changes must work effectively on macOS, Windows and Linux,
  unless the change is inherently platform-specific. When a feature needs
  platform-native code (e.g. file dialogs, mail, shell integration), provide working
  implementations for all three, guarded by `Q_OS_MAC`/`Q_OS_WIN`/`Q_OS_LINUX`. Never
  leave Linux to fall through a Mac/Windows `#if`/`#elif` pair with no branch (a
  missing `return` crashed startup). Include paths must match the file's real case
  (Linux is case-sensitive). Linux builds and tests on the Ubuntu VM — see
  "Platforms" in `notes/Documentation.md`.
- Context: Running on Macbook Pro 24 GB.
- **Tests:** Qt Test + CTest under `tests/`. Build then
  `ctest --test-dir build/<preset> --output-on-failure`. See `tests/README.md`.
- **Develop settings:** adding or changing an adjustment (Exposure, Grain, ...) touches
  ~12 files across the pipeline, sidecar, panel, presets and docs — follow "Adding or
  Changing a Develop Adjustment (Checklist)" in `notes/Documentation.md`. Preset and
  sidecar keys are a published format: add freely; renaming, rescaling or retuning one
  needs a migration step in Develop/developschema.h (see "Develop Settings Versions
  (Schema Drift)").
