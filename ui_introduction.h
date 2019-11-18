/********************************************************************************
** Form generated from reading UI file 'introduction.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INTRODUCTION_H
#define UI_INTRODUCTION_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_introWidget
{
public:
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *verticalLayout_3;
    QFrame *frame_2;
    QVBoxLayout *verticalLayout;
    QLabel *welcomeLabel;
    QFrame *frame;
    QHBoxLayout *horizontalLayout_2;
    QLabel *tipslabel;

    void setupUi(QWidget *introWidget)
    {
        if (introWidget->objectName().isEmpty())
            introWidget->setObjectName(QString::fromUtf8("introWidget"));
        introWidget->resize(600, 649);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(introWidget->sizePolicy().hasHeightForWidth());
        introWidget->setSizePolicy(sizePolicy);
        introWidget->setMinimumSize(QSize(0, 0));
        introWidget->setMaximumSize(QSize(16777215, 16777215));
        scrollArea = new QScrollArea(introWidget);
        scrollArea->setObjectName(QString::fromUtf8("scrollArea"));
        scrollArea->setGeometry(QRect(12, 12, 571, 625));
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 569, 623));
        sizePolicy.setHeightForWidth(scrollAreaWidgetContents->sizePolicy().hasHeightForWidth());
        scrollAreaWidgetContents->setSizePolicy(sizePolicy);
        verticalLayout_3 = new QVBoxLayout(scrollAreaWidgetContents);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        frame_2 = new QFrame(scrollAreaWidgetContents);
        frame_2->setObjectName(QString::fromUtf8("frame_2"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(frame_2->sizePolicy().hasHeightForWidth());
        frame_2->setSizePolicy(sizePolicy1);
        frame_2->setFrameShape(QFrame::StyledPanel);
        frame_2->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(frame_2);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        welcomeLabel = new QLabel(frame_2);
        welcomeLabel->setObjectName(QString::fromUtf8("welcomeLabel"));
        welcomeLabel->setWordWrap(true);

        verticalLayout->addWidget(welcomeLabel);


        verticalLayout_3->addWidget(frame_2);

        frame = new QFrame(scrollAreaWidgetContents);
        frame->setObjectName(QString::fromUtf8("frame"));
        sizePolicy.setHeightForWidth(frame->sizePolicy().hasHeightForWidth());
        frame->setSizePolicy(sizePolicy);
        frame->setMaximumSize(QSize(16777215, 16777215));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        horizontalLayout_2 = new QHBoxLayout(frame);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        tipslabel = new QLabel(frame);
        tipslabel->setObjectName(QString::fromUtf8("tipslabel"));
        tipslabel->setAlignment(Qt::AlignHCenter|Qt::AlignTop);
        tipslabel->setWordWrap(true);
        tipslabel->setMargin(0);
        tipslabel->setIndent(50);

        horizontalLayout_2->addWidget(tipslabel);


        verticalLayout_3->addWidget(frame);

        scrollArea->setWidget(scrollAreaWidgetContents);

        retranslateUi(introWidget);

        QMetaObject::connectSlotsByName(introWidget);
    } // setupUi

    void retranslateUi(QWidget *introWidget)
    {
        introWidget->setWindowTitle(QCoreApplication::translate("introWidget", "Form", nullptr));
        welcomeLabel->setText(QCoreApplication::translate("introWidget", "<html><head/><body><p align=\"center\"><span style=\" font-size:24pt;\">Welcome to Winnow</span></p><p align=\"center\"><br/><span style=\" font-size:14pt;\">To get started select a folder with images from the Folders panel or from the menu: File &gt; Open</span></p></body></html>", nullptr));
        tipslabel->setText(QCoreApplication::translate("introWidget", "<html><head/><body><p><span style=\" font-size:18pt;\">Tips:</span></p><ul type=\"circle\" style=\"margin-top: 0px; margin-bottom: 0px; margin-left: 0px; margin-right: 0px; -qt-list-indent: 1;\"><li style=\" margin-top:12px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><span style=\" font-size:14pt;\">You can view a </span><span style=\" font-size:14pt; color:#ffff0a;\">help document</span><span style=\" font-size:14pt;\"> from the menu: Help &gt; Winnow Help or just press ?</span></li>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class introWidget: public Ui_introWidget {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INTRODUCTION_H
