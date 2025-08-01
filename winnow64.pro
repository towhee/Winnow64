CONFIG += sdk_no_version_check

macx:CONFIG += app_bundle

# Must rerun qmake and rebuild when change:
#CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT
#CONFIG(release, debug|release):DEFINES += QT_NO_WARNING_OUTPUT

#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000
greaterThan(QT_MAJOR_VERSION, 5) {
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x051500
#    win32:QT += core5compat   # huge performance hit without this
}

# Compile without warnings
# GCC / Clang
# QMAKE_CXXFLAGS += -Wno-unused-variable
# QMAKE_CFLAGS += -Wno-unused-variable
# MSVC
# pragma warning(disable: 4101) # 4101 is the warning number for unused variable in MSVC

#QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.13

# Compile on MacOS without permission prompts:
# - Add Build Step > Custom Process Step
# - Command: chmod
# - Arguments: +x /path/to.winnow.exe

# MacOS universal binaries
# QMAKE_APPLE_DEVICE_ARCHS = arm64
# QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64

CONFIG += c++17
win32:QMAKE_CXXFLAGS += /std:c++17
win32:QMAKE_CXXFLAGS += /Zc:__cplusplus
win32:QMAKE_CXXFLAGS += /wd4138             # supress "*/" found outside of comment

TEMPLATE = app
TARGET = Winnow
INCLUDEPATH += .
INCLUDEPATH += Dialogs
INCLUDEPATH += Utilities

INCLUDEPATH += $$PWD
DEPENDPATH += $$PWD

Release:DESTDIR = release
Release:OBJECTS_DIR = release/.obj
Release:MOC_DIR = release/.moc
Release:RCC_DIR = release/.rcc
Release:UI_DIR = release/.ui

Debug:DESTDIR = debug
Debug:OBJECTS_DIR = debug/.obj
Debug:MOC_DIR = debug/.moc
Debug:RCC_DIR = debug/.rcc
Debug:UI_DIR = debug/.ui

QT += widgets
QT += multimedia
QT += multimediawidgets
QT += concurrent

