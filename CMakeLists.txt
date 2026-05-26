cmake_minimum_required(VERSION 3.21)

# Pin the macOS deployment target (must be set before project()). If left unset
# CMake defaults it to the host OS — e.g. minos 15.6 — which makes the app link
# as a current-OS binary, so system frameworks take their newest code paths. One
# such path (ShareKit's share-sheet image-preview writer) SIGBUSes. Matching
# Qt's own build (minos 11.0) / the legacy qmake build avoids it. The CMake
# preset sets the same value; this is the fallback for non-preset configures.
if(NOT DEFINED CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS version")
endif()

project(Winnow LANGUAGES C CXX)

if(APPLE)
    enable_language(OBJCXX)
endif()

# -----------------------------------------------------------------------------
# SDK sanity guard (macOS)
#   Warn (never fail) when building against a macOS SDK newer than this Mac's
#   OS. A too-new SDK makes Apple frameworks take newer "linked-on-or-after"
#   code paths at runtime, which can misbehave on the older OS. This catches
#   the common version-skew of a newer Xcode (one SDK only) on an older macOS.
# -----------------------------------------------------------------------------
if(APPLE)
    if(CMAKE_OSX_SYSROOT)
        set(_winnow_sdk_arg "${CMAKE_OSX_SYSROOT}")
    else()
        set(_winnow_sdk_arg "macosx")
    endif()
    execute_process(COMMAND xcrun --sdk "${_winnow_sdk_arg}" --show-sdk-version
        OUTPUT_VARIABLE _winnow_sdk_version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    execute_process(COMMAND sw_vers -productVersion
        OUTPUT_VARIABLE _winnow_os_version OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(_winnow_sdk_version AND _winnow_os_version)
        string(REGEX MATCH "^[0-9]+" _winnow_sdk_major "${_winnow_sdk_version}")
        string(REGEX MATCH "^[0-9]+" _winnow_os_major  "${_winnow_os_version}")
        if(_winnow_sdk_major GREATER _winnow_os_major)
            message(WARNING
                "macOS SDK ${_winnow_sdk_version} is newer than this Mac's OS "
                "${_winnow_os_version}. System frameworks may take newer code paths at "
                "runtime (this previously crashed ShareKit). To build against an SDK "
                "matching the OS, switch the active toolchain "
                "(`sudo xcode-select --switch /Library/Developer/CommandLineTools`) or set "
                "-DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX${_winnow_os_major}.sdk")
        endif()
    endif()
endif()

# -----------------------------------------------------------------------------
# Standards / Qt tooling
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# Work around Qt's FindWrapOpenGL.cmake (Qt 6.x) unconditionally adding
# `-framework AGL` on macOS. AGL was removed from macOS years ago and the
# stub framework has no binary, so the linker fails. Pre-creating the
# WrapOpenGL::WrapOpenGL target makes Qt's find module short-circuit
# (see lines 6-9 of FindWrapOpenGL.cmake).
if(APPLE AND NOT TARGET WrapOpenGL::WrapOpenGL)
    add_library(WrapOpenGL::WrapOpenGL INTERFACE IMPORTED GLOBAL)
    target_link_libraries(WrapOpenGL::WrapOpenGL INTERFACE "-framework OpenGL")
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core Widgets Multimedia MultimediaWidgets Concurrent Svg Test
)

qt_standard_project_setup()

# -----------------------------------------------------------------------------
# Sources (cross-platform)
# -----------------------------------------------------------------------------
set(WINNOW_SOURCES
    # Cache
    Cache/cachedata.cpp
    Cache/framedecoder.cpp
    Cache/imagecache.cpp
    Cache/imagedecoder.cpp
    Cache/metaread.cpp
    Cache/reader.cpp

    # Datamodel
    Datamodel/buildfilters.cpp
    Datamodel/datamodel.cpp
    Datamodel/filters.cpp
    Datamodel/selection.cpp

    # Dialogs
    Dialogs/aboutdlg.cpp
    Dialogs/addthumbnailsdlg.cpp
    Dialogs/appdlg.cpp
    Dialogs/copystyledlg.cpp
    Dialogs/editlistdlg.cpp
    Dialogs/erasememcardimagesdlg.cpp
    Dialogs/findduplicatesdlg.cpp
    Dialogs/imagedlg.cpp
    Dialogs/ingestdlg.cpp
    Dialogs/ingesterrors.cpp
    Dialogs/loadusbdlg.cpp
    Dialogs/loupeinfodlg.cpp
    Dialogs/managegraphicsdlg.cpp
    Dialogs/manageimagesdlg.cpp
    Dialogs/managetilesdlg.cpp
    Dialogs/patterndlg.cpp
    Dialogs/preferencesdlg.cpp
    Dialogs/renamedlg.cpp
    Dialogs/saveasdlg.cpp
    Dialogs/selectionorpicksdlg.cpp
    Dialogs/testaligndlg.cpp
    Dialogs/tokendlg.cpp
    Dialogs/updateapp.cpp
    Dialogs/workspacedlg.cpp
    Dialogs/zoomdlg.cpp

    # Effects / Embellish
    Effects/effects.cpp
    Effects/graphicseffect.cpp
    Effects/graphicsitemeventfilter.cpp
    Embellish/embel.cpp
    Embellish/embelexport.cpp
    Embellish/Properties/embelproperties.cpp

    # File
    File/bookmarks.cpp
    File/fstree.cpp
    File/hoverdelegate.cpp
    File/ingest.cpp

    # FocusStack
    FocusStack/fs.cpp
    FocusStack/fsalign.cpp
    FocusStack/fsdepth.cpp
    FocusStack/fsfocus.cpp
    FocusStack/fsfusion.cpp
    FocusStack/fsfusiondmap.cpp
    FocusStack/fsfusionpmax.cpp
    FocusStack/fsfusionreassign.cpp
    FocusStack/fsfusionwavelet.cpp
    FocusStack/fsloader.cpp
    FocusStack/fsmerge.cpp
    FocusStack/fsphotometric.cpp
    FocusStack/fsutilities.cpp
    FocusStack/fusionpyr.cpp
    FocusStack/generateFocusStack.cpp

    # Image
    Image/autonomousimage.cpp
    Image/imagealign.cpp
    Image/pixmap.cpp
    Image/stack.cpp
    Image/thumb.cpp

    # ImageFormats
    ImageFormats/Base/rawbase.cpp
    ImageFormats/Canon/canon.cpp
    ImageFormats/Canon/canoncr3.cpp
    ImageFormats/Dng/dng.cpp
    ImageFormats/Fuji/fuji.cpp
    ImageFormats/Heic/Heic.cpp
    ImageFormats/Jpeg/jpeg.cpp
    ImageFormats/Nikon/nikon.cpp
    ImageFormats/Olympus/olympus.cpp
    ImageFormats/Panasonic/panasonic.cpp
    ImageFormats/Png/png.cpp
    ImageFormats/Sony/sony.cpp
    ImageFormats/Tiff/rorytiff.cpp
    ImageFormats/Tiff/tiff.cpp
    ImageFormats/Tiff/tiffdecoder.cpp
    ImageFormats/Video/mov.cpp
    ImageFormats/Video/mp4.cpp

    # ImageStack
    ImageStack/focushalo.cpp
    ImageStack/focusstackutilities.cpp
    ImageStack/focuswavelet.cpp
    ImageStack/stack_depth_map.cpp
    ImageStack/stack_focus_measure.cpp
    ImageStack/stackaligner.cpp
    ImageStack/stackcontroller.cpp
    ImageStack/stackfusion.cpp

    # Lcms2 (C)
    Lcms2/cmsalpha.c
    Lcms2/cmscam02.c
    Lcms2/cmscgats.c
    Lcms2/cmscnvrt.c
    Lcms2/cmserr.c
    Lcms2/cmsgamma.c
    Lcms2/cmsgmt.c
    Lcms2/cmshalf.c
    Lcms2/cmsintrp.c
    Lcms2/cmsio0.c
    Lcms2/cmsio1.c
    Lcms2/cmslut.c
    Lcms2/cmsmd5.c
    Lcms2/cmsmtrx.c
    Lcms2/cmsnamed.c
    Lcms2/cmsopt.c
    Lcms2/cmspack.c
    Lcms2/cmspcs.c
    Lcms2/cmsplugin.c
    Lcms2/cmsps2.c
    Lcms2/cmssamp.c
    Lcms2/cmssm.c
    Lcms2/cmstypes.c
    Lcms2/cmsvirt.c
    Lcms2/cmswtpnt.c
    Lcms2/cmsxform.c

    # Log
    Log/issue.cpp
    Log/log.cpp

    # Main
    Main/diagnostics.cpp
    Main/dockwidget.cpp
    Main/draganddrop.cpp
    Main/externalapp.cpp
    Main/fileoperations.cpp
    Main/global.cpp
    Main/initialize.cpp
    Main/main.cpp
    Main/mainwindow.cpp
    Main/menusandactions.cpp
    Main/navigate.cpp
    Main/pick.cpp
    Main/qtlocalpeer.cpp
    # qtlockedfile.cpp is #included by qtlocalpeer.cpp — do not compile standalone
    Main/qtsingleapplication.cpp
    Main/setandupdate.cpp
    Main/settings.cpp
    Main/slideshow.cpp
    Main/sortandfilter.cpp
    Main/status.cpp
    Main/test.cpp
    Main/viewmodes.cpp
    Main/widgetcss.cpp
    Main/workspaces.cpp

    # Metadata
    Metadata/exif.cpp
    Metadata/exiftool.cpp
    Metadata/gps.cpp
    Metadata/ifd.cpp
    Metadata/iptc.cpp
    Metadata/irb.cpp
    Metadata/metadata.cpp
    Metadata/metareport.cpp
    Metadata/xmp.cpp

    # PropertyEditor
    PropertyEditor/preferences.cpp
    PropertyEditor/propertydelegate.cpp
    PropertyEditor/propertyeditor.cpp
    PropertyEditor/propertywidgets.cpp

    # Utilities
    Utilities/bit.cpp
    Utilities/classificationlabel.cpp
    Utilities/coloranalysis.cpp
    Utilities/dirwatcher.cpp
    Utilities/dropshadowlabel.cpp
    Utilities/focuspointtrainer.cpp
    Utilities/focuspredictor.cpp
    Utilities/foldercompressor.cpp
    Utilities/htmlwindow.cpp
    Utilities/icc.cpp
    Utilities/inputdlg.cpp
    Utilities/performance.cpp
    Utilities/popup.cpp
    Utilities/progressbar.cpp
    Utilities/renamefile.cpp
    Utilities/reportdialog.cpp
    Utilities/showhelp.cpp
    Utilities/stresstest.cpp
    Utilities/usbutil.cpp
    Utilities/utilities.cpp

    # Views
    Views/compareImages.cpp
    Views/compareview.cpp
    Views/iconview.cpp
    Views/iconviewdelegate.cpp
    Views/imageview.cpp
    Views/infostring.cpp
    Views/infoview.cpp
    Views/tableview.cpp
    Views/videoview.cpp
    Views/videowidget.cpp
)

# -----------------------------------------------------------------------------
# Headers (cross-platform). Listed so they show up in IDEs and trigger AUTOMOC.
# -----------------------------------------------------------------------------
set(WINNOW_HEADERS
    Cache/cachedata.h
    Cache/framedecoder.h
    Cache/imagecache.h
    Cache/imagedecoder.h
    Cache/metaread.h
    Cache/reader.h

    Datamodel/buildfilters.h
    Datamodel/datamodel.h
    Datamodel/filters.h
    Datamodel/selection.h

    Dialogs/aboutdlg.h
    Dialogs/addthumbnailsdlg.h
    Dialogs/appdlg.h
    Dialogs/copystyledlg.h
    Dialogs/editlistdlg.h
    Dialogs/erasememcardimagesdlg.h
    Dialogs/findduplicatesdlg.h
    Dialogs/imagedlg.h
    Dialogs/ingestdlg.h
    Dialogs/ingesterrors.h
    Dialogs/loadusbdlg.h
    Dialogs/loupeinfodlg.h
    Dialogs/managegraphicsdlg.h
    Dialogs/manageimagesdlg.h
    Dialogs/managetilesdlg.h
    Dialogs/patterndlg.h
    Dialogs/preferencesdlg.h
    Dialogs/renamedlg.h
    Dialogs/saveasdlg.h
    Dialogs/selectionorpicksdlg.h
    Dialogs/testaligndlg.h
    Dialogs/tokendlg.h
    Dialogs/updateapp.h
    Dialogs/workspacedlg.h
    Dialogs/zoomdlg.h

    Effects/effects.h
    Effects/graphicseffect.h
    Effects/graphicsitemeventfilter.h
    Effects/interpolate.h
    Effects/REF_imageblitz_effects.h

    Embellish/embel.h
    Embellish/embelexport.h
    Embellish/Properties/embelproperties.h

    File/bookmarks.h
    File/fstree.h
    File/hoverdelegate.h
    File/ingest.h

    FocusStack/FSFusionWaveletTemplates.h
    FocusStack/fs.h
    FocusStack/fsalign.h
    FocusStack/fsalign_types.h
    FocusStack/fsdepth.h
    FocusStack/fsfocus.h
    FocusStack/fsfusion.h
    FocusStack/fsfusiondmap.h
    FocusStack/fsfusionpmax.h
    FocusStack/fsfusionreassign.h
    FocusStack/fsfusionwavelet.h
    FocusStack/fsloader.h
    FocusStack/fsmerge.h
    FocusStack/fsphotometric.h
    FocusStack/fsutilities.h
    FocusStack/fusionpyr.h

    Image/autonomousimage.h
    Image/imagealign.h
    Image/pixmap.h
    Image/stack.h
    Image/thumb.h

    ImageFormats/Base/rawbase.h
    ImageFormats/Canon/canon.h
    ImageFormats/Canon/canoncr3.h
    ImageFormats/Dng/dng.h
    ImageFormats/Fuji/fuji.h
    ImageFormats/Heic/heic.h
    ImageFormats/Heic/heif.h
    ImageFormats/Jpeg/jpeg.h
    ImageFormats/Nikon/nikon.h
    ImageFormats/Olympus/olympus.h
    ImageFormats/Panasonic/panasonic.h
    ImageFormats/Png/png.h
    ImageFormats/Sony/sony.h
    ImageFormats/Tiff/rorytiff.h
    ImageFormats/Tiff/tiff.h
    ImageFormats/Tiff/tiffdecoder.h
    ImageFormats/Video/mov.h
    ImageFormats/Video/mp4.h

    ImageStack/FocusStackConstants.h
    ImageStack/focushalo.h
    ImageStack/focusstackutilities.h
    ImageStack/focuswavelet.h
    ImageStack/stack_depth_map.h
    ImageStack/stack_focus_measure.h
    ImageStack/stackaligner.h
    ImageStack/stackcontroller.h
    ImageStack/stackfusion.h

    Lcms2/lcms2.h
    Lcms2/lcms2_plugin.h

    Log/issue.h
    Log/log.h

    Main/dockwidget.h
    Main/global.h
    Main/mainwindow.h
    Main/qtlocalpeer.h
    Main/qtlockedfile.h
    Main/qtsingleapplication.h
    Main/widgetcss.h

    Metadata/exif.h
    Metadata/exiftool.h
    Metadata/gps.h
    Metadata/ifd.h
    Metadata/imagemetadata.h
    Metadata/iptc.h
    Metadata/irb.h
    Metadata/metadata.h
    Metadata/metareport.h
    Metadata/rapidxml.h
    Metadata/rapidxml_print_rgh.h
    Metadata/xmp.h

    PropertyEditor/preferences.h
    PropertyEditor/propertydelegate.h
    PropertyEditor/propertyeditor.h
    PropertyEditor/propertywidgets.h

    Utilities/bit.h
    Utilities/classificationlabel.h
    Utilities/coloranalysis.h
    Utilities/dirwatcher.h
    Utilities/dropshadowlabel.h
    Utilities/focuspointtrainer.h
    Utilities/focuspredictor.h
    Utilities/foldercompressor.h
    Utilities/htmlwindow.h
    Utilities/icc.h
    Utilities/inputdlg.h
    Utilities/movingavg.h
    Utilities/performance.h
    Utilities/popup.h
    Utilities/progressbar.h
    Utilities/renamefile.h
    Utilities/reportdialog.h
    Utilities/showhelp.h
    Utilities/stresstest.h
    Utilities/usbutil.h
    Utilities/utilities.h

    Views/compareImages.h
    Views/compareview.h
    Views/iconview.h
    Views/iconviewdelegate.h
    Views/imageview.h
    Views/infostring.h
    Views/infoview.h
    Views/tableview.h
    Views/videoview.h
    Views/videowidget.h
)

# -----------------------------------------------------------------------------
# UI forms
# -----------------------------------------------------------------------------
set(WINNOW_FORMS
    Dialogs/aboutdlg.ui
    Dialogs/addthumbnailsdlg.ui
    Dialogs/aligndlg.ui
    Dialogs/appdlg.ui
    Dialogs/copystyledlg.ui
    Dialogs/editlistdlg.ui
    Dialogs/erasememcardimagesdlg.ui
    Dialogs/findduplicatesdlg.ui
    Dialogs/imagedlg.ui
    Dialogs/ingestdlg.ui
    Dialogs/ingesterrors.ui
    Dialogs/loadusbdlg.ui
    Dialogs/loupeinfodlg.ui
    Dialogs/managegraphicsdlg.ui
    Dialogs/manageimagesdlg.ui
    Dialogs/managetilesdlg.ui
    Dialogs/patterndlg.ui
    Dialogs/renamedlg.ui
    Dialogs/renamefiledlg.ui
    Dialogs/saveasdlg.ui
    Dialogs/selectionorpicksdlg.ui
    Dialogs/test1.ui
    Dialogs/test12.ui
    Dialogs/testaligndlg.ui
    Dialogs/testdlg.ui
    Dialogs/tokendlg.ui
    Dialogs/updateapp.ui
    Dialogs/workspacedlg.ui
    Dialogs/zoomdlg.ui
    Embellish/embelCoord.ui
    Help/message.ui
    Help/shortcutsform.ui
    Help/welcome.ui
    Metadata/metadatareport.ui
    Utilities/inputdlg.ui
)

# Gather all .ui files into a single flat "UI" node in the IDE tree (like
# qmake's Forms node) instead of scattering them across their source folders.
# Display-only; no TREE = flat list. Mirrors the `notes` source_group below.
source_group("UI" FILES ${WINNOW_FORMS})

set(WINNOW_RESOURCES winnow.qrc)

# -----------------------------------------------------------------------------
# Notes folder — listed so it shows up in the IDE project tree.
# Not compiled, not processed by AUTOMOC/AUTOUIC/AUTORCC.
# -----------------------------------------------------------------------------
file(GLOB_RECURSE WINNOW_NOTES
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    CONFIGURE_DEPENDS
    "notes/*")
set_source_files_properties(${WINNOW_NOTES} PROPERTIES
    HEADER_FILE_ONLY TRUE
    SKIP_AUTOGEN TRUE)
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/notes PREFIX "notes" FILES ${WINNOW_NOTES})

# -----------------------------------------------------------------------------
# Platform-specific sources / headers
# -----------------------------------------------------------------------------
if(APPLE)
    list(APPEND WINNOW_SOURCES
        Cache/framedecoder_mac.mm
        Image/thumb_mac.mm
        ImageFormats/Jpeg/jpeg2.cpp
        ImageFormats/Jpeg/jpegturbo.cpp
        ImageFormats/Jpeg/jpgdecoder.cpp
        ImageFormats/Tiff/libtiff.cpp
        # qtlockedfile_unix.cpp is #included by qtlocalpeer.cpp — do not compile standalone
        Utilities/mac.mm
    )
    list(APPEND WINNOW_HEADERS
        ImageFormats/Jpeg/decoderJpg.h
        ImageFormats/Jpeg/jpeg2.h
        ImageFormats/Jpeg/jpgdecoder.h
        ImageFormats/Jpeg/jpegturbo.h
        ImageFormats/Tiff/libtiff.h
        Utilities/mac.h
    )
elseif(WIN32)
    list(APPEND WINNOW_SOURCES
        Main/qtlockedfile_win.cpp
        Main/winnativeeventfilter.cpp
        Utilities/win.cpp
    )
    list(APPEND WINNOW_HEADERS
        ImageFormats/Heic/de265.h
        Main/winnativeeventfilter.h
        Utilities/win.h
    )
endif()

# -----------------------------------------------------------------------------
# Target
# -----------------------------------------------------------------------------
qt_add_executable(Winnow
    MACOSX_BUNDLE
    WIN32                       # ignored on non-Windows
    ${WINNOW_SOURCES}
    ${WINNOW_HEADERS}
    ${WINNOW_FORMS}
    ${WINNOW_RESOURCES}
    ${WINNOW_NOTES}
)

target_compile_definitions(Winnow PRIVATE
    QT_DISABLE_DEPRECATED_BEFORE=0x051500
)

target_include_directories(Winnow PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    Dialogs
    Utilities
    FocusStack
)

# qmake puts all generated ui_*.h headers in a single UI_DIR, so any source
# can include any of them. CMake AUTOUIC by default only searches alongside
# the .h that includes ui_*.h. Add every directory containing .ui files.
set_target_properties(Winnow PROPERTIES
    AUTOUIC_SEARCH_PATHS
        "${CMAKE_CURRENT_SOURCE_DIR}/Dialogs;${CMAKE_CURRENT_SOURCE_DIR}/Embellish;${CMAKE_CURRENT_SOURCE_DIR}/Help;${CMAKE_CURRENT_SOURCE_DIR}/Metadata;${CMAKE_CURRENT_SOURCE_DIR}/Utilities"
)

target_link_libraries(Winnow PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
    Qt6::Concurrent
    Qt6::Svg
    Qt6::Test
)

# Mirrors `macx-clang: QMAKE_CXXFLAGS += -include arm_acle.h`
if(APPLE)
    target_compile_options(Winnow PRIVATE -include arm_acle.h)
endif()

# Release hardening (mirrors the .pro)
target_compile_options(Winnow PRIVATE
    "$<$<AND:$<CONFIG:Release>,$<PLATFORM_ID:Darwin>>:-fstack-protector-strong>"
    "$<$<AND:$<CONFIG:Release>,$<PLATFORM_ID:Darwin>>:-D_FORTIFY_SOURCE=2>"
    "$<$<AND:$<CONFIG:Release>,$<CXX_COMPILER_ID:MSVC>>:/sdl>"
)

# Suppress MSVC warning C4138 ("*/ found outside of comment") to match the .pro
if(MSVC)
    target_compile_options(Winnow PRIVATE /wd4138 /Zc:__cplusplus)
endif()

# =============================================================================
# macOS dependencies + bundle resources
# =============================================================================
if(APPLE)
    set(HOMEBREW_PREFIX "/opt/homebrew" CACHE PATH "Homebrew prefix")

    # System frameworks
    target_link_libraries(Winnow PRIVATE
        "-framework ApplicationServices"
        "-framework AppKit"
        "-framework CoreFoundation"
        "-framework Foundation"
        "-framework ImageIO"
        "-framework CoreGraphics"
        "-framework AVFoundation"
        "-framework CoreMedia"
    )

    # OpenCV (Homebrew). Linked directly to avoid OpenCV's CMake config pulling
    # in transitive deps like -framework AGL (Apple's removed OpenGL framework).
    # Mirrors the explicit LIBS approach in winnow64.pro.
    target_link_directories(Winnow PRIVATE ${HOMEBREW_PREFIX}/lib)
    target_link_libraries(Winnow PRIVATE
        opencv_core.411
        opencv_imgproc.411
        opencv_imgcodecs
        opencv_video
        opencv_dnn.411
    )
    target_include_directories(Winnow PRIVATE
        ${HOMEBREW_PREFIX}/opt/opencv/include/opencv4)

    # libtiff (Homebrew keg-only)
    find_library(TIFF_LIB tiff
        PATHS ${HOMEBREW_PREFIX}/opt/libtiff/lib NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(Winnow PRIVATE ${TIFF_LIB})
    target_include_directories(Winnow PRIVATE
        ${HOMEBREW_PREFIX}/opt/libtiff/include)

    # libjpeg-turbo (Homebrew keg-only)
    find_library(TURBOJPEG_LIB turbojpeg
        PATHS ${HOMEBREW_PREFIX}/opt/jpeg-turbo/lib NO_DEFAULT_PATH REQUIRED)
    target_link_libraries(Winnow PRIVATE ${TURBOJPEG_LIB})
    target_include_directories(Winnow PRIVATE
        ${HOMEBREW_PREFIX}/opt/jpeg-turbo/include)

    # zlib (system)
    find_package(ZLIB REQUIRED)
    target_link_libraries(Winnow PRIVATE ZLIB::ZLIB)

    # ---- Bundle settings ----
    set_target_properties(Winnow PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME           "Winnow"
        MACOSX_BUNDLE_GUI_IDENTIFIER        "com.winnow.Winnow"
        MACOSX_BUNDLE_ICON_FILE             "winnow.icns"
    )

    # App icon -> Contents/Resources/winnow.icns
    target_sources(Winnow PRIVATE images/winnow.icns)
    set_source_files_properties(images/winnow.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")

    # ---- Bundle data: copy ReleaseExtras into the .app ----
    # Mirrors QMAKE_BUNDLE_DATA in the .pro.
    function(winnow_bundle_tree src_dir dest_subdir)
        file(GLOB_RECURSE _files
            RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
            "${src_dir}/*")
        foreach(f IN LISTS _files)
            # Strip the leading "ReleaseExtras/" so the layout under
            # MacOS/ matches the .pro's behaviour.
            file(RELATIVE_PATH rel "${CMAKE_CURRENT_SOURCE_DIR}/ReleaseExtras" "${CMAKE_CURRENT_SOURCE_DIR}/${f}")
            get_filename_component(rel_dir "${rel}" DIRECTORY)
            target_sources(Winnow PRIVATE "${f}")
            set_source_files_properties("${f}" PROPERTIES
                MACOSX_PACKAGE_LOCATION "MacOS/${rel_dir}"
                HEADER_FILE_ONLY TRUE
                SKIP_AUTOGEN TRUE)
        endforeach()
        # Keep these bundle files under one IDE tree node instead of letting
        # CMake scatter them into "Resources"/"Header Files" by file extension
        # (the embedded Winnet.app ships an Info.plist and a whole framework of
        # headers). Display-only; mirrors the `notes` source_group above.
        source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/${src_dir}
            PREFIX "${src_dir}" FILES ${_files})
    endfunction()

    winnow_bundle_tree("ReleaseExtras/ExifTool" "ExifTool")
    winnow_bundle_tree("ReleaseExtras/Winnets" "Winnets")

    # Single file: ONNX model
    target_sources(Winnow PRIVATE ReleaseExtras/focus_point_model.onnx)
    set_source_files_properties(ReleaseExtras/focus_point_model.onnx PROPERTIES
        MACOSX_PACKAGE_LOCATION "MacOS"
        HEADER_FILE_ONLY TRUE
        SKIP_AUTOGEN TRUE)
endif()

# =============================================================================
# Windows dependencies (mirrors the win32 block in the .pro)
# =============================================================================
if(WIN32)
    set(LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Lib)

    # OpenCV (combined "world" build, same lib for Debug and Release)
    target_link_libraries(Winnow PRIVATE
        ${LIB_DIR}/opencv/windows/build/x64/vc16/lib/opencv_world4110.lib)
    target_include_directories(Winnow PRIVATE
        ${LIB_DIR}/opencv/windows/build/include)

    # Per-config dependencies (Debug vs Release paths)
    target_link_libraries(Winnow PRIVATE
        $<IF:$<CONFIG:Debug>,
            ${LIB_DIR}/libde265/debug/libde265.lib,
            ${LIB_DIR}/libde265/release/libde265.lib>
        $<IF:$<CONFIG:Debug>,
            ${LIB_DIR}/zlib/build/Debug/zlib.lib,
            ${LIB_DIR}/zlib/build/Release/zlib.lib>
        $<IF:$<CONFIG:Debug>,
            ${LIB_DIR}/libheif/debug/libheif.lib,
            ${LIB_DIR}/libheif/release/libheif.lib>
        $<IF:$<CONFIG:Debug>,
            ${LIB_DIR}/libtiff/build/libtiff/Debug/tiff.lib,
            ${LIB_DIR}/libtiff/build/libtiff/Release/tiff.lib>
    )

    target_include_directories(Winnow PRIVATE
        ${LIB_DIR}/libde265/include
        ${LIB_DIR}/zlib
        ${LIB_DIR}/zlib/build
        ${LIB_DIR}/libheif/include
        ${LIB_DIR}/libtiff/libtiff
        ${LIB_DIR}/libtiff/build/libtiff
    )

    # App icon for the .exe
    target_sources(Winnow PRIVATE images/winnow.ico)
endif()

# =============================================================================
# ThreadSanitizer (TSan) build of the app — opt-in, used by the tsan test layer.
#   Enabled via the mac-tsan preset (-DWINNOW_SANITIZE_THREAD=ON). OFF by default
#   so normal debug/release builds are untouched. Run tests/tsan/run_tsan.sh.
# =============================================================================
option(WINNOW_SANITIZE_THREAD "Build the Winnow app with ThreadSanitizer" OFF)
if(WINNOW_SANITIZE_THREAD)
    target_compile_options(Winnow PRIVATE -fsanitize=thread -fno-omit-frame-pointer -g)
    target_link_options(Winnow PRIVATE -fsanitize=thread)
endif()

# =============================================================================
# Tests (Qt Test + CTest)
#   Configure as usual, then:
#     ctest --test-dir build/<preset> --output-on-failure
#   Disable with -DWINNOW_BUILD_TESTS=OFF (does not affect the Winnow target).
# =============================================================================
option(WINNOW_BUILD_TESTS "Build the Winnow test suite" ON)
if(WINNOW_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
