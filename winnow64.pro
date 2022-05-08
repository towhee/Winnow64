#CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT
CONFIG += sdk_no_version_check

#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000
greaterThan(QT_MAJOR_VERSION, 5) {
    DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x051500
#    QT += core5compat
}

#QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.13

# MacOS universal binaries
#QMAKE_APPLE_DEVICE_ARCHS = arm64
#QMAKE_APPLE_DEVICE_ARCHS = x86_64 arm64

CONFIG += c++1z
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
#DEPENDPATH += $$PWD/libxml

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
QT += concurrent
QT += network

HEADERS += Cache/mdcache.h \
    Cache/metaread.h \
    Cache/metareader.h
HEADERS += Cache/cachedata.h
HEADERS += Cache/tshash.h
HEADERS += Cache/imagecache.h
HEADERS += Cache/imagedecoder.h
HEADERS += Datamodel/buildfilters.h
HEADERS += Datamodel/datamodel.h
HEADERS += Datamodel/filters.h
HEADERS += Datamodel/HashMap.h
HEADERS += Datamodel/HashNode.h
HEADERS += Dialogs/aboutdlg.h
HEADERS += Dialogs/appdlg.h
HEADERS += Dialogs/copystyledlg.h
HEADERS += Dialogs/editlistdlg.h
HEADERS += Dialogs/ingestdlg.h
HEADERS += Dialogs/ingesterrors.h
HEADERS += Dialogs/loadusbdlg.h
HEADERS += Dialogs/managegraphicsdlg.h
HEADERS += Dialogs/manageimagesdlg.h
HEADERS += Dialogs/managetilesdlg.h
HEADERS += Dialogs/patterndlg.h
HEADERS += Dialogs/preferencesdlg.h
HEADERS += Dialogs/renamedlg.h
HEADERS += Dialogs/saveasdlg.h
HEADERS += Dialogs/selectionorpicksdlg.h
HEADERS += Dialogs/testaligndlg.h
HEADERS += Dialogs/workspacedlg.h
HEADERS += Dialogs/tokendlg.h
HEADERS += Dialogs/updateapp.h
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
HEADERS += File/ingest.h
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
win32:HEADERS += ImageFormats/Heic/heic.h
win32:HEADERS += ImageFormats/Heic/heif.h
win32:HEADERS += ImageFormats/Heic/de265.h
HEADERS += ImageFormats/Jpeg/jpeg.h
HEADERS += ImageFormats/Nikon/nikon.h
HEADERS += ImageFormats/Olympus/olympus.h
HEADERS += ImageFormats/Panasonic/panasonic.h
HEADERS += ImageFormats/Sony/sony.h
HEADERS += ImageFormats/Tiff/tiff.h
HEADERS += Lcms2/lcms2.h
HEADERS += Lcms2/lcms2_plugin.h
HEADERS += Main/dockwidget.h
HEADERS += Main/global.h
HEADERS += Main/mainwindow.h
HEADERS += Main/qtlocalpeer.h
HEADERS += Main/qtlockedfile.h
HEADERS += Main/qtsingleapplication.h
HEADERS += Main/widgetcss.h
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
HEADERS += Utilities/dropshadowlabel.h
HEADERS += Utilities/foldercompressor.h
HEADERS += Utilities/htmlwindow.h
HEADERS += Utilities/icc.h
mac:HEADERS += Utilities/mac.h
HEADERS += Utilities/inputdlg.h
HEADERS += Utilities/performance.h
HEADERS += Utilities/popup.h
HEADERS += Utilities/progressbar.h
HEADERS += Utilities/usb.h
HEADERS += Utilities/utilities.h
win32:HEADERS += Utilities/win.h
HEADERS += Views/compareImages.h
HEADERS += Views/compareview.h
HEADERS += Views/iconview.h
HEADERS += Views/iconviewdelegate.h
HEADERS += Views/imageview.h
HEADERS += Views/infostring.h
HEADERS += Views/infoview.h
HEADERS += Views/tableview.h

SOURCES += Cache/cachedata.cpp \
    Cache/metaread.cpp \
    Cache/metareader.cpp \
    Main/diagnostics.cpp \
    Main/initialize.cpp \
    Main/menusandactions.cpp \
    Main/slideshow.cpp \
    Main/sortandfilter.cpp \
    Main/viewmodes.cpp \
    Main/workspaces.cpp
