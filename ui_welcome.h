/********************************************************************************
** Form generated from reading UI file 'welcome.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WELCOME_H
#define UI_WELCOME_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_welcomeScrollArea
{
public:
    QWidget *scrollAreaWidgetContents;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QVBoxLayout *verticalLayout;
    QSpacerItem *verticalSpacer_2;
    QFrame *frame_2;
    QVBoxLayout *verticalLayout_2;
    QLabel *welcomeLabel;
    QLabel *getStartedLabel;
    QFrame *frame;
    QHBoxLayout *horizontalLayout_2;
    QLabel *tipsLabel;
    QSpacerItem *verticalSpacer;
    QSpacerItem *horizontalSpacer_2;

    void setupUi(QScrollArea *welcomeScrollArea)
    {
        if (welcomeScrollArea->objectName().isEmpty())
            welcomeScrollArea->setObjectName(QString::fromUtf8("welcomeScrollArea"));
        welcomeScrollArea->resize(1340, 1044);
        welcomeScrollArea->setFrameShape(QFrame::NoFrame);
        welcomeScrollArea->setFrameShadow(QFrame::Plain);
        welcomeScrollArea->setLineWidth(5);
        welcomeScrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 1326, 1781));
        horizontalLayout = new QHBoxLayout(scrollAreaWidgetContents);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(-1, -1, -1, 1100);
        horizontalSpacer = new QSpacerItem(30, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);

        frame_2 = new QFrame(scrollAreaWidgetContents);
        frame_2->setObjectName(QString::fromUtf8("frame_2"));
        frame_2->setMaximumSize(QSize(16777215, 16777215));
        frame_2->setFrameShape(QFrame::NoFrame);
        frame_2->setFrameShadow(QFrame::Raised);
        verticalLayout_2 = new QVBoxLayout(frame_2);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        welcomeLabel = new QLabel(frame_2);
        welcomeLabel->setObjectName(QString::fromUtf8("welcomeLabel"));
        welcomeLabel->setMinimumSize(QSize(500, 0));
        welcomeLabel->setMaximumSize(QSize(16777215, 16777215));
        welcomeLabel->setFrameShape(QFrame::Box);
        welcomeLabel->setAlignment(Qt::AlignCenter);
        welcomeLabel->setWordWrap(true);

        verticalLayout_2->addWidget(welcomeLabel);

        getStartedLabel = new QLabel(frame_2);
        getStartedLabel->setObjectName(QString::fromUtf8("getStartedLabel"));

        verticalLayout_2->addWidget(getStartedLabel);


        verticalLayout->addWidget(frame_2);

        frame = new QFrame(scrollAreaWidgetContents);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setFrameShape(QFrame::NoFrame);
        frame->setFrameShadow(QFrame::Raised);
        horizontalLayout_2 = new QHBoxLayout(frame);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        tipsLabel = new QLabel(frame);
        tipsLabel->setObjectName(QString::fromUtf8("tipsLabel"));
        tipsLabel->setMinimumSize(QSize(500, 0));
        tipsLabel->setMaximumSize(QSize(1000, 16777215));
        tipsLabel->setFrameShape(QFrame::Box);
        tipsLabel->setLineWidth(1);
        tipsLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        tipsLabel->setWordWrap(true);

        horizontalLayout_2->addWidget(tipsLabel);


        verticalLayout->addWidget(frame, 0, Qt::AlignTop);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        horizontalLayout->addLayout(verticalLayout);

        horizontalSpacer_2 = new QSpacerItem(30, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        welcomeScrollArea->setWidget(scrollAreaWidgetContents);

        retranslateUi(welcomeScrollArea);

        QMetaObject::connectSlotsByName(welcomeScrollArea);
    } // setupUi

    void retranslateUi(QScrollArea *welcomeScrollArea)
    {
        welcomeScrollArea->setWindowTitle(QCoreApplication::translate("welcomeScrollArea", "ScrollArea", nullptr));
        welcomeLabel->setText(QCoreApplication::translate("welcomeScrollArea", "<html><head/><body><p align=\"center\"><span style=\" color:#cccccc;\"><br/></span><span style=\" font-size:24pt; color:#ffffff;\">Welcome to Winnow</span><span style=\" color:#cccccc;\"><br/></span></p></body></html>", nullptr));
        getStartedLabel->setText(QCoreApplication::translate("welcomeScrollArea", "<html><head/><body><p align=\"center\"><span style=\" font-size:13pt; color:#cccccc;\">To get started select a folder with images from the Folders panel <br/>or from the menu: </span><span style=\" font-size:10pt; font-style:italic; color:#cccccc;\">File &gt; Open Folder</span><span style=\" font-size:13pt; color:#cccccc;\"> (Shortcut &quot;O&quot;)</span></p></body></html>", nullptr));
        tipsLabel->setText(QCoreApplication::translate("welcomeScrollArea", "<html><head/><body><p align=\"center\"><br/></p><p><span style=\" font-size:13pt; font-weight:600; color:#ffffff;\">Help</span><span style=\" font-size:13pt; color:#cccccc;\">: <br/>There is a help document you can access via the menu: </span><span style=\" font-size:13pt; font-style:italic; color:#cccccc;\">Help &gt; Winnow Help</span><span style=\" font-size:13pt; color:#cccccc;\"> or you can press &quot;?&quot;.</span></p><p><span style=\" font-size:13pt; font-weight:600; color:#ffffff;\">Shortcuts</span><span style=\" font-size:13pt; color:#cccccc;\">: <br/>Most of the shortcuts are shown in the menus. You can see a complete list via the menu: </span><span style=\" font-size:13pt; font-style:italic; color:#cccccc;\">Help &gt; Shortcuts</span><span style=\" font-size:13pt; color:#cccccc;\">.</span></p><p><span style=\" font-size:13pt; font-weight:600; color:#ffffff;\">Workspaces</span><span style=\" font-size:13pt; color:#cccccc;\">: <br/>Check out workspaces to change to a different layout instantly. You c"
                        "an create workspaces and access them via Ctrl/Cmd 0-9.</span></p><p><span style=\" font-size:13pt; font-weight:600; color:#ffffff;\">Panels</span><span style=\" font-size:13pt; color:#cccccc;\">: <br/>The panels will float or dock to the top, bottom, right or left sides of the main window. The panel titlebar must be visible to move panels.</span></p><p><span style=\" font-size:13pt; font-weight:600; color:#ffffff;\">Context Menus</span><span style=\" font-size:13pt; color:#cccccc;\">: <br/>Right mouse click to see context menus. This is handy when the main menu is hidden in full screen mode.</span></p><p><span style=\" font-size:13pt; font-weight:600; color:#ffffff;\">Tool tips</span><span style=\" font-size:13pt; color:#cccccc;\">: <br/>Hover over an item with the mouse to see a tool tip. Try this in the preferences dialog to see explanations for different settings.<", nullptr));
    } // retranslateUi

};

namespace Ui {
    class welcomeScrollArea: public Ui_welcomeScrollArea {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WELCOME_H
