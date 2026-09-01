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
    # Utilities::backup carries an image's sidecars via FileOps, which keeps the develop
    # preview cache in step -- so the closure now reaches both. Qt Core only (no OpenCV,
    # no MainWindow), so the closure stays bounded as the comment above requires.
    ${CMAKE_SOURCE_DIR}/Utilities/fileops.cpp
    ${CMAKE_SOURCE_DIR}/Cache/devpreviewcache.cpp
    # The develop preview index lives in the shared local index database.
    ${CMAKE_SOURCE_DIR}/Cache/cachedb.cpp
    # The catalog is the database's second tenant, and tst_devpreview's fixtures share a
    # file with it, so the closure carries both.
    ${CMAKE_SOURCE_DIR}/Cache/catalog.cpp
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
        Qt6::Sql
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
# tst_colorgrade tests Develop/colorgrade.h (header-only: no OpenCV / develop.cpp needed).
winnow_add_unit_test(tst_colorgrade unit/tst_colorgrade.cpp)
# tst_calibrate tests Develop/calibrate.h (header-only, same as colorgrade).
winnow_add_unit_test(tst_calibrate unit/tst_calibrate.cpp)
# tst_tonecurve tests Develop/tonecurve.h (header-only; needs Qt Core only, for the
# QString encoding the sidecar and presets share).
winnow_add_unit_test(tst_tonecurve unit/tst_tonecurve.cpp)
# tst_denoiseraw pins the tri-state EditParams::denoiseRaw ("is the raw denoise wanted?")
# through editparams.h + editstack.h -- both header-only, so Qt Core (JSON) is enough.
winnow_add_unit_test(tst_denoiseraw unit/tst_denoiseraw.cpp)
# tst_sharpen tests Develop/sharpen.h (header-only: no OpenCV / develop.cpp needed).
winnow_add_unit_test(tst_sharpen unit/tst_sharpen.cpp)
# tst_detailroi tests Develop/detailroi.h -- the Detail 1:1 preview's ROI mapping
# (header-only integer geometry, same closure as tst_sharpen).
winnow_add_unit_test(tst_detailroi unit/tst_detailroi.cpp)
# tst_localcontrast is the CHARACTERIZATION gate for the local-contrast family: it pins
# what Texture and Dehaze rendered before they were extracted onto the shared kernel, so
# it compiles develop.cpp (and therefore needs OpenCV) like tst_renderstack does. It also
# unit-tests the header-only Develop/localcontrast.h kernel.
winnow_add_unit_test(tst_localcontrast unit/tst_localcontrast.cpp
    ${CMAKE_SOURCE_DIR}/Develop/develop.cpp
    ${CMAKE_SOURCE_DIR}/Develop/whitebalance.cpp)
if(APPLE)
    target_include_directories(tst_localcontrast PRIVATE
        ${WINNOW_OPENCV_PREFIX}/include/opencv4)
    target_link_directories(tst_localcontrast PRIVATE ${WINNOW_OPENCV_PREFIX}/lib)
    target_link_libraries(tst_localcontrast PRIVATE opencv_core opencv_imgproc)
elseif(WIN32)
    target_include_directories(tst_localcontrast PRIVATE
        ${LIB_DIR}/opencv/windows/build/include)
    target_link_libraries(tst_localcontrast PRIVATE
        ${LIB_DIR}/opencv/windows/build/x64/vc16/lib/opencv_world4110.lib)
endif()
# tst_maskfalloff tests Develop/maskfalloff.h + the brush dab that rides it (both
# header-only: no OpenCV / develop.cpp needed).
winnow_add_unit_test(tst_maskfalloff unit/tst_maskfalloff.cpp)
# tst_maskedge tests Develop/maskedge.h, the grow/shrink morphology behind the Edge
# slider (header-only, and deliberately free of Qt apart from the test harness).
winnow_add_unit_test(tst_maskedge unit/tst_maskedge.cpp)
# tst_maskhalo tests Develop/maskhalo.h, the halo suppression behind the Halo slider.
# Header-only like tst_maskedge, but its Scharr/dilate/box passes are OpenCV, so it takes
# the same extra link/include as tst_localcontrast rather than widening the shared
# closure.
winnow_add_unit_test(tst_maskhalo unit/tst_maskhalo.cpp)
if(APPLE)
    target_include_directories(tst_maskhalo PRIVATE
        ${WINNOW_OPENCV_PREFIX}/include/opencv4)
    target_link_directories(tst_maskhalo PRIVATE ${WINNOW_OPENCV_PREFIX}/lib)
    target_link_libraries(tst_maskhalo PRIVATE opencv_core opencv_imgproc)
