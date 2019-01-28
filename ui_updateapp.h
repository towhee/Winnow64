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

QT_BEGIN_NAMESPACE

class Ui_UpdateApp
{
public:
    QDialogButtonBox *buttonBox;
    QLabel *proceedLabel;
    QLabel *titleLabel;
    QLabel *currentVersionlabel_2;
    QLabel *versionLabel;
    QLabel *viewVersionLabel;

    void setupUi(QDialog *UpdateApp)
    {
        if (UpdateApp->objectName().isEmpty())
            UpdateApp->setObjectName(QStringLiteral("UpdateApp"));
        UpdateApp->resize(544, 299);
        UpdateApp->setAutoFillBackground(false);
        UpdateApp->setStyleSheet(QStringLiteral("Background-color =rgb(190, 190, 190)"));
        buttonBox = new QDialogButtonBox(UpdateApp);
        buttonBox->setObjectName(QStringLiteral("buttonBox"));
        buttonBox->setGeometry(QRect(280, 250, 241, 32));
        QFont font;
        font.setPointSize(9);
        buttonBox->setFont(font);
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::No|QDialogButtonBox::Yes);
        proceedLabel = new QLabel(UpdateApp);
        proceedLabel->setObjectName(QStringLiteral("proceedLabel"));
        proceedLabel->setGeometry(QRect(30, 190, 391, 31));
        proceedLabel->setFont(font);
        titleLabel = new QLabel(UpdateApp);
        titleLabel->setObjectName(QStringLiteral("titleLabel"));
        titleLabel->setGeometry(QRect(30, 20, 571, 31));
        titleLabel->setFont(font);
        titleLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        currentVersionlabel_2 = new QLabel(UpdateApp);
        currentVersionlabel_2->setObjectName(QStringLiteral("currentVersionlabel_2"));
        currentVersionlabel_2->setGeometry(QRect(440, 670, 161, 31));
        QFont font1;
        font1.setPointSize(12);
        currentVersionlabel_2->setFont(font1);
        versionLabel = new QLabel(UpdateApp);
        versionLabel->setObjectName(QStringLiteral("versionLabel"));
        versionLabel->setGeometry(QRect(30, 80, 481, 31));
        versionLabel->setFont(font);
        viewVersionLabel = new QLabel(UpdateApp);
        viewVersionLabel->setObjectName(QStringLiteral("viewVersionLabel"));
        viewVersionLabel->setGeometry(QRect(30, 120, 601, 31));
        viewVersionLabel->setFont(font);

        retranslateUi(UpdateApp);
        QObject::connect(buttonBox, SIGNAL(accepted()), UpdateApp, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), UpdateApp, SLOT(reject()));

        QMetaObject::connectSlotsByName(UpdateApp);
    } // setupUi

    void retranslateUi(QDialog *UpdateApp)
    {
        UpdateApp->setWindowTitle(QApplication::translate("UpdateApp", "Winnow Update", nullptr));
        proceedLabel->setText(QApplication::translate("UpdateApp", "Do you want to proceed with the update?", nullptr));
        titleLabel->setText(QApplication::translate("UpdateApp", "There is a Winnow update", nullptr));
        currentVersionlabel_2->setText(QApplication::translate("UpdateApp", "You have version:", nullptr));
        versionLabel->setText(QApplication::translate("UpdateApp", "You currently have version  0.9.5 released 2019-01-25", nullptr));
        viewVersionLabel->setText(QApplication::translate("UpdateApp", "<html><head/><body><p>You can view version update notes <a href=\"http://165.227.46.158/winnow/winnow.html\"><span style=\" text-decoration: underline; color:#ffffff;\">here</span></a>.</p></body></html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class UpdateApp: public Ui_UpdateApp {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_UPDATEAPP_H