HEADERS += Cache/cachedata.h
HEADERS += Utilities/focuspointtrainer.h
HEADERS += Utilities/focuspredictor.h
HEADERS += Cache/tiffthumbdecoder.h
HEADERS += ImageFormats/Video/mov.h
HEADERS += ImageFormats/Video/mp4.h
HEADERS += ImageFormats/Png/png.h
HEADERS += Cache/framedecoder.h
HEADERS += Cache/imagecache.h
HEADERS += Cache/imagedecoder.h
HEADERS += Cache/metaread.h
HEADERS += Cache/reader.h
HEADERS += Datamodel/buildfilters.h
HEADERS += Datamodel/datamodel.h
HEADERS += Datamodel/filters.h
HEADERS += Datamodel/selection.h
HEADERS += Dialogs/aboutdlg.h
HEADERS += Dialogs/addthumbnailsdlg.h
HEADERS += Dialogs/appdlg.h
HEADERS += Dialogs/copystyledlg.h
HEADERS += Dialogs/editlistdlg.h
HEADERS += Dialogs/erasememcardimagesdlg.h
HEADERS += Dialogs/findduplicatesdlg.h
HEADERS += Dialogs/imagedlg.h
HEADERS += Dialogs/ingestdlg.h
HEADERS += Dialogs/ingesterrors.h
HEADERS += Dialogs/loadusbdlg.h
HEADERS += Dialogs/loupeinfodlg.h
HEADERS += Dialogs/managegraphicsdlg.h
HEADERS += Dialogs/manageimagesdlg.h
HEADERS += Dialogs/managetilesdlg.h
HEADERS += Dialogs/patterndlg.h
HEADERS += Dialogs/preferencesdlg.h
HEADERS += Dialogs/renamedlg.h
HEADERS += Dialogs/saveasdlg.h
HEADERS += Dialogs/selectionorpicksdlg.h
HEADERS += Dialogs/testaligndlg.h
HEADERS += Dialogs/tokendlg.h
HEADERS += Dialogs/updateapp.h
HEADERS += Dialogs/workspacedlg.h
HEADERS += Dialogs/zoomdlg.h
HEADERS += Effects/effects.h
HEADERS += Effects/graphicseffect.h
HEADERS += Embellish/embel.h
HEADERS += Effects/graphicsitemeventfilter.h
HEADERS += Effects/interpolate.h
HEADERS += Effects/REF_imageblitz_effects.h
HEADERS += Embellish/embelexport.h
HEADERS += Embellish/Properties/embelproperties.h
HEADERS += File/bookmarks.h
HEADERS += File/fstree.h
HEADERS += File/hoverdelegate.h
HEADERS += File/ingest.h
HEADERS += Image/autonomousimage.h
HEADERS += Image/imagealign.h
HEADERS += Image/pixmap.h
HEADERS += Image/stack.h
HEADERS += Image/thumb.h
#HEADERS += Image/tiffhandler.h
HEADERS += ImageFormats/Canon/canon.h
HEADERS += ImageFormats/Canon/canoncr3.h
HEADERS += ImageFormats/Dng/dng.h
HEADERS += ImageFormats/Fuji/fuji.h
#rgh remove heic
HEADERS += ImageFormats/Heic/heic.h
HEADERS += ImageFormats/Heic/heif.h
#HEADERS += ImageFormats/Heic/de265.h
#win32:HEADERS += ImageFormats/Heic/heic.h
#win32:HEADERS += ImageFormats/Heic/heif.h
win32:HEADERS += ImageFormats/Heic/de265.h
HEADERS += ImageFormats/Jpeg/jpeg.h
macx:HEADERS += ImageFormats/Jpeg/jpeg2.h
macx:HEADERS += ImageFormats/Jpeg/decoderJpg.h
macx:HEADERS += ImageFormats/Jpeg/jpgdecoder.h
macx:HEADERS += ImageFormats/jpeg/jpegturbo.h
HEADERS += ImageFormats/Nikon/nikon.h
HEADERS += ImageFormats/Olympus/olympus.h
HEADERS += ImageFormats/Panasonic/panasonic.h
HEADERS += ImageFormats/Sony/sony.h
HEADERS += ImageFormats/Tiff/tiff.h
macx:HEADERS += ImageFormats/Tiff/libtiff.h
HEADERS += ImageFormats/Tiff/rorytiff.h
HEADERS += Lcms2/lcms2.h
HEADERS += Lcms2/lcms2_plugin.h
HEADERS += Log/issue.h
HEADERS += Log/log.h
HEADERS += Main/dockwidget.h
HEADERS += Main/global.h
# HEADERS += Main/logger.h
HEADERS += Main/mainwindow.h
HEADERS += Main/qtlocalpeer.h
HEADERS += Main/qtlockedfile.h
HEADERS += Main/qtsingleapplication.h
HEADERS += Main/widgetcss.h
win32:HEADERS += Main/winnativeeventfilter.h
HEADERS += Metadata/exif.h
HEADERS += Metadata/exiftool.h
HEADERS += Metadata/irb.h
HEADERS += Metadata/gps.h
HEADERS += Metadata/ifd.h
HEADERS += Metadata/iptc.h
HEADERS += Metadata/imagemetadata.h
HEADERS += Metadata/metadata.h
HEADERS += Metadata/metareport.h
HEADERS += Metadata/rapidxml.h
HEADERS += Metadata/rapidxml_print_rgh.h
HEADERS += Metadata/xmp.h
#HEADERS += Metadata/ExifTool.h
#HEADERS += Metadata/ExifToolPipe.h
#HEADERS += Metadata/TagInfo.h
HEADERS += PropertyEditor/preferences.h
HEADERS += PropertyEditor/propertyeditor.h
HEADERS += PropertyEditor/propertydelegate.h
HEADERS += PropertyEditor/propertywidgets.h
HEADERS += Utilities/bit.h
HEADERS += Utilities/classificationlabel.h
HEADERS += Utilities/coloranalysis.h
HEADERS += Utilities/dirwatcher.h
HEADERS += Utilities/dropshadowlabel.h
HEADERS += Utilities/foldercompressor.h
HEADERS += Utilities/htmlwindow.h
HEADERS += Utilities/icc.h
macx:HEADERS += Utilities/mac.h
win32:HEADERS += Utilities/win.h
HEADERS += Utilities/inputdlg.h
HEADERS += Utilities/performance.h
HEADERS += Utilities/popup.h
HEADERS += Utilities/progressbar.h
HEADERS += Utilities/renamefile.h
HEADERS += Utilities/showhelp.h
HEADERS += Utilities/stresstest.h
HEADERS += \
    Utilities/usbutil.h
