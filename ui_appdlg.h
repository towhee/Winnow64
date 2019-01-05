/********************************************************************************
** Form generated from reading UI file 'appdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_APPDLG_H
#define UI_APPDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QTableWidget>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Appdlg
{
public:
    QWidget *horizontalLayoutWidget;
    QHBoxLayout *horizontalLayout;
    QTableWidget *appsTable;
    QWidget *horizontalLayoutWidget_2;
    QHBoxLayout *horizontalLayout_2;
    QPushButton *addBtn;
    QPushButton *removeBtn;
    QPushButton *okBtn;
    QPushButton *cancelBtn;
    QLabel *label;

    void setupUi(QDialog *Appdlg)
    {
        if (Appdlg->objectName().isEmpty())
            Appdlg->setObjectName(QStringLiteral("Appdlg"));
        Appdlg->resize(886, 683);
        horizontalLayoutWidget = new QWidget(Appdlg);
        horizontalLayoutWidget->setObjectName(QStringLiteral("horizontalLayoutWidget"));
        horizontalLayoutWidget->setGeometry(QRect(20, 20, 831, 441));
        horizontalLayout = new QHBoxLayout(horizontalLayoutWidget);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        appsTable = new QTableWidget(horizontalLayoutWidget);
        if (appsTable->columnCount() < 2)
            appsTable->setColumnCount(2);
        appsTable->setObjectName(QStringLiteral("appsTable"));
        appsTable->setFrameShape(QFrame::Box);
        appsTable->setEditTriggers(QAbstractItemView::AnyKeyPressed|QAbstractItemView::DoubleClicked|QAbstractItemView::EditKeyPressed|QAbstractItemView::SelectedClicked);
        appsTable->setColumnCount(2);
        appsTable->horizontalHeader()->setDefaultSectionSize(300);
        appsTable->verticalHeader()->setVisible(false);

        horizontalLayout->addWidget(appsTable);

        horizontalLayoutWidget_2 = new QWidget(Appdlg);
        horizontalLayoutWidget_2->setObjectName(QStringLiteral("horizontalLayoutWidget_2"));
        horizontalLayoutWidget_2->setGeometry(QRect(210, 600, 641, 51));
        horizontalLayout_2 = new QHBoxLayout(horizontalLayoutWidget_2);
        horizontalLayout_2->setObjectName(QStringLiteral("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
        addBtn = new QPushButton(horizontalLayoutWidget_2);
        addBtn->setObjectName(QStringLiteral("addBtn"));
        addBtn->setAutoDefault(false);

        horizontalLayout_2->addWidget(addBtn);

        removeBtn = new QPushButton(horizontalLayoutWidget_2);
        removeBtn->setObjectName(QStringLiteral("removeBtn"));
        removeBtn->setAutoDefault(false);

        horizontalLayout_2->addWidget(removeBtn);

        okBtn = new QPushButton(horizontalLayoutWidget_2);
        okBtn->setObjectName(QStringLiteral("okBtn"));
        okBtn->setAutoDefault(false);

        horizontalLayout_2->addWidget(okBtn);

        cancelBtn = new QPushButton(horizontalLayoutWidget_2);
        cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
        cancelBtn->setAutoDefault(false);

        horizontalLayout_2->addWidget(cancelBtn);

        label = new QLabel(Appdlg);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(20, 480, 821, 101));

        retranslateUi(Appdlg);

        QMetaObject::connectSlotsByName(Appdlg);
    } // setupUi

    void retranslateUi(QDialog *Appdlg)
    {
        Appdlg->setWindowTitle(QApplication::translate("Appdlg", "External Program List", nullptr));
        addBtn->setText(QApplication::translate("Appdlg", "Add Program", nullptr));
        removeBtn->setText(QApplication::translate("Appdlg", "Remove Program", nullptr));
        okBtn->setText(QApplication::translate("Appdlg", "Save changes", nullptr));
        cancelBtn->setText(QApplication::translate("Appdlg", "Cancel", nullptr));
        label->setText(QApplication::translate("Appdlg", "<html><head/><body><p><span style=\" color:#ffff00;\">Add</span> external programs using the &quot;Add Program&quot; button. The program will be added to the table. </p><p>To <span style=\" color:#ffff00;\">remove</span> a program select the program in the table and click &quot;Remove Program&quot; </p><p>To <span style=\" color:#ffff00;\">change</span> the displayed program name edit the table cell.</p></body></html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class Appdlg: public Ui_Appdlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_APPDLG_H
