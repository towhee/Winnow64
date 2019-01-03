/********************************************************************************
** Form generated from reading UI file 'welcome.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WELCOME_H
#define UI_WELCOME_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
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
    QHBoxLayout *horizontalLayout_3;
    QLabel *welcomeLabel;
    QFrame *frame;
    QHBoxLayout *horizontalLayout_2;
    QLabel *tipsLabel;
    QSpacerItem *verticalSpacer;
    QSpacerItem *horizontalSpacer_2;

    void setupUi(QScrollArea *welcomeScrollArea)
    {
        if (welcomeScrollArea->objectName().isEmpty())
            welcomeScrollArea->setObjectName(QStringLiteral("welcomeScrollArea"));
        welcomeScrollArea->resize(814, 508);
        welcomeScrollArea->setFrameShape(QFrame::NoFrame);
        welcomeScrollArea->setFrameShadow(QFrame::Plain);
        welcomeScrollArea->setLineWidth(5);
        welcomeScrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QStringLiteral("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 793, 832));
        horizontalLayout = new QHBoxLayout(scrollAreaWidgetContents);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalSpacer = new QSpacerItem(56, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);

        frame_2 = new QFrame(scrollAreaWidgetContents);
        frame_2->setObjectName(QStringLiteral("frame_2"));
        frame_2->setMaximumSize(QSize(500, 16777215));
        frame_2->setFrameShape(QFrame::NoFrame);
        frame_2->setFrameShadow(QFrame::Raised);
        horizontalLayout_3 = new QHBoxLayout(frame_2);
        horizontalLayout_3->setObjectName(QStringLiteral("horizontalLayout_3"));
        welcomeLabel = new QLabel(frame_2);
        welcomeLabel->setObjectName(QStringLiteral("welcomeLabel"));
        welcomeLabel->setMinimumSize(QSize(500, 0));
        welcomeLabel->setMaximumSize(QSize(600, 16777215));
        welcomeLabel->setFrameShape(QFrame::Box);
        welcomeLabel->setAlignment(Qt::AlignCenter);
        welcomeLabel->setWordWrap(true);

        horizontalLayout_3->addWidget(welcomeLabel);


        verticalLayout->addWidget(frame_2);

        frame = new QFrame(scrollAreaWidgetContents);
        frame->setObjectName(QStringLiteral("frame"));
        frame->setFrameShape(QFrame::NoFrame);
        frame->setFrameShadow(QFrame::Raised);
        horizontalLayout_2 = new QHBoxLayout(frame);
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        tipsLabel = new QLabel(frame);
        tipsLabel->setObjectName(QStringLiteral("tipsLabel"));
        tipsLabel->setMaximumSize(QSize(500, 16777215));
        tipsLabel->setFrameShape(QFrame::Box);
        tipsLabel->setLineWidth(1);
        tipsLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        tipsLabel->setWordWrap(true);

        horizontalLayout_2->addWidget(tipsLabel);


        verticalLayout->addWidget(frame, 0, Qt::AlignTop);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        horizontalLayout->addLayout(verticalLayout);

        horizontalSpacer_2 = new QSpacerItem(67, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        welcomeScrollArea->setWidget(scrollAreaWidgetContents);

        retranslateUi(welcomeScrollArea);

        QMetaObject::connectSlotsByName(welcomeScrollArea);
    } // setupUi

    void retranslateUi(QScrollArea *welcomeScrollArea)
    {
        welcomeScrollArea->setWindowTitle(QApplication::translate("welcomeScrollArea", "ScrollArea", nullptr));
        welcomeLabel->setText(QApplication::translate("welcomeScrollArea", "<html><head/><body><p align=\"center\"><span style=\" color:#cccccc;\"><br/></span><span style=\" font-size:24pt; color:#ffffff;\">Welcome to Winnow</span></p><p align=\"center\"><span style=\" color:#cccccc;\"><br/></span><span style=\" font-size:10pt; color:#cccccc;\">To get started select a folder with images from the Folders panel <br/>or from the menu: </span><span style=\" font-size:10pt; font-style:italic; color:#cccccc;\">File &gt; Open Folder</span><span style=\" font-size:10pt; color:#cccccc;\"> (Shortcut &quot;O&quot;)</span><span style=\" color:#cccccc;\"><br/></span></p></body></html>", nullptr));
        tipsLabel->setText(QApplication::translate("welcomeScrollArea", "<html><head/><body><p align=\"center\"><br/></p><p><span style=\" font-size:10pt; font-weight:600; color:#ffffff;\">Help</span><span style=\" font-size:10pt; color:#cccccc;\">: <br/>There is a help document you can access via the menu: </span><span style=\" font-size:10pt; font-style:italic; color:#cccccc;\">Help &gt; Winnow Help</span><span style=\" font-size:10pt; color:#cccccc;\"> or you can press &quot;?&quot;.</span></p><p><span style=\" font-size:10pt; font-weight:600; color:#ffffff;\">Shortcuts</span><span style=\" font-size:10pt; color:#cccccc;\">: <br/>Most of the shortcuts are shown in the menus. You can see a complete list via the menu: </span><span style=\" font-size:10pt; font-style:italic; color:#cccccc;\">Help &gt; Shortcuts</span><span style=\" font-size:10pt; color:#cccccc;\">.</span></p><p><span style=\" font-size:10pt; font-weight:600; color:#ffffff;\">Workspaces</span><span style=\" font-size:10pt; color:#cccccc;\">: <br/>Check out workspaces to change to a different layout instantly. You c"
                        "an create workspaces and access them via Ctrl/Cmd 0-9.</span></p><p><span style=\" font-size:10pt; font-weight:600; color:#ffffff;\">Panels</span><span style=\" font-size:10pt; color:#cccccc;\">: <br/>The panels will float or dock to the top, bottom, right or left sides of the main window. The panel titlebar must be visible to move panels.</span></p><p><span style=\" font-size:10pt; font-weight:600; color:#ffffff;\">Context Menus</span><span style=\" font-size:10pt; color:#cccccc;\">: <br/>Right mouse click to see context menus. This is handy when the main menu is hidden in full screen mode.</span></p><p><span style=\" font-size:10pt; font-weight:600; color:#ffffff;\">Tool tips</span><span style=\" font-size:10pt; color:#cccccc;\">: <br/>Hover over an item with the mouse to see a tool tip. Try this in the preferences dialog to see explanations for different settings.</span></p></body></html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class welcomeScrollArea: public Ui_welcomeScrollArea {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WELCOME_H
