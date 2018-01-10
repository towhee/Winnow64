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
    tableview.h \
    thumbview.h \
    thumbviewdelegate.h \
    testaligndlg.h \
    Dialogs/prefdlg.h \
    Dialogs/workspacedlg.h \
    Dialogs/copypickdlg.h \
    Dialogs/popup.h \
    Dialogs/zoomdlg.h \
    MacOS/macscale.h \
    MacOS/macScale_p.h \
    thumb.h

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
    tableview.cpp \
    thumbview.cpp \
    thumbviewdelegate.cpp \
    testaligndlg.cpp \
    Dialogs/prefdlg.cpp \
    Dialogs/workspacedlg.cpp \
    Dialogs/copypickdlg.cpp \
    Dialogs/popup.cpp \
    Dialogs/zoomdlg.cpp \
    thumb.cpp

OBJECTIVE_SOURCES += MacOS/macscale.mm

FORMS += \
    copypickdlg.ui \
    prefdlg.ui \
    workspacedlg.ui \
    helpform.ui \
    aligndlg.ui \
    testaligndlg.ui \
    introduction.ui \
    welcome.ui \
    shortcutsform.ui \
    zoomdlg.ui \
    message.ui \
    metadatareport.ui

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
    metadata.txt

#macx {
#    QMAKE_MAC_SDK = macosx10.12
#}


#QT += macextras
mac:LIBS += -framework ApplicationServices
mac:LIBS += -framework AppKit
#mac:LIBS += -v

