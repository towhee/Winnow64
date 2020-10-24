/********************************************************************************
** Form generated from reading UI file 'inputdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INPUTDLG_H
#define UI_INPUTDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_InputDlg
{
public:
    QVBoxLayout *verticalLayout_2;
    QSpacerItem *verticalSpacer_2;
    QVBoxLayout *verticalLayout;
    QLabel *descriptionText;
    QLineEdit *inputText;
    QLabel *msg;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QPushButton *cancelBtn;
    QPushButton *okBtn;

    void setupUi(QDialog *InputDlg)
    {
        if (InputDlg->objectName().isEmpty())
            InputDlg->setObjectName(QString::fromUtf8("InputDlg"));
        InputDlg->resize(423, 151);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(InputDlg->sizePolicy().hasHeightForWidth());
        InputDlg->setSizePolicy(sizePolicy);
        verticalLayout_2 = new QVBoxLayout(InputDlg);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalSpacer_2 = new QSpacerItem(20, 15, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout_2->addItem(verticalSpacer_2);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        descriptionText = new QLabel(InputDlg);
        descriptionText->setObjectName(QString::fromUtf8("descriptionText"));

        verticalLayout->addWidget(descriptionText);


        verticalLayout_2->addLayout(verticalLayout);

        inputText = new QLineEdit(InputDlg);
        inputText->setObjectName(QString::fromUtf8("inputText"));

        verticalLayout_2->addWidget(inputText);

        msg = new QLabel(InputDlg);
        msg->setObjectName(QString::fromUtf8("msg"));
        msg->setStyleSheet(QString::fromUtf8("color:red"));

        verticalLayout_2->addWidget(msg);

        verticalSpacer = new QSpacerItem(20, 15, QSizePolicy::Minimum, QSizePolicy::MinimumExpanding);

        verticalLayout_2->addItem(verticalSpacer);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        cancelBtn = new QPushButton(InputDlg);
        cancelBtn->setObjectName(QString::fromUtf8("cancelBtn"));
        cancelBtn->setMinimumSize(QSize(100, 0));
        cancelBtn->setFocusPolicy(Qt::StrongFocus);
        cancelBtn->setAutoDefault(false);

        horizontalLayout->addWidget(cancelBtn);

        okBtn = new QPushButton(InputDlg);
        okBtn->setObjectName(QString::fromUtf8("okBtn"));
        okBtn->setMinimumSize(QSize(100, 0));

        horizontalLayout->addWidget(okBtn);


        verticalLayout_2->addLayout(horizontalLayout);


        retranslateUi(InputDlg);

        QMetaObject::connectSlotsByName(InputDlg);
    } // setupUi

    void retranslateUi(QDialog *InputDlg)
    {
        InputDlg->setWindowTitle(QCoreApplication::translate("InputDlg", "Dialog", nullptr));
        descriptionText->setText(QCoreApplication::translate("InputDlg", "TextLabel", nullptr));
        msg->setText(QString());
        cancelBtn->setText(QCoreApplication::translate("InputDlg", "Cancel", nullptr));
        okBtn->setText(QCoreApplication::translate("InputDlg", "OK", nullptr));
    } // retranslateUi

};

namespace Ui {
    class InputDlg: public Ui_InputDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INPUTDLG_H
