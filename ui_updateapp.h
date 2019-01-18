/********************************************************************************
** Form generated from reading UI file 'updateapp.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_UPDATEAPP_H
#define UI_UPDATEAPP_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_UpdateApp
{
public:
    QDialogButtonBox *buttonBox;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QHBoxLayout *horizontalLayout;
    QTextBrowser *textBrowser;
    QLabel *label;
    QLabel *label_2;
    QLabel *label_3;

    void setupUi(QDialog *UpdateApp)
    {
        if (UpdateApp->objectName().isEmpty())
            UpdateApp->setObjectName(QStringLiteral("UpdateApp"));
        UpdateApp->resize(640, 480);
        UpdateApp->setAutoFillBackground(false);
        UpdateApp->setStyleSheet(QStringLiteral("Background-color =rgb(190, 190, 190)"));
        buttonBox = new QDialogButtonBox(UpdateApp);
        buttonBox->setObjectName(QStringLiteral("buttonBox"));
        buttonBox->setGeometry(QRect(10, 440, 621, 32));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::No|QDialogButtonBox::Yes);
        scrollArea = new QScrollArea(UpdateApp);
        scrollArea->setObjectName(QStringLiteral("scrollArea"));
        scrollArea->setGeometry(QRect(10, 110, 611, 231));
        scrollArea->setStyleSheet(QStringLiteral("Background-color =rgb(190, 190, 190)"));
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QStringLiteral("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 609, 229));
        scrollAreaWidgetContents->setStyleSheet(QStringLiteral("Background-color =rgb(190, 190, 190)"));
        horizontalLayout = new QHBoxLayout(scrollAreaWidgetContents);
        horizontalLayout->setSpacing(7);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        textBrowser = new QTextBrowser(scrollAreaWidgetContents);
        textBrowser->setObjectName(QStringLiteral("textBrowser"));
        QFont font;
        font.setPointSize(11);
        textBrowser->setFont(font);
        textBrowser->setStyleSheet(QStringLiteral("Background-color = rgb(190, 190, 190)"));
        textBrowser->setFrameShape(QFrame::NoFrame);
        textBrowser->setFrameShadow(QFrame::Plain);
        textBrowser->setLineWidth(0);

        horizontalLayout->addWidget(textBrowser);

        scrollArea->setWidget(scrollAreaWidgetContents);
        label = new QLabel(UpdateApp);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(20, 70, 220, 31));
        QFont font1;
        font1.setPointSize(12);
        label->setFont(font1);
        label_2 = new QLabel(UpdateApp);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setGeometry(QRect(10, 380, 561, 31));
        label_2->setFont(font1);
        label_3 = new QLabel(UpdateApp);
        label_3->setObjectName(QStringLiteral("label_3"));
        label_3->setGeometry(QRect(20, 20, 571, 31));
        label_3->setFont(font1);
        label_3->setAlignment(Qt::AlignCenter);

        retranslateUi(UpdateApp);
        QObject::connect(buttonBox, SIGNAL(accepted()), UpdateApp, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), UpdateApp, SLOT(reject()));

        QMetaObject::connectSlotsByName(UpdateApp);
    } // setupUi

    void retranslateUi(QDialog *UpdateApp)
    {
        UpdateApp->setWindowTitle(QApplication::translate("UpdateApp", "Winnow", nullptr));
        textBrowser->setHtml(QApplication::translate("UpdateApp", "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.0//EN\" \"http://www.w3.org/TR/REC-html40/strict.dtd\">\n"
"<html><head><meta name=\"qrichtext\" content=\"1\" /><style type=\"text/css\">\n"
"p, li { white-space: pre-wrap; }\n"
"</style></head><body style=\" font-family:'MS Shell Dlg 2'; font-size:11pt; font-weight:400; font-style:normal;\">\n"
"<p style=\"-qt-paragraph-type:empty; margin-top:8px; margin-bottom:0px; margin-left:0px; margin-right:0px; -qt-block-indent:0; text-indent:0px;\"><br /></p></body></html>", nullptr));
        label->setText(QApplication::translate("UpdateApp", "Update notes:", nullptr));
        label_2->setText(QApplication::translate("UpdateApp", "Do you want to proceed with the update?", nullptr));
        label_3->setText(QApplication::translate("UpdateApp", "There is a Winnow update", nullptr));
    } // retranslateUi

};

namespace Ui {
    class UpdateApp: public Ui_UpdateApp {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_UPDATEAPP_H