elseif(WIN32)
    target_include_directories(tst_maskhalo PRIVATE
        ${LIB_DIR}/opencv/windows/build/include)
    target_link_libraries(tst_maskhalo PRIVATE
        ${LIB_DIR}/opencv/windows/build/x64/vc16/lib/opencv_world4110.lib)
endif()
# tst_devpreview tests Cache/devpreviewcache.cpp, the on-disk cache of develop
# previews (already in the shared closure above).
winnow_add_unit_test(tst_devpreview unit/tst_devpreview.cpp)
# tst_fileops tests Utilities/fileops.cpp, the choke point every image file operation goes
# through (already in the shared closure above).
winnow_add_unit_test(tst_fileops unit/tst_fileops.cpp)
# tst_whitebalance compiles Develop/whitebalance.cpp (depends only on workingimage.h).
winnow_add_unit_test(tst_whitebalance unit/tst_whitebalance.cpp
    ${CMAKE_SOURCE_DIR}/Develop/whitebalance.cpp)
# tst_renderstack exercises the Develop scope compositor's interactive RESUME path, so it
# compiles the real render closure (workingimagecache + develop + whitebalance +
# outputtransform). That closure needs OpenCV (develop.cpp's blurs) -- the only unit test
# that does, hence the extra link/include below rather than widening the shared closure.
winnow_add_unit_test(tst_renderstack unit/tst_renderstack.cpp
    ${CMAKE_SOURCE_DIR}/Develop/workingimagecache.cpp
    ${CMAKE_SOURCE_DIR}/Develop/develop.cpp
    ${CMAKE_SOURCE_DIR}/Develop/whitebalance.cpp
    ${CMAKE_SOURCE_DIR}/Develop/outputtransform.cpp)
if(APPLE)
    # Same prefix the app resolved (keg-only opencv@4 when present) -- see the OpenCV
    # block in the top-level CMakeLists; a test linking a DIFFERENT OpenCV than the code
    # under test is exactly the mismatch that strands protobuf/abseil at launch.
    target_include_directories(tst_renderstack PRIVATE
        ${WINNOW_OPENCV_PREFIX}/include/opencv4)
    target_link_directories(tst_renderstack PRIVATE ${WINNOW_OPENCV_PREFIX}/lib)
    target_link_libraries(tst_renderstack PRIVATE opencv_core opencv_imgproc)
elseif(WIN32)
    target_include_directories(tst_renderstack PRIVATE
        ${LIB_DIR}/opencv/windows/build/include)
    target_link_libraries(tst_renderstack PRIVATE
        ${LIB_DIR}/opencv/windows/build/x64/vc16/lib/opencv_world4110.lib)
endif()

# tst_inputprofile guards Develop's stage 0 -- the camera-native -> working-space
# conversion that RawColor stopped doing. Its central assertion is that the transform
# folded into PointCoeffs::preMat equals applying the stages one at a time, since the
# pipeline picks between those two routes depending on whether Denoise is active.
# Same OpenCV wiring as tst_renderstack: it links develop.cpp, which uses cv::.
winnow_add_unit_test(tst_inputprofile unit/tst_inputprofile.cpp
    ${CMAKE_SOURCE_DIR}/Develop/develop.cpp
    ${CMAKE_SOURCE_DIR}/Develop/whitebalance.cpp
    ${CMAKE_SOURCE_DIR}/Develop/outputtransform.cpp)