HEADERS += Utilities/utilities.h
HEADERS += Views/compareImages.h
HEADERS += Views/compareview.h
HEADERS += Views/iconview.h
HEADERS += Views/iconviewdelegate.h
HEADERS += Views/imageview.h
HEADERS += Views/infostring.h
HEADERS += Views/infoview.h
HEADERS += Views/tableview.h
HEADERS += Views/videoview.h
HEADERS += Views/videowidget.h

SOURCES += Cache/cachedata.cpp
SOURCES += Utilities/focuspointtrainer.cpp
SOURCES += Utilities/focuspredictor.cpp
SOURCES += Cache/tiffthumbdecoder.cpp
SOURCES += ImageFormats/Video/mov.cpp
SOURCES += ImageFormats/Video/mp4.cpp
SOURCES += ImageFormats/Png/png.cpp
SOURCES += Cache/framedecoder.cpp
SOURCES += Cache/imagecache.cpp
SOURCES += Cache/imagedecoder.cpp
SOURCES += Cache/metaread.cpp
SOURCES += Cache/reader.cpp
SOURCES += Datamodel/buildfilters.cpp
SOURCES += Datamodel/datamodel.cpp
SOURCES += Datamodel/filters.cpp
SOURCES += Datamodel/selection.cpp
SOURCES += Dialogs/aboutdlg.cpp
SOURCES += Dialogs/addthumbnailsdlg.cpp
SOURCES += Dialogs/appdlg.cpp
SOURCES += Dialogs/copystyledlg.cpp
SOURCES += Dialogs/editlistdlg.cpp
SOURCES += Dialogs/erasememcardimagesdlg.cpp
SOURCES += Dialogs/imagedlg.cpp
SOURCES += Dialogs/ingestdlg.cpp
SOURCES += Dialogs/ingesterrors.cpp
SOURCES += Dialogs/loadusbdlg.cpp
SOURCES += Dialogs/loupeinfodlg.cpp
SOURCES += Dialogs/managegraphicsdlg.cpp
SOURCES += Dialogs/manageimagesdlg.cpp
SOURCES += Dialogs/managetilesdlg.cpp
SOURCES += Dialogs/patterndlg.cpp
SOURCES += Dialogs/preferencesdlg.cpp
SOURCES += Dialogs/renamedlg.cpp
SOURCES += Dialogs/saveasdlg.cpp
SOURCES += Dialogs/selectionorpicksdlg.cpp
SOURCES += Dialogs/testaligndlg.cpp
SOURCES += Dialogs/tokendlg.cpp
SOURCES += Dialogs/updateapp.cpp
SOURCES += Dialogs/findduplicatesdlg.cpp
SOURCES += Dialogs/workspacedlg.cpp
SOURCES += Dialogs/zoomdlg.cpp
SOURCES += Effects/effects.cpp
SOURCES += Effects/graphicseffect.cpp
SOURCES += Effects/graphicsitemeventfilter.cpp
SOURCES += Embellish/embel.cpp
SOURCES += Embellish/Properties/embelproperties.cpp
SOURCES += Embellish/embelexport.cpp
SOURCES += File/bookmarks.cpp
SOURCES += File/fstree.cpp
SOURCES += File/hoverdelegate.cpp
SOURCES += File/ingest.cpp
SOURCES += Image/autonomousimage.cpp
SOURCES += Image/imagealign.cpp
SOURCES += Image/pixmap.cpp
SOURCES += Image/stack.cpp
SOURCES += Image/thumb.cpp
#SOURCES += Image/tiffhandler.cpp
SOURCES += ImageFormats/Canon/canon.cpp
SOURCES += ImageFormats/Canon/canoncr3.cpp
SOURCES += ImageFormats/Dng/dng.cpp
SOURCES += ImageFormats/Fuji/fuji.cpp
SOURCES += ImageFormats/Heic/Heic.cpp
#win32:SOURCES += ImageFormats/Heic/Heic.cpp
SOURCES += ImageFormats/Jpeg/jpeg.cpp               # For parsing
macx:SOURCES += ImageFormats/Jpeg/jpeg2.cpp         # Copy jpeg.h for experimenting
macx:SOURCES += ImageFormats/Jpeg/jpgdecoder.cpp    # JED project (Jpeg Encoder Decoder)
macx:SOURCES += ImageFormats/Jpeg/jpegturbo.cpp     # libjpeg-turbo
SOURCES += ImageFormats/Nikon/nikon.cpp
SOURCES += ImageFormats/Olympus/olympus.cpp
SOURCES += ImageFormats/Panasonic/panasonic.cpp
SOURCES += ImageFormats/Sony/sony.cpp
SOURCES += ImageFormats/Tiff/tiff.cpp               # For parsing
macx:SOURCES += ImageFormats/Tiff/libtiff.cpp       # use library directly
SOURCES += ImageFormats/Tiff/rorytiff.cpp           # Rory decoder
SOURCES += Lcms2/cmsalpha.c
SOURCES += Lcms2/cmscam02.c
SOURCES += Lcms2/cmscgats.c
SOURCES += Lcms2/cmscnvrt.c
SOURCES += Lcms2/cmserr.c
SOURCES += Lcms2/cmsgamma.c
SOURCES += Lcms2/cmsgmt.c
SOURCES += Lcms2/cmshalf.c
SOURCES += Lcms2/cmsintrp.c
SOURCES += Lcms2/cmsio0.c
SOURCES += Lcms2/cmsio1.c
SOURCES += Lcms2/cmslut.c
SOURCES += Lcms2/cmsmd5.c
SOURCES += Lcms2/cmsmtrx.c
SOURCES += Lcms2/cmsnamed.c
SOURCES += Lcms2/cmsopt.c
SOURCES += Lcms2/cmspack.c
SOURCES += Lcms2/cmspcs.c
SOURCES += Lcms2/cmsplugin.c
SOURCES += Lcms2/cmsps2.c
SOURCES += Lcms2/cmssamp.c
SOURCES += Lcms2/cmssm.c
SOURCES += Lcms2/cmstypes.c
SOURCES += Lcms2/cmsvirt.c
SOURCES += Lcms2/cmswtpnt.c
SOURCES += Lcms2/cmsxform.c
SOURCES += Log/issue.cpp
SOURCES += Log/log.cpp
SOURCES += Main/main.cpp
# all part of MW (mainwindow)
SOURCES += Main/mainwindow.cpp
SOURCES += Main/diagnostics.cpp
SOURCES += Main/draganddrop.cpp
SOURCES += Main/externalapp.cpp
SOURCES += Main/fileoperations.cpp
SOURCES += Main/initialize.cpp
# SOURCES += Main/log.cpp
# SOURCES += Main/logger.cpp
SOURCES += Main/menusandactions.cpp
SOURCES += Main/navigate.cpp
SOURCES += Main/pick.cpp
SOURCES += Main/setandupdate.cpp
SOURCES += Main/settings.cpp
SOURCES += Main/slideshow.cpp
SOURCES += Main/sortandfilter.cpp
SOURCES += Main/status.cpp
SOURCES += Main/test.cpp
SOURCES += Main/viewmodes.cpp
win32:SOURCES += Main/winnativeeventfilter.cpp
SOURCES += Main/workspaces.cpp
SOURCES += Main/dockwidget.cpp
SOURCES += Main/global.cpp
SOURCES += Main/qtlocalpeer.cpp
SOURCES += Main/qtlockedfile.cpp
macx:SOURCES += Main/qtlockedfile_unix.cpp
win32:SOURCES += Main/qtlockedfile_win.cpp
SOURCES += Main/qtsingleapplication.cpp
SOURCES += Main/widgetcss.cpp
SOURCES += Metadata/exif.cpp
SOURCES += Metadata/exiftool.cpp
SOURCES += Metadata/irb.cpp
SOURCES += Metadata/gps.cpp
SOURCES += Metadata/ifd.cpp
SOURCES += Metadata/iptc.cpp
SOURCES += Metadata/metadata.cpp
SOURCES += Metadata/metareport.cpp
SOURCES += Metadata/xmp.cpp
#SOURCES += Metadata/ExifTool.cpp
#SOURCES += Metadata/ExifToolPipe.cpp
#SOURCES += Metadata/TagInfo.cpp
SOURCES += PropertyEditor/preferences.cpp
SOURCES += PropertyEditor/propertyeditor.cpp
SOURCES += PropertyEditor/propertydelegate.cpp
SOURCES += PropertyEditor/propertywidgets.cpp
SOURCES += Utilities/bit.cpp
SOURCES += Utilities/classificationlabel.cpp
SOURCES += Utilities/coloranalysis.cpp
SOURCES += Utilities/dirwatcher.cpp
SOURCES += Utilities/dropshadowlabel.cpp
SOURCES += Utilities/foldercompressor.cpp
SOURCES += Utilities/htmlwindow.cpp
SOURCES += Utilities/icc.cpp
SOURCES += Utilities/inputdlg.cpp
macx:SOURCES += Utilities/mac.mm
SOURCES += Utilities/performance.cpp
SOURCES += Utilities/popup.cpp
SOURCES += Utilities/progressbar.cpp
SOURCES += Utilities/renamefile.cpp
SOURCES += Utilities/showhelp.cpp
SOURCES += Utilities/stresstest.cpp
SOURCES += \
    Utilities/usbutil.cpp
