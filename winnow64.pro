CONFIG +=sdk_no_version_check
TEMPLATE = app
TARGET = Winnow
INCLUDEPATH += .
INCLUDEPATH += Dialogs
INCLUDEPATH += Utilities
INCLUDEPATH += MacOS
#INCLUDEPATH += Lib/libtiff
#INCLUDEPATH += Lib/zlib

UI_DIR = $$PWD

QT += widgets
QT += concurrent
QT += xmlpatterns

HEADERS += Cache/imagecache.h
HEADERS += Cache/mdcache.h
HEADERS += Cache/tshash.h
HEADERS += Datamodel/datamodel.h
HEADERS += Datamodel/filters.h
HEADERS += Dialogs/aboutdlg.h
HEADERS += Dialogs/appdlg.h
HEADERS += Dialogs/ingestdlg.h
HEADERS += Dialogs/loadusbdlg.h
HEADERS += Dialogs/preferencesdlg.h
HEADERS += Dialogs/renamedlg.h
HEADERS += Dialogs/saveasdlg.h
HEADERS += Dialogs/testaligndlg.h
HEADERS += Dialogs/workspacedlg.h
HEADERS += Dialogs/tokendlg.h
HEADERS += Dialogs/updateapp.h
HEADERS += Dialogs/zoomdlg.h
HEADERS += File/bookmarks.h
HEADERS += File/fstree.h
HEADERS += Image/imagealign.h
HEADERS += Image/pixmap.h
HEADERS += Image/thumb.h
#HEADERS += Image/tiffhandler.h
HEADERS += ImageFormats/Heic.h
HEADERS += Main/dockwidget.h
HEADERS += Main/global.h
HEADERS += Main/mainwindow.h
HEADERS += Main/widgetcss.h
HEADERS += Metadata/imagemetadata.h
HEADERS += Metadata/metadata.h
HEADERS += Metadata/xmp.h
HEADERS += PropertyEditor/preferences.h
HEADERS += PropertyEditor/propertyeditor.h
HEADERS += PropertyEditor/propertydelegate.h
HEADERS += PropertyEditor/propertywidgets.h
HEADERS += Utilities/classificationlabel.h
HEADERS += Utilities/dropshadowlabel.h
win32:HEADERS += Utilities/icc.h
mac:HEADERS += Utilities/mac.h
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
SOURCES += Datamodel/datamodel.cpp
SOURCES += Datamodel/filters.cpp
SOURCES += Dialogs/aboutdlg.cpp
SOURCES += Dialogs/appdlg.cpp
SOURCES += Dialogs/ingestdlg.cpp
SOURCES += Dialogs/loadusbdlg.cpp
SOURCES += Dialogs/preferencesdlg.cpp
SOURCES += Dialogs/renamedlg.cpp
SOURCES += Dialogs/saveasdlg.cpp
SOURCES += Dialogs/testaligndlg.cpp
SOURCES += Dialogs/tokendlg.cpp
SOURCES += Dialogs/updateapp.cpp
SOURCES += Dialogs/workspacedlg.cpp
SOURCES += Dialogs/zoomdlg.cpp
SOURCES += File/bookmarks.cpp
SOURCES += File/fstree.cpp
SOURCES += Image/imagealign.cpp
SOURCES += Image/pixmap.cpp
SOURCES += Image/thumb.cpp
#SOURCES += Image/tiffhandler.cpp
SOURCES += ImageFormats/Heic.cpp
SOURCES += Main/dockwidget.cpp
SOURCES += Main/global.cpp
SOURCES += Main/main.cpp
SOURCES += Main/mainwindow.cpp
SOURCES += Main/widgetcss.cpp
SOURCES += Metadata/metadata.cpp
SOURCES += Metadata/xmp.cpp
SOURCES += PropertyEditor/preferences.cpp
SOURCES += PropertyEditor/propertyeditor.cpp
SOURCES += PropertyEditor/propertydelegate.cpp
SOURCES += PropertyEditor/propertywidgets.cpp
SOURCES += Utilities/classificationlabel.cpp
SOURCES += Utilities/dropshadowlabel.cpp
win32:SOURCES += Utilities/icc.cpp
mac:SOURCES += Utilities/mac.cpp
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

FORMS += Dialogs/aboutdlg.ui \
    Dialogs/ingestdlg_copy.ui \
    Dialogs/saveasdlg.ui \
    Dialogs/test12.ui
FORMS += Dialogs/aligndlg.ui
FORMS += Dialogs/appdlg.ui
FORMS += Dialogs/ingestdlg.ui
FORMS += Dialogs/loadusbdlg.ui
FORMS += Dialogs/renamedlg.ui
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
DISTFILES += notes/Menu.txt
DISTFILES += notes/scratch.css
DISTFILES += notes/scratch.html
DISTFILES += notes/Scratch.txt
DISTFILES += notes/Shortcuts.txt
DISTFILES += notes/snippets.txt
DISTFILES += notes/xmp.txt

#include(Lib/zlib/zlib.pri)
#include(Lib/libtiff/libtiff.pri)

mac:LIBS += -framework ApplicationServices
mac:LIBS += -framework AppKit

win32:QMAKE_CXXFLAGS += /MD
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../LIBRARYNAME/Lib/ -lLIBRARY /NODEFAULTLIB:library

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/zlib/x64-Release/ -lzlib

win32:INCLUDEPATH += $$PWD/Lib/zlib
win32:DEPENDPATH += $$PWD/Lib/zlib

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/Lib/MS/ -lCORE_RL_lcms_
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/Lib/MS/ -llcms2_staticd

# attempt to use dll
#win32:CONFIG(release, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/bin/ -llcms2
#else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/Lib/lcms2-2.9/bin/ -llcms2d

win32:INCLUDEPATH += $$PWD/Lib/lcms2-2.9/include
win32:DEPENDPATH += $$PWD/Lib/lcms2-2.9/Lib/MS/
#DEPENDPATH += $$PWD/Lib/lcms2-2.9/bin