SOURCES += Cache/imagecache.cpp
SOURCES += Cache/imagedecoder.cpp
SOURCES += Cache/mdcache.cpp
SOURCES += Datamodel/buildfilters.cpp
SOURCES += Datamodel/datamodel.cpp
SOURCES += Datamodel/filters.cpp
SOURCES += Dialogs/aboutdlg.cpp
SOURCES += Dialogs/appdlg.cpp
SOURCES += Dialogs/copystyledlg.cpp
SOURCES += Dialogs/editlistdlg.cpp
SOURCES += Dialogs/ingestdlg.cpp
SOURCES += Dialogs/ingesterrors.cpp
SOURCES += Dialogs/loadusbdlg.cpp
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
SOURCES += File/ingest.cpp
SOURCES += Image/imagealign.cpp
SOURCES += Image/pixmap.cpp
SOURCES += Image/stack.cpp
SOURCES += Image/thumb.cpp
#SOURCES += Image/tiffhandler.cpp
SOURCES += ImageFormats/Canon/canon.cpp
SOURCES += ImageFormats/Canon/canoncr3.cpp
SOURCES += ImageFormats/Dng/dng.cpp
SOURCES += ImageFormats/Fuji/fuji.cpp
win32:SOURCES += ImageFormats/Heic/Heic.cpp
SOURCES += ImageFormats/jpeg/jpeg.cpp
SOURCES += ImageFormats/Nikon/nikon.cpp
SOURCES += ImageFormats/Olympus/olympus.cpp
SOURCES += ImageFormats/Panasonic/panasonic.cpp
SOURCES += ImageFormats/Sony/sony.cpp
SOURCES += ImageFormats/Tiff/tiff.cpp

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

SOURCES += Main/dockwidget.cpp
SOURCES += Main/global.cpp
SOURCES += Main/main.cpp
SOURCES += Main/mainwindow.cpp
SOURCES += Main/qtlocalpeer.cpp
SOURCES += Main/qtlockedfile.cpp
mac:SOURCES += Main/qtlockedfile_unix.cpp
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
SOURCES += Utilities/dropshadowlabel.cpp
SOURCES += Utilities/foldercompressor.cpp
SOURCES += Utilities/htmlwindow.cpp
SOURCES += Utilities/icc.cpp
mac:SOURCES += Utilities/mac.cpp
SOURCES += Utilities/inputdlg.cpp
SOURCES += Utilities/performance.cpp
SOURCES += Utilities/popup.cpp
SOURCES += Utilities/progressbar.cpp
SOURCES += Utilities/usb.cpp
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

FORMS += Dialogs/aboutdlg.ui
FORMS += Dialogs/aligndlg.ui
FORMS += Dialogs/appdlg.ui
FORMS += Dialogs/copystyledlg.ui
FORMS += Dialogs/editlistdlg.ui
FORMS += Dialogs/ingestdlg.ui
FORMS += Dialogs/ingesterrors.ui
#FORMS += Dialogs/ingestdlg_copy.ui
FORMS += Dialogs/loadusbdlg.ui
FORMS += Dialogs/managegraphicsdlg.ui
FORMS += Dialogs/manageimagesdlg.ui
FORMS += Dialogs/managetilesdlg.ui
FORMS += Dialogs/patterndlg.ui
FORMS += Dialogs/renamedlg.ui
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
FORMS += Help/helpform.ui
FORMS += Help/helpingest.ui
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

DISTFILES += Docs/ingestautopath
DISTFILES += Docs/ingestautopath.html
DISTFILES += Docs/versions
DISTFILES += Docs/test.html
DISTFILES += notes/_Notes
DISTFILES += notes/_ToDo.txt
DISTFILES += notes/DeployInstall.txt
DISTFILES += notes/ExiftoolCommands.txt
DISTFILES += notes/git.txt
DISTFILES += notes/help_videos_on_mac.txt
DISTFILES += notes/HelpDocCreation.txt
DISTFILES += notes/HelpVideoNotes.txt
DISTFILES += notes/ImageMagick
DISTFILES += notes/hevcinfo
DISTFILES += notes/Menu.txt
DISTFILES += notes/scratch.css
DISTFILES += notes/scratch.html
DISTFILES += notes/Scratch.txt
DISTFILES += notes/Shortcuts.txt
DISTFILES += notes/snippets.txt
DISTFILES += notes/users.txt
DISTFILES += notes/xmp.txt

#include(Lib/zlib/zlib.pri)
#include(Lib/libtiff/libtiff.pri)

#macx:QMAKE_MAC_SDK = macosx10.14
#macx:QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.14
macx:LIBS += -framework ApplicationServices
macx:LIBS += -framework AppKit
#macx:LIBS += -framework ColorSync
#macx:LIBS += -framework Foundation

# lcms
#macx: LIBS += -L$$PWD/Lib/lcms2/release/ -llcms2
#macx: INCLUDEPATH += $$PWD/Lib/lcms2/include
#macx: DEPENDPATH += $$PWD/Lib/lcms2/include
#macx: PRE_TARGETDEPS += $$PWD/Lib/lcms2/release/liblcms2.a


#win32:QMAKE_CXXFLAGS += /MD

# zLib
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/zlib/x64-Release/ -lzlib
win32:INCLUDEPATH += $$PWD/Lib/zlib
win32:DEPENDPATH += $$PWD/Lib/zlib

# lcms not using compiled lib: compiling source in project using D:\My Projects\Winnow Project\Winnow64\Lcms2
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/Lib/MS/ -lCORE_RL_lcms_
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/Lib/MS/ -llcms2_staticd
#win32:INCLUDEPATH += $$PWD/Lib/lcms2-2.9/include
#win32:DEPENDPATH += $$PWD/Lib/lcms2-2.9/Lib/MS/

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

