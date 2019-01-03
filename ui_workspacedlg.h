/********************************************************************************
** Form generated from reading UI file 'workspacedlg.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_WORKSPACEDLG_H
#define UI_WORKSPACEDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Workspacedlg
{
public:
    QComboBox *workspaceCB;
    QLabel *workspaceLbl;
    QWidget *layoutWidget;
    QHBoxLayout *horizontalLayout;
    QPushButton *deleteBtn;
    QPushButton *reassignBtn;
    QPushButton *doneBtn;
    QLabel *status;

    void setupUi(QDialog *Workspacedlg)
    {
        if (Workspacedlg->objectName().isEmpty())
            Workspacedlg->setObjectName(QStringLiteral("Workspacedlg"));
        Workspacedlg->resize(489, 197);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(Workspacedlg->sizePolicy().hasHeightForWidth());
        Workspacedlg->setSizePolicy(sizePolicy);
        workspaceCB = new QComboBox(Workspacedlg);
        workspaceCB->setObjectName(QStringLiteral("workspaceCB"));
        workspaceCB->setGeometry(QRect(27, 50, 431, 28));
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(workspaceCB->sizePolicy().hasHeightForWidth());
        workspaceCB->setSizePolicy(sizePolicy1);
        workspaceCB->setEditable(true);
        workspaceCB->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
        workspaceCB->setIconSize(QSize(16, 24));
        workspaceLbl = new QLabel(Workspacedlg);
        workspaceLbl->setObjectName(QStringLiteral("workspaceLbl"));
        workspaceLbl->setGeometry(QRect(32, 30, 91, 16));
        layoutWidget = new QWidget(Workspacedlg);
        layoutWidget->setObjectName(QStringLiteral("layoutWidget"));
        layoutWidget->setGeometry(QRect(24, 120, 431, 32));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        deleteBtn = new QPushButton(layoutWidget);
        deleteBtn->setObjectName(QStringLiteral("deleteBtn"));

        horizontalLayout->addWidget(deleteBtn);

        reassignBtn = new QPushButton(layoutWidget);
        reassignBtn->setObjectName(QStringLiteral("reassignBtn"));

        horizontalLayout->addWidget(reassignBtn);

        doneBtn = new QPushButton(layoutWidget);
        doneBtn->setObjectName(QStringLiteral("doneBtn"));

        horizontalLayout->addWidget(doneBtn);

        status = new QLabel(Workspacedlg);
        status->setObjectName(QStringLiteral("status"));
        status->setGeometry(QRect(20, 160, 441, 20));
        status->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        retranslateUi(Workspacedlg);
        QObject::connect(doneBtn, SIGNAL(pressed()), Workspacedlg, SLOT(accept()));

        doneBtn->setDefault(true);


        QMetaObject::connectSlotsByName(Workspacedlg);
    } // setupUi

    void retranslateUi(QDialog *Workspacedlg)
    {
        Workspacedlg->setWindowTitle(QApplication::translate("Workspacedlg", "Manage Workspaces", nullptr));
        workspaceLbl->setText(QApplication::translate("Workspacedlg", "Workspace:", nullptr));
        deleteBtn->setText(QApplication::translate("Workspacedlg", "Delete", nullptr));
        reassignBtn->setText(QApplication::translate("Workspacedlg", "Update to current layout", nullptr));
        doneBtn->setText(QApplication::translate("Workspacedlg", "Done", nullptr));
        status->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class Workspacedlg: public Ui_Workspacedlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_WORKSPACEDLG_H
