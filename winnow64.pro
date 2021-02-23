#CONFIG(release, debug|release):DEFINES += QT_NO_DEBUG_OUTPUT
CONFIG += sdk_no_version_check
TEMPLATE = app
TARGET = Winnow
INCLUDEPATH += .
INCLUDEPATH += Dialogs
INCLUDEPATH += Utilities
INCLUDEPATH += MacOS
#INCLUDEPATH += "C:/Program Files (x86)/Visual Leak Detector/include/"
#LIBS += -L"C:/Program Files (x86)/Visual Leak Detector/lib/Win64"

UI_DIR = $$PWD/UI

QT += widgets
QT += concurrent
QT += xmlpatterns
#QT += opengl

HEADERS += Cache/imagecache.h
HEADERS += Cache/mdcache.h
HEADERS += Cache/tshash.h
HEADERS += Datamodel/buildfilters.h
HEADERS += Datamodel/datamodel.h
HEADERS += Datamodel/filters.h
HEADERS += Dialogs/aboutdlg.h
HEADERS += Dialogs/appdlg.h
HEADERS += Dialogs/copystyledlg.h
HEADERS += Dialogs/ingestdlg.h
HEADERS += Dialogs/loadusbdlg.h
HEADERS += Dialogs/managetilesdlg.h
HEADERS += Dialogs/patterndlg.h
HEADERS += Dialogs/preferencesdlg.h
HEADERS += Dialogs/renamedlg.h
HEADERS += Dialogs/saveasdlg.h
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
HEADERS += Image/imagealign.h
HEADERS += Image/pixmap.h
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
HEADERS += Utilities/htmlwindow.h
win32:HEADERS += Utilities/icc.h
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

SOURCES += Cache/imagecache.cpp
SOURCES += Cache/mdcache.cpp
SOURCES += Datamodel/buildfilters.cpp
SOURCES += Datamodel/datamodel.cpp
SOURCES += Datamodel/filters.cpp
SOURCES += Dialogs/aboutdlg.cpp
SOURCES += Dialogs/appdlg.cpp
SOURCES += Dialogs/copystyledlg.cpp
SOURCES += Dialogs/ingestdlg.cpp
SOURCES += Dialogs/loadusbdlg.cpp
SOURCES += Dialogs/managetilesdlg.cpp
SOURCES += Dialogs/patterndlg.cpp
SOURCES += Dialogs/preferencesdlg.cpp
SOURCES += Dialogs/renamedlg.cpp
SOURCES += Dialogs/saveasdlg.cpp
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
SOURCES += Image/imagealign.cpp
SOURCES += Image/pixmap.cpp
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
SOURCES += Utilities/htmlwindow.cpp
win32:SOURCES += Utilities/icc.cpp
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
FORMS += Embellish/embelCoord.ui
FORMS += Dialogs/ingestdlg.ui
FORMS += Utilities/inputdlg.ui
#FORMS += Dialogs/ingestdlg_copy.ui
FORMS += Dialogs/loadusbdlg.ui
FORMS += Dialogs/managetilesdlg.ui
FORMS += Dialogs/patterndlg.ui
FORMS += Dialogs/renamedlg.ui
FORMS += Dialogs/saveasdlg.ui
FORMS += Dialogs/testaligndlg.ui
FORMS += Dialogs/testdlg.ui
FORMS += Dialogs/test1.ui
FORMS += Dialogs/test12.ui
FORMS += Dialogs/tokendlg.ui
FORMS += Dialogs/updateapp.ui
FORMS += Dialogs/workspacedlg.ui
FORMS += Dialogs/zoomdlg.ui
FORMS += Help/helpform.ui
FORMS += Help/helpingest.ui
FORMS += Help/ingestautopath.ui
FORMS += Help/introduction.ui
FORMS += Help/message.ui
FORMS += Help/shortcutsform.ui
FORMS += Help/welcome.ui
FORMS += Metadata/metadatareport.ui

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

mac:LIBS += -framework ApplicationServices
mac:LIBS += -framework AppKit

win32:QMAKE_CXXFLAGS += /MD
#win32:QMAKE_CXXFLAGS += /MDd
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../LIBRARYNAME/Lib/ -lLIBRARY /NODEFAULTLIB:library

# zLib
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/zlib/x64-Release/ -lzlib
win32:INCLUDEPATH += $$PWD/Lib/zlib
win32:DEPENDPATH += $$PWD/Lib/zlib

# lcms
win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/Lib/MS/ -lCORE_RL_lcms_
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/Lib/MS/ -llcms2_staticd
win32:INCLUDEPATH += $$PWD/Lib/lcms2-2.9/include
win32:DEPENDPATH += $$PWD/Lib/lcms2-2.9/Lib/MS/

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

# old stuff

# use imagemagic libs
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/libde265/release/ -lCORE_RL_libde265_
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/libde265/debug/ -lCORE_DB_libde265_

##else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/libde265/debug/ libde265.lib
#win32:INCLUDEPATH += $$PWD/Lib/libde265/include
#win32:DEPENDPATH += $$PWD/Lib/libde265/debug
#win32:DEPENDPATH += $$PWD/Lib/libde265/release

# use imagemagic libs
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/libheif/release/ -lCORE_RL_libheif_
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/libheif/debug/ -lCORE_DB_libheif_

#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/libheif/debug/ libheif.lib
#win32:INCLUDEPATH += $$PWD/Lib/libheif/include
#win32:DEPENDPATH += $$PWD/Lib/libheif

# attempt to use dll
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/bin/ -llcms2
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/bin/ -llcms2d

#DEPENDPATH += $$PWD/Lib/lcms2-2.9/bin
