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
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_UpdateApp
{
public:
    QDialogButtonBox *buttonBox;
    QLabel *proceedLabel;
    QLabel *titleLabel;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QLabel *viewVersionLabel;
    QLabel *versionLabel;
    QLabel *currentVersionlabel;

    void setupUi(QDialog *UpdateApp)
    {
        if (UpdateApp->objectName().isEmpty())
            UpdateApp->setObjectName(QStringLiteral("UpdateApp"));
        UpdateApp->resize(649, 381);
        UpdateApp->setAutoFillBackground(false);
        UpdateApp->setStyleSheet(QStringLiteral("Background-color =rgb(190, 190, 190)"));
        buttonBox = new QDialogButtonBox(UpdateApp);
        buttonBox->setObjectName(QStringLiteral("buttonBox"));
        buttonBox->setGeometry(QRect(10, 330, 621, 32));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::No|QDialogButtonBox::Yes);
        proceedLabel = new QLabel(UpdateApp);
        proceedLabel->setObjectName(QStringLiteral("proceedLabel"));
        proceedLabel->setGeometry(QRect(10, 260, 561, 31));
        QFont font;
        font.setPointSize(12);
        proceedLabel->setFont(font);
        titleLabel = new QLabel(UpdateApp);
        titleLabel->setObjectName(QStringLiteral("titleLabel"));
        titleLabel->setGeometry(QRect(20, 20, 571, 31));
        QFont font1;
        font1.setPointSize(14);
        titleLabel->setFont(font1);
        titleLabel->setAlignment(Qt::AlignCenter);
        scrollArea = new QScrollArea(UpdateApp);
        scrollArea->setObjectName(QStringLiteral("scrollArea"));
        scrollArea->setGeometry(QRect(10, 100, 621, 111));
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QStringLiteral("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 619, 109));
        viewVersionLabel = new QLabel(scrollAreaWidgetContents);
        viewVersionLabel->setObjectName(QStringLiteral("viewVersionLabel"));
        viewVersionLabel->setGeometry(QRect(10, 50, 601, 31));
        viewVersionLabel->setFont(font);
        versionLabel = new QLabel(scrollAreaWidgetContents);
        versionLabel->setObjectName(QStringLiteral("versionLabel"));
        versionLabel->setGeometry(QRect(120, 15, 441, 31));
        versionLabel->setFont(font);
        currentVersionlabel = new QLabel(scrollAreaWidgetContents);
        currentVersionlabel->setObjectName(QStringLiteral("currentVersionlabel"));
        currentVersionlabel->setGeometry(QRect(10, 15, 111, 31));
        currentVersionlabel->setFont(font);
        scrollArea->setWidget(scrollAreaWidgetContents);

        retranslateUi(UpdateApp);
        QObject::connect(buttonBox, SIGNAL(accepted()), UpdateApp, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), UpdateApp, SLOT(reject()));

        QMetaObject::connectSlotsByName(UpdateApp);
    } // setupUi

    void retranslateUi(QDialog *UpdateApp)
    {
        UpdateApp->setWindowTitle(QApplication::translate("UpdateApp", "Winnow", nullptr));
        proceedLabel->setText(QApplication::translate("UpdateApp", "Do you want to proceed with the update?", nullptr));
        titleLabel->setText(QApplication::translate("UpdateApp", "There is a Winnow update", nullptr));
        viewVersionLabel->setText(QApplication::translate("UpdateApp", "<html><head/><body><p>You can view version update notes <a href=\"http://165.227.46.158/winnow/winnow.html\"><span style=\" text-decoration: underline; color:#ffffff;\">here</span></a>.</p></body></html>", nullptr));
        versionLabel->setText(QApplication::translate("UpdateApp", "Version 0.9.5", nullptr));
        currentVersionlabel->setText(QApplication::translate("UpdateApp", "You have:", nullptr));
    } // retranslateUi

};

namespace Ui {
    class UpdateApp: public Ui_UpdateApp {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_UPDATEAPP_H
