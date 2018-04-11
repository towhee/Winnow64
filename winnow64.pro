TEMPLATE = app
TARGET = Winnow
INCLUDEPATH += .
INCLUDEPATH += Dialogs
INCLUDEPATH += MacOS

QT += widgets
QT += xmlpatterns

HEADERS += \
    Cache/imagecache.h \
    Cache/mdcache.h \
    Dialogs/appdlg.h \
    Dialogs/prefdlg.h \
    Dialogs/renamedlg.h \
    Dialogs/testaligndlg.h \
    Dialogs/workspacedlg.h \
    Dialogs/tokendlg.h \
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
    Dialogs/ingestdlg.h \
    Utilities/circlelabel.h \
    Utilities/dropshadowlabel.h \
    Utilities/popup.h \
    Utilities/progressbar.h \
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
    Utilities/circlelabel.cpp \
    Utilities/dropshadowlabel.cpp \
    Utilities/popup.cpp \
    Utilities/progressbar.cpp \
    Utilities/utilities.cpp

# OBJECTIVE_SOURCES += MacOS/macscale.mm

FORMS += \
    Dialogs/renamedlg.ui \
    Dialogs/aligndlg.ui \
    Dialogs/appdlg.ui \
    Dialogs/copypickdlg.ui \
    Help/helpform.ui \
    Help/helpingest.ui \
    Dialogs/prefdlg.ui \
    Help/shortcutsform.ui \
    Dialogs/testaligndlg.ui \
    Dialogs/tokendlg.ui \
    Dialogs/workspacedlg.ui \
    Dialogs/zoomdlg.ui \
    Help/introduction.ui \
    Help/welcome.ui \
    Metadata/metadatareport.ui \
    Help/message.ui

RESOURCES += winnow.qrc
ICON = images/winnow.icns
RC_ICONS = images/winnow.ico

DISTFILES += \
    notes/_Notes \
    notes/_ToDo.txt \
    notes/Deploy.txt \
    notes/git.txt \
    notes/HelpDocCreation.txt \
    notes/Menu.txt \
    notes/metadata.txt \
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