SOURCES += Utilities/utilities.cpp
win32:SOURCES += Utilities/win.cpp
SOURCES += Views/compareImages.cpp
SOURCES += Views/compareview.cpp
SOURCES += Views/iconview.cpp
SOURCES += Views/iconviewdelegate.cpp
SOURCES += Views/imageview.cpp
SOURCES += Views/infoview.cpp
SOURCES += Views/tableview.cpp
SOURCES += Views/infostring.cpp
SOURCES += Views/videoview.cpp
SOURCES += Views/videowidget.cpp

FORMS += Dialogs/aboutdlg.ui \
    Help/helppixeldelta.ui
FORMS += Dialogs/loupeinfodlg.ui
FORMS += Dialogs/addthumbnailsdlg.ui
FORMS += Dialogs/aligndlg.ui
FORMS += Dialogs/appdlg.ui
FORMS += Dialogs/copystyledlg.ui
FORMS += Dialogs/editlistdlg.ui
FORMS += Dialogs/erasememcardimagesdlg.ui
FORMS += Dialogs/findduplicatesdlg.ui
FORMS += Dialogs/imagedlg.ui
FORMS += Dialogs/ingestdlg.ui
FORMS += Dialogs/ingesterrors.ui
FORMS += Dialogs/loadusbdlg.ui
FORMS += Dialogs/managegraphicsdlg.ui
FORMS += Dialogs/manageimagesdlg.ui
FORMS += Dialogs/managetilesdlg.ui
FORMS += Dialogs/patterndlg.ui
FORMS += Dialogs/renamedlg.ui
FORMS += Dialogs/renamefiledlg.ui
FORMS += Dialogs/saveasdlg.ui
FORMS += Dialogs/selectionorpicksdlg.ui
FORMS += Dialogs/testaligndlg.ui
FORMS += Dialogs/testdlg.ui
FORMS += Dialogs/test1.ui
FORMS += Dialogs/test12.ui
FORMS += Dialogs/tokendlg.ui
FORMS += Dialogs/updateapp.ui
FORMS += Dialogs/workspacedlg.ui
FORMS += Dialogs/zoomdlg.ui
FORMS += Embellish/embelCoord.ui
FORMS += Help/filtershelp.ui
FORMS += Help/foldershelp.ui
FORMS += Help/helpfindduplicates.ui
FORMS += Help/helpform.ui
FORMS += Help/helpingest.ui
FORMS += Help/helppixeldelta.ui
FORMS += Help/ingestautopath.ui
FORMS += Help/introduction.ui
FORMS += Help/message.ui
FORMS += Help/shortcutsform.ui
FORMS += Help/welcome.ui
FORMS += Metadata/metadatareport.ui
FORMS += Utilities/inputdlg.ui

