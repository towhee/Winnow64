/********************************************************************************
** Form generated from reading UI file 'managetilesdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MANAGETILESDLG_H
#define UI_MANAGETILESDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_ManageTilesDlg
{
public:
    QVBoxLayout *verticalLayout_2;
    QVBoxLayout *verticalLayout;
    QLabel *label;
    QComboBox *tileBox;
    QLabel *tileLabel;
    QHBoxLayout *horizontalLayout_3;
    QSpacerItem *horizontalSpacer;
    QPushButton *deleteBtn;
    QPushButton *closeBtn;

    void setupUi(QDialog *ManageTilesDlg)
    {
        if (ManageTilesDlg->objectName().isEmpty())
            ManageTilesDlg->setObjectName(QString::fromUtf8("ManageTilesDlg"));
        ManageTilesDlg->resize(426, 218);
        verticalLayout_2 = new QVBoxLayout(ManageTilesDlg);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label = new QLabel(ManageTilesDlg);
        label->setObjectName(QString::fromUtf8("label"));

        verticalLayout->addWidget(label);

        tileBox = new QComboBox(ManageTilesDlg);
        tileBox->setObjectName(QString::fromUtf8("tileBox"));
        tileBox->setMinimumSize(QSize(400, 0));
        tileBox->setMaximumSize(QSize(400, 16777215));

        verticalLayout->addWidget(tileBox);


        verticalLayout_2->addLayout(verticalLayout);

        tileLabel = new QLabel(ManageTilesDlg);
        tileLabel->setObjectName(QString::fromUtf8("tileLabel"));
        tileLabel->setMinimumSize(QSize(100, 100));
        tileLabel->setMaximumSize(QSize(100, 100));
        tileLabel->setFrameShape(QFrame::Box);

        verticalLayout_2->addWidget(tileLabel);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer);

        deleteBtn = new QPushButton(ManageTilesDlg);
        deleteBtn->setObjectName(QString::fromUtf8("deleteBtn"));
        deleteBtn->setMinimumSize(QSize(100, 0));
        deleteBtn->setMaximumSize(QSize(100, 16777215));
        deleteBtn->setAutoDefault(false);

        horizontalLayout_3->addWidget(deleteBtn);

        closeBtn = new QPushButton(ManageTilesDlg);
        closeBtn->setObjectName(QString::fromUtf8("closeBtn"));
        closeBtn->setMinimumSize(QSize(100, 0));
        closeBtn->setMaximumSize(QSize(100, 16777215));
        closeBtn->setAutoDefault(false);

        horizontalLayout_3->addWidget(closeBtn);


        verticalLayout_2->addLayout(horizontalLayout_3);


        retranslateUi(ManageTilesDlg);

        closeBtn->setDefault(false);


        QMetaObject::connectSlotsByName(ManageTilesDlg);
    } // setupUi

    void retranslateUi(QDialog *ManageTilesDlg)
    {
        ManageTilesDlg->setWindowTitle(QCoreApplication::translate("ManageTilesDlg", "Manage Tiles", nullptr));
        label->setText(QCoreApplication::translate("ManageTilesDlg", "Tiles", nullptr));
        tileLabel->setText(QString());
        deleteBtn->setText(QCoreApplication::translate("ManageTilesDlg", "Delete", nullptr));
        closeBtn->setText(QCoreApplication::translate("ManageTilesDlg", "Close", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ManageTilesDlg: public Ui_ManageTilesDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MANAGETILESDLG_H
