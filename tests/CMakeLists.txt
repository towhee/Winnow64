# =============================================================================
# Winnow test suite — Qt Test + CTest
#
# Run after significant changes / before release:
#   cmake --build build/mac-debug
#   ctest --test-dir build/mac-debug --output-on-failure
#
# Unit tests link the production sources directly (no copy of the code under
# test). Because each .cpp is one translation unit, testing any Utilities
# function pulls in utilities.cpp's link closure. That closure is small and
# bounded (global.cpp includes only global.h + <algorithm>; no spiral into
# MainWindow). Expand WINNOW_CORE_TEST_SOURCES only if the linker asks for more.
# =============================================================================

set(WINNOW_CORE_TEST_SOURCES
    ${CMAKE_SOURCE_DIR}/Utilities/utilities.cpp
    ${CMAKE_SOURCE_DIR}/Utilities/inputdlg.cpp
    ${CMAKE_SOURCE_DIR}/Utilities/popup.cpp
    ${CMAKE_SOURCE_DIR}/Main/global.cpp
    ${CMAKE_SOURCE_DIR}/Log/log.cpp
    ${CMAKE_SOURCE_DIR}/Log/issue.cpp
)

# Windows: utilities.cpp/inputdlg.cpp call Win::setTitleBarColor (Utilities/win.cpp).
if(WIN32)
    list(APPEND WINNOW_CORE_TEST_SOURCES ${CMAKE_SOURCE_DIR}/Utilities/win.cpp)
endif()

set(WINNOW_TEST_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/Utilities
    ${CMAKE_SOURCE_DIR}/FocusStack
    ${CMAKE_SOURCE_DIR}/Dialogs
    ${CMAKE_SOURCE_DIR}/Metadata
)

# .ui files live alongside their classes; mirror the app's AUTOUIC search paths
# so any production source dragged in can resolve its generated ui_*.h.
set(CMAKE_AUTOUIC_SEARCH_PATHS
    ${CMAKE_SOURCE_DIR}/Utilities
    ${CMAKE_SOURCE_DIR}/Dialogs
)

# Register a Qt Test executable that links the shared production closure.
function(winnow_add_unit_test name)
    add_executable(${name} ${ARGN} ${WINNOW_CORE_TEST_SOURCES})
    target_include_directories(${name} PRIVATE ${WINNOW_TEST_INCLUDE_DIRS})
    target_link_libraries(${name} PRIVATE
        Qt6::Core
        Qt6::Widgets
        Qt6::Concurrent
        Qt6::Test
    )
    if(APPLE)
        # Mirror the app's `-include arm_acle.h` (some sources assume it).
        target_compile_options(${name} PRIVATE -include arm_acle.h)
    endif()
    add_test(NAME ${name} COMMAND ${name})
    # Headless: no display server required.
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
        LABELS "unit"
    )
endfunction()

winnow_add_unit_test(tst_byteops   unit/tst_byteops.cpp)
winnow_add_unit_test(tst_pathutils unit/tst_pathutils.cpp)
winnow_add_unit_test(tst_rational  unit/tst_rational.cpp)
# tst_exiftags also compiles Metadata/exif.cpp (its only dependency is exif.h).
winnow_add_unit_test(tst_exiftags  unit/tst_exiftags.cpp ${CMAKE_SOURCE_DIR}/Metadata/exif.cpp)
# tst_ifd compiles Metadata/ifd.cpp + metareport.cpp (ifd's link closure).
winnow_add_unit_test(tst_ifd       unit/tst_ifd.cpp
    ${CMAKE_SOURCE_DIR}/Metadata/ifd.cpp
    ${CMAKE_SOURCE_DIR}/Metadata/metareport.cpp)

# -----------------------------------------------------------------------------
# Smoke test — launch the real app against the fixture folder via the --selftest
# flag (main.cpp). Exits 0 only if the folder loaded images without crashing.
# CTest does NOT build the app first, so build it before running:
#   cmake --build build/<preset> --target Winnow
#   ctest --test-dir build/<preset> --output-on-failure
# -----------------------------------------------------------------------------
if(TARGET Winnow)
    add_test(
        NAME smoke_launch
        COMMAND $<TARGET_FILE:Winnow> --selftest ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/images
    )
    set_tests_properties(smoke_launch PROPERTIES
        TIMEOUT 90
        ENVIRONMENT "WINNOW_SELFTEST_MS=4000"
        LABELS "smoke"
    )

    # End-to-end metadata read: the full Metadata pipeline parses a committed real
    # camera JPEG via the --metatest flag. Expected make/model are passed by env so
    # the fixture's identity lives here, not in app code. Exits 0 on a match.
    add_test(
        NAME metadata_read
        COMMAND $<TARGET_FILE:Winnow> --metatest
                ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/images/sample_nikon_d700.jpg
    )
    set_tests_properties(metadata_read PROPERTIES
        TIMEOUT 60
        ENVIRONMENT "WINNOW_METATEST_MAKE=NIKON;WINNOW_METATEST_MODEL=D700"
        LABELS "metadata"
    )

    # Soak smoke — a few seconds of the bounce harness (--soaktest) over the
    # fixture folder, to verify the load/navigate loop runs and exits cleanly.
    # This is NOT the leak/race hunt (that's tests/soak/run_soak.sh under the
    # sanitizers); it just keeps the harness itself from rotting. Fast-exit
    # avoids teardown flakiness in the quick check.
    #
    # Opt-in (OFF by default) so the normal `ctest` run stays fast:
    #   cmake -DWINNOW_BUILD_SOAK_SMOKE=ON ... && ctest -L soak
    option(WINNOW_BUILD_SOAK_SMOKE "Register the short soak smoke CTest" OFF)
    if(WINNOW_BUILD_SOAK_SMOKE)
        add_test(
            NAME soak_smoke
            COMMAND $<TARGET_FILE:Winnow> --soaktest ${CMAKE_CURRENT_SOURCE_DIR}/fixtures/images
        )
        set_tests_properties(soak_smoke PROPERTIES
            TIMEOUT 60
            ENVIRONMENT "WINNOW_SOAK_MS=5000;WINNOW_SOAK_IMG_MS=20;WINNOW_SOAK_FAST_EXIT=1"
            LABELS "soak"
        )
    endif()
endif()
