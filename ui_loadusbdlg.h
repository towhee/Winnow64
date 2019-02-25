/********************************************************************************
** Form generated from reading UI file 'loadusbdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
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
    QWidget *widget;
    QVBoxLayout *verticalLayout;
    QListWidget *usbList;
    QLabel *label;

    void setupUi(QDialog *LoadUsbDlg)
    {
        if (LoadUsbDlg->objectName().isEmpty())
            LoadUsbDlg->setObjectName(QString::fromUtf8("LoadUsbDlg"));
        LoadUsbDlg->resize(339, 241);
        widget = new QWidget(LoadUsbDlg);
        widget->setObjectName(QString::fromUtf8("widget"));
        widget->setGeometry(QRect(0, 0, 341, 241));
        verticalLayout = new QVBoxLayout(widget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(5, 5, 5, 5);
        usbList = new QListWidget(widget);
        usbList->setObjectName(QString::fromUtf8("usbList"));

        verticalLayout->addWidget(usbList);

        label = new QLabel(widget);
        label->setObjectName(QString::fromUtf8("label"));

        verticalLayout->addWidget(label);


        retranslateUi(LoadUsbDlg);

        QMetaObject::connectSlotsByName(LoadUsbDlg);
    } // setupUi

    void retranslateUi(QDialog *LoadUsbDlg)
    {
        LoadUsbDlg->setWindowTitle(QApplication::translate("LoadUsbDlg", "USB Drives Available", nullptr));
        label->setText(QApplication::translate("LoadUsbDlg", "Select USB drive to view images", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoadUsbDlg: public Ui_LoadUsbDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOADUSBDLG_H
