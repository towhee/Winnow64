TEMPLATE = app
TARGET = Winnow
INCLUDEPATH += .
INCLUDEPATH += Dialogs
INCLUDEPATH += Utilities
INCLUDEPATH += MacOS

QT += widgets
QT += xmlpatterns

HEADERS += \
    Cache/imagecache.h \
    Cache/mdcache.h \
    Dialogs/appdlg.h \
    Dialogs/ingestdlg.h \
    Dialogs/prefdlg.h \
    Dialogs/renamedlg.h \
    Dialogs/testaligndlg.h \
    Dialogs/workspacedlg.h \
    Dialogs/tokendlg.h \
    Dialogs/updateapp.h \
    Dialogs/zoomdlg.h \
    Image/imagealign.h \
    Image/pixmap.h \
    Image/thumb.h \
    Datamodel/datamodel.h \
    Datamodel/filters.h \
    File/bookmarks.h \
    File/fstree.h \
#    MacOS/macscale.h \
#    MacOS/macScale_p.h \
    Main/global.h \
    Main/mainwindow.h \
    Metadata/metadata.h \
    Metadata/xmp.h \
    Views/compareImages.h \
    Views/compareview.h \
    Views/imageview.h \
    Views/infoview.h \
    Views/tableview.h \
    Views/thumbview.h \
    Views/thumbviewdelegate.h \
    Views/infostring.h \
    Utilities/classificationlabel.h \
    Utilities/dropshadowlabel.h \
    Utilities/popup.h \
    Utilities/progressbar.h \
    Utilities/usb.h \
    Utilities/utilities.h

SOURCES += \
    Cache/imagecache.cpp \
    Cache/mdcache.cpp \
    Datamodel/datamodel.cpp \
    Datamodel/filters.cpp \
    Dialogs/appdlg.cpp \
    Dialogs/ingestdlg.cpp \
    Dialogs/prefdlg.cpp \
    Dialogs/renamedlg.cpp \
    Dialogs/testaligndlg.cpp \
    Dialogs/tokendlg.cpp \
    Dialogs/updateapp.cpp \
    Dialogs/workspacedlg.cpp \
    Dialogs/zoomdlg.cpp \
    File/bookmarks.cpp \
    File/fstree.cpp \
    Image/imagealign.cpp \
    Image/pixmap.cpp \
    Image/thumb.cpp \
    Main/global.cpp \
    Main/main.cpp \
    Main/mainwindow.cpp \
    Metadata/metadata.cpp \
    Metadata/xmp.cpp \
    Views/compareImages.cpp \
    Views/compareview.cpp \
    Views/imageview.cpp \
    Views/infoview.cpp \
    Views/tableview.cpp \
    Views/thumbview.cpp \
    Views/thumbviewdelegate.cpp \
    Views/infostring.cpp \
    Utilities/classificationlabel.cpp \
    Utilities/dropshadowlabel.cpp \
    Utilities/popup.cpp \
    Utilities/progressbar.cpp \
    Utilities/usb.cpp \
    Utilities/utilities.cpp

# OBJECTIVE_SOURCES += MacOS/macscale.mm

FORMS += \
    Dialogs/aligndlg.ui \
    Dialogs/appdlg.ui \
    Dialogs/ingestdlg.ui \
    Dialogs/prefdlg.ui \
    Dialogs/renamedlg.ui \
    Dialogs/testaligndlg.ui \
    Dialogs/tokendlg.ui \
    Dialogs/updateapp.ui \
    Dialogs/workspacedlg.ui \
    Dialogs/zoomdlg.ui \
    Help/helpform.ui \
    Help/helpingest.ui \
    Help/introduction.ui \
    Help/message.ui \
    Help/shortcutsform.ui \
    Help/welcome.ui \
    Metadata/metadatareport.ui \
    Dialogs/testdlg.ui

RESOURCES += winnow.qrc
ICON = images/winnow.icns
RC_ICONS = images/winnow.ico

DISTFILES += \
    notes/_Notes \
    notes/_ToDo.txt \
    notes/DeployInstall.txt \
    notes/ExiftoolCommands.txt \
    notes/git.txt \
    notes/HelpDocCreation.txt \
    notes/Menu.txt \
    notes/Scratch.txt \
    notes/Shortcuts.txt \
    notes/snippets.txt \
    notes/xmp.txt

#macx {
#    QMAKE_MAC_SDK = macosx10.12
#}


#QT += macextras
mac:LIBS += -framework ApplicationServices
mac:LIBS += -framework AppKit
#mac:LIBS += -v

