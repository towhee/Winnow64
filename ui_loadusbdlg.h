/********************************************************************************
** Form generated from reading UI file 'loadusbdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOADUSBDLG_H
#define UI_LOADUSBDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LoadUsbDlg
{
public:
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout;
    QListWidget *usbList;
    QLabel *label;

    void setupUi(QDialog *LoadUsbDlg)
    {
        if (LoadUsbDlg->objectName().isEmpty())
            LoadUsbDlg->setObjectName(QString::fromUtf8("LoadUsbDlg"));
        LoadUsbDlg->resize(339, 241);
        QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Minimum);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(LoadUsbDlg->sizePolicy().hasHeightForWidth());
        LoadUsbDlg->setSizePolicy(sizePolicy);
        LoadUsbDlg->setMinimumSize(QSize(300, 0));
        layoutWidget = new QWidget(LoadUsbDlg);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(0, 0, 341, 241));
        verticalLayout = new QVBoxLayout(layoutWidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(5, 5, 5, 5);
        usbList = new QListWidget(layoutWidget);
        usbList->setObjectName(QString::fromUtf8("usbList"));
        QSizePolicy sizePolicy1(QSizePolicy::MinimumExpanding, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(usbList->sizePolicy().hasHeightForWidth());
        usbList->setSizePolicy(sizePolicy1);

        verticalLayout->addWidget(usbList);

        label = new QLabel(layoutWidget);
        label->setObjectName(QString::fromUtf8("label"));
        sizePolicy1.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy1);

        verticalLayout->addWidget(label);


        retranslateUi(LoadUsbDlg);

        QMetaObject::connectSlotsByName(LoadUsbDlg);
    } // setupUi

    void retranslateUi(QDialog *LoadUsbDlg)
    {
        LoadUsbDlg->setWindowTitle(QCoreApplication::translate("LoadUsbDlg", "USB Drives Available", nullptr));
        label->setText(QCoreApplication::translate("LoadUsbDlg", "Select USB drive to view images", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoadUsbDlg: public Ui_LoadUsbDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOADUSBDLG_H
