/********************************************************************************
** Form generated from reading UI file 'renamedlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RENAMEDLG_H
#define UI_RENAMEDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>
#include "renamedlg.h"

QT_BEGIN_NAMESPACE

class Ui_RenameDlg
{
public:
    QLabel *nameLbl;
    RenameEdit *name;
    QLabel *statusLbl;
    QWidget *layoutWidget;
    QHBoxLayout *horizontalLayout;
    QPushButton *cancelBtn;
    QPushButton *okBtn;

    void setupUi(QDialog *RenameDlg)
    {
        if (RenameDlg->objectName().isEmpty())
            RenameDlg->setObjectName(QString::fromUtf8("RenameDlg"));
        RenameDlg->resize(440, 222);
        nameLbl = new QLabel(RenameDlg);
        nameLbl->setObjectName(QString::fromUtf8("nameLbl"));
        nameLbl->setGeometry(QRect(30, 30, 371, 16));
        name = new RenameEdit(RenameDlg);
        name->setObjectName(QString::fromUtf8("name"));
        name->setGeometry(QRect(30, 50, 381, 22));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(name->sizePolicy().hasHeightForWidth());
        name->setSizePolicy(sizePolicy);
        statusLbl = new QLabel(RenameDlg);
        statusLbl->setObjectName(QString::fromUtf8("statusLbl"));
        statusLbl->setGeometry(QRect(30, 100, 381, 16));
        layoutWidget = new QWidget(RenameDlg);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(210, 170, 195, 30));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        cancelBtn = new QPushButton(layoutWidget);
        cancelBtn->setObjectName(QString::fromUtf8("cancelBtn"));

        horizontalLayout->addWidget(cancelBtn);

        okBtn = new QPushButton(layoutWidget);
        okBtn->setObjectName(QString::fromUtf8("okBtn"));

        horizontalLayout->addWidget(okBtn);


        retranslateUi(RenameDlg);

        QMetaObject::connectSlotsByName(RenameDlg);
    } // setupUi

    void retranslateUi(QDialog *RenameDlg)
    {
        RenameDlg->setWindowTitle(QCoreApplication::translate("RenameDlg", "Rename", nullptr));
        nameLbl->setText(QCoreApplication::translate("RenameDlg", "Name:", nullptr));
        statusLbl->setText(QString());
        cancelBtn->setText(QCoreApplication::translate("RenameDlg", "Cancel", nullptr));
        okBtn->setText(QCoreApplication::translate("RenameDlg", "OK", nullptr));
    } // retranslateUi

};

namespace Ui {
    class RenameDlg: public Ui_RenameDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RENAMEDLG_H