RESOURCES += winnow.qrc
ICON = images/winnow.icns
RC_ICONS = images/winnow.ico

DISTFILES += $$files(Lib/*, true)
DISTFILES += Docs/ingestautopath
DISTFILES += notes/InstallMediaPipe.txt
DISTFILES += CMakeLists.txt
DISTFILES += _CMakeLists.txt
DISTFILES += notes/_InstallLibrary.txt
DISTFILES += notes/_install library.txt
DISTFILES += notes/_install library.txt
DISTFILES += notes/_install library.txt
DISTFILES += notes/_install library.txt
DISTFILES += notes/Create help dialog.rtf
DISTFILES += notes/Create help dialog.txt
DISTFILES += Docs/ingestautopath.html
DISTFILES += Docs/versions
DISTFILES += Docs/test.html
DISTFILES += notes/_Symbols
DISTFILES += notes/_Notes
DISTFILES += notes/_ToDo.txt
DISTFILES += notes/DeployInstall.txt
DISTFILES += notes/ExiftoolCommands.txt
DISTFILES += notes/git.txt
DISTFILES += notes/help_videos_on_mac.txt
DISTFILES += notes/HelpDocCreation.txt
DISTFILES += notes/HelpVideoNotes.txt
DISTFILES += notes/hevcinfo
DISTFILES += notes/ImageMagick
DISTFILES += notes/Menu.txt
DISTFILES += notes/Object_detection
DISTFILES += notes/scratch.css
DISTFILES += notes/scratch.html
DISTFILES += notes/Scratch.txt
DISTFILES += notes/Scratch2.txt
DISTFILES += notes/Shortcuts.txt
DISTFILES += notes/snippets.txt
DISTFILES += notes/users.txt
DISTFILES += notes/VideoScripts
DISTFILES += notes/xmp.txt

macx:LIBS += -framework ApplicationServices
macx:LIBS += -framework AppKit
macx:LIBS += -framework CoreFoundation
macx:LIBS += -framework Foundation

# onnxruntime
# macx:INCLUDEPATH += /Users/roryhill/onnxruntime-osx-arm64-1.17.0/include
# LIBS += -L/Users/roryhill/onnxruntime-osx-arm64-1.17.0/lib -lonnxruntime

# opencv
macx:OPENCV_PREFIX = /opt/homebrew/opt/opencv
macx:INCLUDEPATH += $$OPENCV_PREFIX/include/opencv4
macx:LIBS += -L$$OPENCV_PREFIX/lib -lopencv_core -lopencv_imgproc -lopencv_dnn
win32:INCLUDEPATH += $$PWD/Lib/opencv/windows/build/include
win32:LIBS += -L$$PWD/Lib/opencv/windows/build/x64/vc16/lib -lopencv_world4110

# zLib
macx:INCLUDEPATH += /usr/include
macx:LIBS += -lz
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/zlib/build/Release/ -lzlib
win32:INCLUDEPATH += $$PWD/Lib/zlib
win32:INCLUDEPATH += $$PWD/Lib/zlib/build
win32:DEPENDPATH += $$PWD/Lib/zlib/build/Release

# libde265 (frame parallel)
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/libde265/release/ -llibde265
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/libde265/debug/ -llibde265
win32:INCLUDEPATH += $$PWD/Lib/libde265/include
win32:DEPENDPATH  += $$PWD/Lib/libde265/release

# libheif
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/libheif/release/ -llibheif
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/libheif/debug/ -llibheif
win32:INCLUDEPATH += $$PWD/Lib/libheif/include
win32:DEPENDPATH +=  $$PWD/Lib/libheif/release

# libtiff
win32:LIBS += -L$$PWD/Lib/libtiff/build/libtiff/Release -ltiff
#win32: CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/libtiff/build/libtiff/Release -ltiff
win32:INCLUDEPATH += $$PWD/Lib/libtiff/libtiff
win32:INCLUDEPATH += $$PWD/Lib/libtiff/build/libtiff
win32:DEPENDPATH +=  $$PWD/Lib/libtiff/build/libtiff/Release
macx:LIBTIFF_PREFIX = /opt/homebrew/opt/libtiff
macx:INCLUDEPATH += $$LIBTIFF_PREFIX/include
# macx:LIBS += -L$$LIBTIFF_PREFIX/lib -llibtiff  # not working, use absolute ref
macx::LIBS += /opt/homebrew/opt/libtiff/lib/libtiff.dylib

# libjpeg-turbo
macx:JPEGTURBO_PREFIX = /opt/homebrew/opt/jpeg-turbo
macx:INCLUDEPATH += $$JPEGTURBO_PREFIX/include
macx:LIBS += -L$$JPEGTURBO_PREFIX/lib -lturbojpeg
