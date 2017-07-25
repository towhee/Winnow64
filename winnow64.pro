TEMPLATE = app
TARGET = Winnow
INCLUDEPATH += .
INCLUDEPATH += Dialogs

QT += widgets

HEADERS += \
    bookmarks.h \
    dircompleter.h \
    dropshadowlabel.h \
    fstree.h \
    global.h \
    imagecache.h \
    imageview.h \
    infoview.h \
    mainwindow.h \
    mdcache.h \
    metadata.h \
    thumbcache.h \
    thumbview.h \
    thumbviewdelegate.h \
    thumbviewfilter.h \
    Dialogs/prefdlg.h \
    Dialogs/workspacedlg.h \
    Dialogs/copypickdlg.h \
    Dialogs/popup.h \
    compareImages.h \
    compareview.h
SOURCES += \
    bookmarks.cpp \
    dircompleter.cpp \
    dropshadowlabel.cpp \
    fstree.cpp \
    global.cpp \
    imagecache.cpp \
    imageview.cpp \
    infoview.cpp \
    main.cpp \
    mainwindow.cpp \
    mdcache.cpp \
    metadata.cpp \
    thumbcache.cpp \
    thumbview.cpp \
    thumbviewdelegate.cpp \
    thumbviewfilter.cpp \
    Dialogs/prefdlg.cpp \
    Dialogs/workspacedlg.cpp \
    Dialogs/copypickdlg.cpp \
    Dialogs/popup.cpp \
    compareImages.cpp \
    compareview.cpp
FORMS += \
    copypickdlg.ui \
    prefdlg.ui \
    workspacedlg.ui \
    helpform.ui

RESOURCES += winnow.qrc
ICON = images/winnow.icns
RC_ICONS = images/winnow.ico

icon.files = images/winnow.png
icon16.files = images/icon16/winnow.png
iconPixmaps.files = images/icon16/winnow.png

INSTALLS += target icon icon16 iconPixmaps desktop

DISTFILES += Notes.txt Errors \
    Menu.txt \
    teststyle.css \
    qss/WinnowStyle.css \
    qss/WinnowStyle.css \
    Shortcuts.txt

macx {
    QMAKE_MAC_SDK = macosx10.12
}

