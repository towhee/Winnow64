/********************************************************************************
** Form generated from reading UI file 'saveasdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SAVEASDLG_H
#define UI_SAVEASDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_SaveAsDlg
{
public:
    QVBoxLayout *verticalLayout;
    QSpacerItem *verticalSpacer_2;
    QHBoxLayout *horizontalLayout;
    QLabel *label_2;
    QComboBox *imageTypesCB;
    QSpacerItem *horizontalSpacer_4;
    QLabel *label_3;
    QSpinBox *compressionBox;
    QSpacerItem *horizontalSpacer_2;
    QSpacerItem *verticalSpacer_3;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label;
    QLabel *folderPath;
    QSpacerItem *verticalSpacer;
    QProgressBar *progressBar;
    QHBoxLayout *horizontalLayout_3;
    QSpacerItem *horizontalSpacer;
    QPushButton *selectBtn;
    QSpacerItem *horizontalSpacer_3;
    QPushButton *cancelBtn;
    QPushButton *saveBtn;

    void setupUi(QDialog *SaveAsDlg)
    {
        if (SaveAsDlg->objectName().isEmpty())
            SaveAsDlg->setObjectName(QString::fromUtf8("SaveAsDlg"));
        SaveAsDlg->resize(513, 183);
        verticalLayout = new QVBoxLayout(SaveAsDlg);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label_2 = new QLabel(SaveAsDlg);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setMinimumSize(QSize(100, 0));

        horizontalLayout->addWidget(label_2);

        imageTypesCB = new QComboBox(SaveAsDlg);
        imageTypesCB->setObjectName(QString::fromUtf8("imageTypesCB"));
        imageTypesCB->setMinimumSize(QSize(100, 0));

        horizontalLayout->addWidget(imageTypesCB);

        horizontalSpacer_4 = new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_4);

        label_3 = new QLabel(SaveAsDlg);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        horizontalLayout->addWidget(label_3);

        compressionBox = new QSpinBox(SaveAsDlg);
        compressionBox->setObjectName(QString::fromUtf8("compressionBox"));
        compressionBox->setMinimumSize(QSize(75, 0));
        compressionBox->setMaximum(100);

        horizontalLayout->addWidget(compressionBox);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);


        verticalLayout->addLayout(horizontalLayout);

        verticalSpacer_3 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_3);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        label = new QLabel(SaveAsDlg);
        label->setObjectName(QString::fromUtf8("label"));
        label->setMinimumSize(QSize(100, 0));
        label->setMaximumSize(QSize(100, 16777215));

        horizontalLayout_2->addWidget(label);

        folderPath = new QLabel(SaveAsDlg);
        folderPath->setObjectName(QString::fromUtf8("folderPath"));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(folderPath->sizePolicy().hasHeightForWidth());
        folderPath->setSizePolicy(sizePolicy);

        horizontalLayout_2->addWidget(folderPath);


        verticalLayout->addLayout(horizontalLayout_2);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);

        progressBar = new QProgressBar(SaveAsDlg);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setValue(24);

        verticalLayout->addWidget(progressBar);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer);

        selectBtn = new QPushButton(SaveAsDlg);
        selectBtn->setObjectName(QString::fromUtf8("selectBtn"));
        selectBtn->setMinimumSize(QSize(100, 0));
        selectBtn->setMaximumSize(QSize(100, 16777215));

        horizontalLayout_3->addWidget(selectBtn);

        horizontalSpacer_3 = new QSpacerItem(20, 20, QSizePolicy::Preferred, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_3);

        cancelBtn = new QPushButton(SaveAsDlg);
        cancelBtn->setObjectName(QString::fromUtf8("cancelBtn"));
        cancelBtn->setMinimumSize(QSize(100, 0));

        horizontalLayout_3->addWidget(cancelBtn);

        saveBtn = new QPushButton(SaveAsDlg);
        saveBtn->setObjectName(QString::fromUtf8("saveBtn"));
        saveBtn->setMinimumSize(QSize(100, 0));

        horizontalLayout_3->addWidget(saveBtn);


        verticalLayout->addLayout(horizontalLayout_3);


        retranslateUi(SaveAsDlg);

        QMetaObject::connectSlotsByName(SaveAsDlg);
    } // setupUi

    void retranslateUi(QDialog *SaveAsDlg)
    {
        SaveAsDlg->setWindowTitle(QCoreApplication::translate("SaveAsDlg", "Save as", nullptr));
        label_2->setText(QCoreApplication::translate("SaveAsDlg", "Image type    ", nullptr));
        label_3->setText(QCoreApplication::translate("SaveAsDlg", "Compression (0-100)    ", nullptr));
        label->setText(QCoreApplication::translate("SaveAsDlg", "Save to    ", nullptr));
        folderPath->setText(QString());
        selectBtn->setText(QCoreApplication::translate("SaveAsDlg", "Pick Folder", nullptr));
        cancelBtn->setText(QCoreApplication::translate("SaveAsDlg", "Cancel", nullptr));
        saveBtn->setText(QCoreApplication::translate("SaveAsDlg", "Save", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SaveAsDlg: public Ui_SaveAsDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SAVEASDLG_H
