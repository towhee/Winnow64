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
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_Appdlg
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QTableWidget *appsTable;
    QSpacerItem *horizontalSpacer;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *moveDown;
    QPushButton *moveUp;
    QPushButton *addBtn;
    QPushButton *removeBtn;
    QPushButton *okBtn;
    QPushButton *cancelBtn;

    void setupUi(QDialog *Appdlg)
    {
        if (Appdlg->objectName().isEmpty())
            Appdlg->setObjectName(QString::fromUtf8("Appdlg"));
        Appdlg->resize(948, 400);
        verticalLayout = new QVBoxLayout(Appdlg);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        appsTable = new QTableWidget(Appdlg);
        if (appsTable->columnCount() < 3)
            appsTable->setColumnCount(3);
        appsTable->setObjectName(QString::fromUtf8("appsTable"));
        appsTable->setFrameShape(QFrame::Box);
        appsTable->setEditTriggers(QAbstractItemView::AnyKeyPressed|QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed|QAbstractItemView::SelectedClicked);
        appsTable->setAlternatingRowColors(true);
        appsTable->setWordWrap(false);
        appsTable->setColumnCount(3);
        appsTable->horizontalHeader()->setDefaultSectionSize(300);
        appsTable->verticalHeader()->setVisible(false);
        appsTable->verticalHeader()->setDefaultSectionSize(26);
        appsTable->verticalHeader()->setMinimumSectionSize(24);

        horizontalLayout->addWidget(appsTable);


        verticalLayout->addLayout(horizontalLayout);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        verticalLayout->addItem(horizontalSpacer);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        moveDown = new QPushButton(Appdlg);
        moveDown->setObjectName(QString::fromUtf8("moveDown"));
        moveDown->setAutoDefault(false);

        horizontalLayout_2->addWidget(moveDown);

        moveUp = new QPushButton(Appdlg);
        moveUp->setObjectName(QString::fromUtf8("moveUp"));
        moveUp->setAutoDefault(false);

        horizontalLayout_2->addWidget(moveUp);

        addBtn = new QPushButton(Appdlg);
        addBtn->setObjectName(QString::fromUtf8("addBtn"));
        addBtn->setAutoDefault(false);

        horizontalLayout_2->addWidget(addBtn);

        removeBtn = new QPushButton(Appdlg);
        removeBtn->setObjectName(QString::fromUtf8("removeBtn"));
        removeBtn->setAutoDefault(false);

        horizontalLayout_2->addWidget(removeBtn);

        okBtn = new QPushButton(Appdlg);
        okBtn->setObjectName(QString::fromUtf8("okBtn"));
        okBtn->setAutoDefault(false);

        horizontalLayout_2->addWidget(okBtn);

        cancelBtn = new QPushButton(Appdlg);
        cancelBtn->setObjectName(QString::fromUtf8("cancelBtn"));
        cancelBtn->setAutoDefault(false);

        horizontalLayout_2->addWidget(cancelBtn);


        verticalLayout->addLayout(horizontalLayout_2);


        retranslateUi(Appdlg);

        QMetaObject::connectSlotsByName(Appdlg);
    } // setupUi

    void retranslateUi(QDialog *Appdlg)
    {
        Appdlg->setWindowTitle(QApplication::translate("Appdlg", "External Program List", nullptr));
#ifndef QT_NO_TOOLTIP
        appsTable->setToolTip(QApplication::translate("Appdlg", "<html><head/><body><p>Select a row before moving or deleting.  Double click in the name field to edit the app name.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        moveDown->setToolTip(QApplication::translate("Appdlg", "Select a row and then click this button to move the row down", nullptr));
#endif // QT_NO_TOOLTIP
        moveDown->setText(QApplication::translate("Appdlg", "Move Down", nullptr));
#ifndef QT_NO_TOOLTIP
        moveUp->setToolTip(QApplication::translate("Appdlg", "Select a row and then click this button to move the row up", nullptr));
#endif // QT_NO_TOOLTIP
        moveUp->setText(QApplication::translate("Appdlg", "Move Up", nullptr));
#ifndef QT_NO_TOOLTIP
        addBtn->setToolTip(QApplication::translate("Appdlg", "Add a program to the table (up to 10 programs)", nullptr));
#endif // QT_NO_TOOLTIP
        addBtn->setText(QApplication::translate("Appdlg", "Add Program", nullptr));
#ifndef QT_NO_TOOLTIP
        removeBtn->setToolTip(QApplication::translate("Appdlg", "Remove the selected program form the table", nullptr));
#endif // QT_NO_TOOLTIP
        removeBtn->setText(QApplication::translate("Appdlg", "Remove Program", nullptr));
#ifndef QT_NO_TOOLTIP
        okBtn->setToolTip(QApplication::translate("Appdlg", "Save the changes, which will be reflected in the external applications available in \"Open with...\"", nullptr));
#endif // QT_NO_TOOLTIP
        okBtn->setText(QApplication::translate("Appdlg", "Save changes", nullptr));
#ifndef QT_NO_TOOLTIP
        cancelBtn->setToolTip(QApplication::translate("Appdlg", "Exit without saving any changes", nullptr));
#endif // QT_NO_TOOLTIP
        cancelBtn->setText(QApplication::translate("Appdlg", "Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Appdlg: public Ui_Appdlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_APPDLG_H
