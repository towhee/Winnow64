/********************************************************************************
** Form generated from reading UI file 'updateapp.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_UPDATEAPP_H
#define UI_UPDATEAPP_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_UpdateApp
{
public:
    QLabel *proceedLabel;
    QLabel *titleLabel;
    QLabel *currentVersionlabel_2;
    QLabel *versionLabel;
    QLabel *viewVersionLabel;
    QPushButton *cancelBtn;
    QPushButton *yesBtn;

    void setupUi(QDialog *UpdateApp)
    {
        if (UpdateApp->objectName().isEmpty())
            UpdateApp->setObjectName(QString::fromUtf8("UpdateApp"));
        UpdateApp->resize(544, 248);
        UpdateApp->setAutoFillBackground(false);
        UpdateApp->setStyleSheet(QString::fromUtf8("Background-color =rgb(190, 190, 190)"));
        proceedLabel = new QLabel(UpdateApp);
        proceedLabel->setObjectName(QString::fromUtf8("proceedLabel"));
        proceedLabel->setGeometry(QRect(30, 130, 391, 31));
        QFont font;
        font.setPointSize(10);
        proceedLabel->setFont(font);
        titleLabel = new QLabel(UpdateApp);
        titleLabel->setObjectName(QString::fromUtf8("titleLabel"));
        titleLabel->setGeometry(QRect(30, 20, 571, 31));
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(true);
        font1.setWeight(75);
        titleLabel->setFont(font1);
        titleLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        currentVersionlabel_2 = new QLabel(UpdateApp);
        currentVersionlabel_2->setObjectName(QString::fromUtf8("currentVersionlabel_2"));
        currentVersionlabel_2->setGeometry(QRect(440, 670, 161, 31));
        QFont font2;
        font2.setPointSize(12);
        currentVersionlabel_2->setFont(font2);
        versionLabel = new QLabel(UpdateApp);
        versionLabel->setObjectName(QString::fromUtf8("versionLabel"));
        versionLabel->setGeometry(QRect(30, 60, 481, 31));
        versionLabel->setFont(font);
        viewVersionLabel = new QLabel(UpdateApp);
        viewVersionLabel->setObjectName(QString::fromUtf8("viewVersionLabel"));
        viewVersionLabel->setGeometry(QRect(30, 84, 601, 31));
        viewVersionLabel->setFont(font);
        cancelBtn = new QPushButton(UpdateApp);
        cancelBtn->setObjectName(QString::fromUtf8("cancelBtn"));
        cancelBtn->setGeometry(QRect(320, 200, 93, 28));
        cancelBtn->setAutoDefault(false);
        yesBtn = new QPushButton(UpdateApp);
        yesBtn->setObjectName(QString::fromUtf8("yesBtn"));
        yesBtn->setGeometry(QRect(430, 200, 100, 28));

        retranslateUi(UpdateApp);

        yesBtn->setDefault(true);


        QMetaObject::connectSlotsByName(UpdateApp);
    } // setupUi

    void retranslateUi(QDialog *UpdateApp)
    {
        UpdateApp->setWindowTitle(QApplication::translate("UpdateApp", "Winnow Update", nullptr));
        proceedLabel->setText(QApplication::translate("UpdateApp", "Do you want to proceed with the update?", nullptr));
        titleLabel->setText(QApplication::translate("UpdateApp", "There is a Winnow update", nullptr));
        currentVersionlabel_2->setText(QApplication::translate("UpdateApp", "You have version:", nullptr));
        versionLabel->setText(QApplication::translate("UpdateApp", "You currently have version  0.9.5 released 2019-01-25", nullptr));
        viewVersionLabel->setText(QApplication::translate("UpdateApp", "<html><head/><body><p>You can view version update notes <a href=\"http://165.227.46.158/winnow/versions.html\"><span style=\" text-decoration: underline; color:#ffffff;\">here</span></a>.</p></body></html>", nullptr));
        cancelBtn->setText(QApplication::translate("UpdateApp", "Cancel", nullptr));
        yesBtn->setText(QApplication::translate("UpdateApp", "Proceed", nullptr));
    } // retranslateUi

};

namespace Ui {
    class UpdateApp: public Ui_UpdateApp {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_UPDATEAPP_H
