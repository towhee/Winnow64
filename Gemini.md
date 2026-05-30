# Winnow Project Context
Version: 2.05
Architecture: Qt-based multi-threaded image browser.

## Core Components
- **Main**: `mainwindow.h` (Central orchestrator)
- **Global**: `global.h` (Enums, Mutexes, `isRory` flag)
- **Data**: `datamodel.h` (Asynchronous folder loading)

## System Goals
Focus stacking, metadata management (Exif/XMP), and high-performance caching.