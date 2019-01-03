/********************************************************************************
** Form generated from reading UI file 'testaligndlg.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TESTALIGNDLG_H
#define UI_TESTALIGNDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>

QT_BEGIN_NAMESPACE

class Ui_TestAlignDlg
{
public:
    QDialogButtonBox *buttonBox;
    QLabel *label;
    QLabel *label_2;
    QLabel *targetLbl;
    QLabel *alignLbl;

    void setupUi(QDialog *TestAlignDlg)
    {
        if (TestAlignDlg->objectName().isEmpty())
            TestAlignDlg->setObjectName(QStringLiteral("TestAlignDlg"));
        TestAlignDlg->resize(381, 320);
        buttonBox = new QDialogButtonBox(TestAlignDlg);
        buttonBox->setObjectName(QStringLiteral("buttonBox"));
        buttonBox->setGeometry(QRect(120, 270, 221, 41));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
        label = new QLabel(TestAlignDlg);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(20, 30, 60, 16));
        label_2 = new QLabel(TestAlignDlg);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setGeometry(QRect(190, 30, 60, 16));
        targetLbl = new QLabel(TestAlignDlg);
        targetLbl->setObjectName(QStringLiteral("targetLbl"));
        targetLbl->setGeometry(QRect(20, 60, 150, 150));
        targetLbl->setFrameShape(QFrame::Box);
        alignLbl = new QLabel(TestAlignDlg);
        alignLbl->setObjectName(QStringLiteral("alignLbl"));
        alignLbl->setGeometry(QRect(190, 60, 150, 150));
        alignLbl->setFrameShape(QFrame::Box);

        retranslateUi(TestAlignDlg);
        QObject::connect(buttonBox, SIGNAL(accepted()), TestAlignDlg, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), TestAlignDlg, SLOT(reject()));

        QMetaObject::connectSlotsByName(TestAlignDlg);
    } // setupUi

    void retranslateUi(QDialog *TestAlignDlg)
    {
        TestAlignDlg->setWindowTitle(QApplication::translate("TestAlignDlg", "Dialog", nullptr));
        label->setText(QApplication::translate("TestAlignDlg", "Target", nullptr));
        label_2->setText(QApplication::translate("TestAlignDlg", "Align", nullptr));
        targetLbl->setText(QString());
        alignLbl->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class TestAlignDlg: public Ui_TestAlignDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TESTALIGNDLG_H
