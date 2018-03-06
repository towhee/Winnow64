TEMPLATE = app
TARGET = Winnow
INCLUDEPATH += .
INCLUDEPATH += Dialogs
INCLUDEPATH += MacOS

#QT += gui-private
QT += widgets
QT += xmlpatterns

HEADERS += \
    Dialogs/prefdlg.h \
    Dialogs/workspacedlg.h \
    Utilities/popup.h \
    Dialogs/tokendlg.h \
    Dialogs/zoomdlg.h \
    MacOS/macscale.h \
    MacOS/macScale_p.h \
    Dialogs/appdlg.h \
    Dialogs/renamedlg.h \
    Metadata/metadata.h \
    Metadata/xmp.h \
    Utilities/circlelabel.h \
    Utilities/dropshadowlabel.h \
    Dialogs/testaligndlg.h \
    Utilities/progressbar.h \
    Views/compareImages.h \
    Views/compareview.h \
    Views/imageview.h \
    Views/infoview.h \
    Views/tableview.h \
    Views/thumbview.h \
    Views/thumbviewdelegate.h \
    Image/pixmap.h \
    Image/thumb.h \
    Image/imagealign.h \
    Datamodel/datamodel.h \
    Datamodel/filters.h \
    File/bookmarks.h \
    File/fstree.h \
    Cache/imagecache.h \
    Cache/mdcache.h \
    Views/infostring.h \
    Main/global.h \
    Main/mainwindow.h \
    Dialogs/ingestdlg.h

SOURCES += \
    Utilities/popup.cpp \
    Dialogs/prefdlg.cpp \
    Dialogs/tokendlg.cpp \
    Dialogs/workspacedlg.cpp \
    Dialogs/zoomdlg.cpp \
    Dialogs/appdlg.cpp \
    Dialogs/renamedlg.cpp \
    Metadata/metadata.cpp \
    Metadata/xmp.cpp \
    Utilities/dropshadowlabel.cpp \
    Utilities/circlelabel.cpp \
    Dialogs/testaligndlg.cpp \
    Utilities/progressbar.cpp \
    Views/compareImages.cpp \
    Views/compareview.cpp \
    Views/imageview.cpp \
    Views/infoview.cpp \
    Views/tableview.cpp \
    Views/thumbview.cpp \
    Views/thumbviewdelegate.cpp \
    Image/pixmap.cpp \
    Image/thumb.cpp \
    Image/imagealign.cpp \
    Datamodel/datamodel.cpp \
    Datamodel/filters.cpp \
    File/bookmarks.cpp \
    File/fstree.cpp \
    Cache/imagecache.cpp \
    Cache/mdcache.cpp \
    Views/infostring.cpp \
    Main/main.cpp \
    Main/global.cpp \
    Main/mainwindow.cpp \
    Dialogs/ingestdlg.cpp

OBJECTIVE_SOURCES += MacOS/macscale.mm

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

