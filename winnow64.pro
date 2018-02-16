TEMPLATE = app
TARGET = Winnow
INCLUDEPATH += .
INCLUDEPATH += Dialogs
INCLUDEPATH += MacOS

QT += gui-private
QT += widgets

HEADERS += \
    bookmarks.h \
    circlelabel.h \
    compareImages.h \
    compareview.h \
    datamodel.h \
    dircompleter.h \
    dropshadowlabel.h \
    filters.h \
    fstree.h \
    global.h \
    imagealign.h \
    imagecache.h \
    imageview.h \
    infoview.h \
    mainwindow.h \
    mdcache.h \
    metadata.h \
    pixmap.h \
    progressbar.h \
    tableview.h \
    thumb.h \
    thumbview.h \
    thumbviewdelegate.h \
    testaligndlg.h \
    Dialogs/prefdlg.h \
    Dialogs/workspacedlg.h \
    Dialogs/copypickdlg.h \
    Dialogs/popup.h \
    Dialogs/tokendlg.h \
    Dialogs/zoomdlg.h \
    MacOS/macscale.h \
    MacOS/macScale_p.h \
    Dialogs/appdlg.h \
    Dialogs/renamedlg.h

SOURCES += \
    bookmarks.cpp \
    circlelabel.cpp \
    compareImages.cpp \
    compareview.cpp \
    datamodel.cpp \
    dircompleter.cpp \
    dropshadowlabel.cpp \
    filters.cpp \
    fstree.cpp \
    global.cpp \
    imagealign.cpp \
    imagecache.cpp \
    imageview.cpp \
    infoview.cpp \
    main.cpp \
    mainwindow.cpp \
    mdcache.cpp \
    metadata.cpp \
    pixmap.cpp \
    progressbar.cpp \
    tableview.cpp \
    thumbview.cpp \
    thumbviewdelegate.cpp \
    testaligndlg.cpp \
    thumb.cpp \
    Dialogs/copypickdlg.cpp \
    Dialogs/popup.cpp \
    Dialogs/prefdlg.cpp \
    Dialogs/tokendlg.cpp \
    Dialogs/workspacedlg.cpp \
    Dialogs/zoomdlg.cpp \
    Dialogs/appdlg.cpp \
    Dialogs/renamedlg.cpp

OBJECTIVE_SOURCES += MacOS/macscale.mm

FORMS += \
    aligndlg.ui \
    appdlg.ui \
    copypickdlg.ui \
    helpform.ui \
    introduction.ui \
    message.ui \
    metadatareport.ui \
    prefdlg.ui \
    shortcutsform.ui \
    testaligndlg.ui \
    tokendlg.ui \
    welcome.ui \
    workspacedlg.ui \
    zoomdlg.ui \
    Dialogs/renamedlg.ui \
    helpingest.ui

RESOURCES += winnow.qrc
ICON = images/winnow.icns
RC_ICONS = images/winnow.ico

DISTFILES += \
    Menu.txt \
    Shortcuts.txt \
    Scratch.txt \
    Notes \
    snippets.txt \
    Deploy.txt \
    xmp.txt \
    _ToDo.txt \
    metadata.txt \
    Tokens.txt \
    HelpDocCreation.txt

#macx {
#    QMAKE_MAC_SDK = macosx10.12
#}


#QT += macextras
mac:LIBS += -framework ApplicationServices
mac:LIBS += -framework AppKit
#mac:LIBS += -v