if(APPLE)
    target_include_directories(tst_inputprofile PRIVATE
        ${WINNOW_OPENCV_PREFIX}/include/opencv4)
    target_link_directories(tst_inputprofile PRIVATE ${WINNOW_OPENCV_PREFIX}/lib)
    target_link_libraries(tst_inputprofile PRIVATE opencv_core opencv_imgproc)
elseif(WIN32)
    target_include_directories(tst_inputprofile PRIVATE
        ${LIB_DIR}/opencv/windows/build/include)
    target_link_libraries(tst_inputprofile PRIVATE
        ${LIB_DIR}/opencv/windows/build/x64/vc16/lib/opencv_world4110.lib)
endif()

# tst_outputtransform guards the 8-bit pack's transfer LUT -- an APPROXIMATION of the
# per-pixel pow it replaced (see the TRANSFER LUT note in outputtransform.cpp), so the
# accuracy bound it was accepted under has to be asserted somewhere. outputtransform.cpp
# depends only on Qt, so no OpenCV closure is needed.
winnow_add_unit_test(tst_outputtransform unit/tst_outputtransform.cpp
    ${CMAKE_SOURCE_DIR}/Develop/outputtransform.cpp)

# tst_exiftags also compiles Metadata/exif.cpp (its only dependency is exif.h).
winnow_add_unit_test(tst_exiftags  unit/tst_exiftags.cpp ${CMAKE_SOURCE_DIR}/Metadata/exif.cpp)
# tst_tiffwalk compiles ImageFormats/Raw/tiffwalk.cpp (Qt Core only -- it reads a QFile).
# It pins TIFF type decoding in Reader::reals(), where an unnamed type falls to a silent
# integer default: that is how DNG NoiseProfile (DOUBLE) reached PMRID as ~1e9.
winnow_add_unit_test(tst_tiffwalk  unit/tst_tiffwalk.cpp
    ${CMAKE_SOURCE_DIR}/ImageFormats/Raw/tiffwalk.cpp)
# tst_ifd compiles Metadata/ifd.cpp + metareport.cpp (ifd's link closure).
winnow_add_unit_test(tst_ifd       unit/tst_ifd.cpp
    ${CMAKE_SOURCE_DIR}/Metadata/ifd.cpp
    ${CMAKE_SOURCE_DIR}/Metadata/metareport.cpp)
# tst_xmpkeywords compiles Metadata/xmp.cpp. It pins the keyword read that every raw
# format used to miss: dc:subject reached only the JPEG/PNG parsers, and Lightroom's
# lr:hierarchicalSubject was not in the vocabulary at all. Also covers the shared
# Metadata/xmpapply.h that replaced eight copies of the same XMP block.
winnow_add_unit_test(tst_xmpkeywords unit/tst_xmpkeywords.cpp
    ${CMAKE_SOURCE_DIR}/Metadata/xmp.cpp)
# tst_searchterms is header-only (Utilities/searchterms.h, Qt Core). It pins the ONE
# grammar behind both search boxes: F2 over the datamodel and Shift+F2 over the index are
# paired as "Folders"/"Catalog", and used to disagree -- "heron OR eagle" was a literal
# string to QString::contains.
winnow_add_unit_test(tst_searchterms unit/tst_searchterms.cpp)
# tst_catalog covers the catalog tables in the shared index database (schema 4).
# Catalog.cpp is already in WINNOW_CORE_TEST_SOURCES, so no extra source is needed.
# Several cases test the BOUNDARY with the devPreview tenant: the migration must not
# disturb devpreview rows, and clearing one tenant must not clear the other. It also
# builds a version 3 file by hand and migrates it, because an existing catalog must merge
# its duplicate keyword rows in place rather than be rebuilt.
winnow_add_unit_test(tst_catalog unit/tst_catalog.cpp)

# tst_rowfields pins Datamodel/rowfields.h -- the single declaration of an image's
# fields -- against the catalog's image table. The table exists so the model's
# columns, the SQL schema and categorySql cannot drift apart; a table nothing
# checks would drift exactly as the four hand-written lists it replaces did.
winnow_add_unit_test(tst_rowfields unit/tst_rowfields.cpp)

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
