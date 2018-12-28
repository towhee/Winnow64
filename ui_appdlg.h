/********************************************************************************
** Form generated from reading UI file 'appdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_APPDLG_H
#define UI_APPDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Appdlg
{
public:
    QWidget *horizontalLayoutWidget;
    QHBoxLayout *horizontalLayout;
    QTableWidget *apps;
    QWidget *horizontalLayoutWidget_2;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *addBtn;
    QPushButton *removeBtn;
    QPushButton *okBtn;

    void setupUi(QDialog *Appdlg)
    {
        if (Appdlg->objectName().isEmpty())
            Appdlg->setObjectName(QString::fromUtf8("Appdlg"));
        Appdlg->resize(703, 585);
        horizontalLayoutWidget = new QWidget(Appdlg);
        horizontalLayoutWidget->setObjectName(QString::fromUtf8("horizontalLayoutWidget"));
        horizontalLayoutWidget->setGeometry(QRect(19, 19, 661, 441));
        horizontalLayout = new QHBoxLayout(horizontalLayoutWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        apps = new QTableWidget(horizontalLayoutWidget);
        if (apps->columnCount() < 2)
            apps->setColumnCount(2);
        apps->setObjectName(QString::fromUtf8("apps"));
        apps->setColumnCount(2);
        apps->horizontalHeader()->setDefaultSectionSize(300);
        apps->verticalHeader()->setVisible(false);

        horizontalLayout->addWidget(apps);

        horizontalLayoutWidget_2 = new QWidget(Appdlg);
        horizontalLayoutWidget_2->setObjectName(QString::fromUtf8("horizontalLayoutWidget_2"));
        horizontalLayoutWidget_2->setGeometry(QRect(220, 519, 441, 51));
        horizontalLayout_2 = new QHBoxLayout(horizontalLayoutWidget_2);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
        addBtn = new QPushButton(horizontalLayoutWidget_2);
        addBtn->setObjectName(QString::fromUtf8("addBtn"));

        horizontalLayout_2->addWidget(addBtn);

        removeBtn = new QPushButton(horizontalLayoutWidget_2);
        removeBtn->setObjectName(QString::fromUtf8("removeBtn"));

        horizontalLayout_2->addWidget(removeBtn);

        okBtn = new QPushButton(horizontalLayoutWidget_2);
        okBtn->setObjectName(QString::fromUtf8("okBtn"));

        horizontalLayout_2->addWidget(okBtn);


        retranslateUi(Appdlg);

        QMetaObject::connectSlotsByName(Appdlg);
    } // setupUi

    void retranslateUi(QDialog *Appdlg)
    {
        Appdlg->setWindowTitle(QApplication::translate("Appdlg", "Open with Program List", nullptr));
        addBtn->setText(QApplication::translate("Appdlg", "Add Program", nullptr));
        removeBtn->setText(QApplication::translate("Appdlg", "Remove Program", nullptr));
        okBtn->setText(QApplication::translate("Appdlg", "OK", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Appdlg: public Ui_Appdlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_APPDLG_H
