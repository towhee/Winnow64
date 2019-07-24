TEMPLATE = app
TARGET = Winnow
INCLUDEPATH += .
INCLUDEPATH += Dialogs
INCLUDEPATH += Utilities
INCLUDEPATH += MacOS

QT += widgets
QT += concurrent
QT += xmlpatterns

HEADERS += Cache/imagecache.h \
    PropertyEditor/preferences.h
HEADERS += Cache/mdcache.h
HEADERS += Cache/tshash.h
HEADERS += Datamodel/datamodel.h
HEADERS += Datamodel/filters.h
HEADERS += Dialogs/aboutdlg.h
HEADERS += Dialogs/appdlg.h
HEADERS += Dialogs/ingestdlg.h
HEADERS += Dialogs/loadusbdlg.h
HEADERS += Dialogs/prefdlg.h
HEADERS += Dialogs/renamedlg.h
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
HEADERS += Main/dockwidget.h
HEADERS += Main/global.h
HEADERS += Main/mainwindow.h
HEADERS += Metadata/imagemetadata.h
HEADERS += Metadata/metadata.h
HEADERS += Metadata/xmp.h
HEADERS += PropertyEditor/propertyeditor.h
HEADERS += PropertyEditor/propertydelegate.h
HEADERS += PropertyEditor/propertywidgets.h
HEADERS += Views/compareImages.h
HEADERS += Views/compareview.h
HEADERS += Views/iconview.h
HEADERS += Views/iconviewdelegate.h
HEADERS += Views/imageview.h
HEADERS += Views/infostring.h
HEADERS += Views/infoview.h
HEADERS += Views/tableview.h
HEADERS += Utilities/classificationlabel.h
HEADERS += Utilities/dropshadowlabel.h
HEADERS += Utilities/popup.h
HEADERS += Utilities/progressbar.h
HEADERS += Utilities/usb.h
HEADERS += Utilities/utilities.h

SOURCES += Cache/imagecache.cpp \
    PropertyEditor/preferences.cpp
SOURCES += Cache/mdcache.cpp
SOURCES += Datamodel/datamodel.cpp
SOURCES += Datamodel/filters.cpp
SOURCES += Dialogs/aboutdlg.cpp
SOURCES += Dialogs/appdlg.cpp
SOURCES += Dialogs/ingestdlg.cpp
SOURCES += Dialogs/loadusbdlg.cpp
SOURCES += Dialogs/prefdlg.cpp
SOURCES += Dialogs/renamedlg.cpp
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
SOURCES += Main/dockwidget.cpp
SOURCES += Main/global.cpp
SOURCES += Main/main.cpp
SOURCES += Main/mainwindow.cpp
SOURCES += Metadata/metadata.cpp
SOURCES += Metadata/xmp.cpp
SOURCES += PropertyEditor/propertyeditor.cpp
SOURCES += PropertyEditor/propertydelegate.cpp
SOURCES += PropertyEditor/propertywidgets.cpp
SOURCES += Views/compareImages.cpp
SOURCES += Views/compareview.cpp
SOURCES += Views/iconview.cpp
SOURCES += Views/iconviewdelegate.cpp
SOURCES += Views/imageview.cpp
SOURCES += Views/infoview.cpp
SOURCES += Views/tableview.cpp
SOURCES += Views/infostring.cpp
SOURCES += Utilities/classificationlabel.cpp
SOURCES += Utilities/dropshadowlabel.cpp
SOURCES += Utilities/popup.cpp
SOURCES += Utilities/progressbar.cpp
SOURCES += Utilities/usb.cpp
SOURCES += Utilities/utilities.cpp

FORMS += Dialogs/aboutdlg.ui
FORMS += Dialogs/aligndlg.ui
FORMS += Dialogs/appdlg.ui
FORMS += Dialogs/ingestdlg.ui
FORMS += Dialogs/loadusbdlg.ui
FORMS += Dialogs/prefdlg.ui
FORMS += Dialogs/renamedlg.ui
FORMS += Dialogs/testaligndlg.ui
FORMS += Dialogs/tokendlg.ui
FORMS += Dialogs/updateapp.ui
FORMS += Dialogs/workspacedlg.ui
FORMS += Dialogs/zoomdlg.ui
FORMS += Help/helpform.ui
FORMS += Help/helpingest.ui
FORMS += Help/introduction.ui
FORMS += Help/message.ui
FORMS += Help/shortcutsform.ui
FORMS += Help/welcome.ui
FORMS += Metadata/metadatareport.ui
FORMS += Dialogs/testdlg.ui
FORMS += Dialogs/test1.ui

RESOURCES += winnow.qrc
ICON = images/winnow.icns
RC_ICONS = images/winnow.ico

DISTFILES += Docs/versions \
    ../../kproperty-master/Notes
DISTFILES += Docs/test.html
DISTFILES += notes/scratch.html
DISTFILES += notes/_Notes
DISTFILES += notes/_ToDo.txt
DISTFILES += notes/DeployInstall.txt
DISTFILES += notes/ExiftoolCommands.txt
DISTFILES += notes/git.txt
DISTFILES += notes/HelpDocCreation.txt
DISTFILES += notes/Menu.txt
DISTFILES += notes/Scratch.txt
DISTFILES += notes/Shortcuts.txt
DISTFILES += notes/snippets.txt
DISTFILES += notes/xmp.txt

#macx {
#    QMAKE_MAC_SDK = macosx10.12
#}

# OBJECTIVE_SOURCES += MacOS/macscale.mm

#HEADERS += MacOS/macscale.h
#HEADERS += MacOS/macScale_p.h

#QT += macextras
mac:LIBS += -framework ApplicationServices
mac:LIBS += -framework AppKit
#mac:LIBS += -v
