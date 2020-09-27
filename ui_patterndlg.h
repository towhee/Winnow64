/********************************************************************************
** Form generated from reading UI file 'patterndlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PATTERNDLG_H
#define UI_PATTERNDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_PatternDlg
{
public:
    QDialogButtonBox *buttonBox;
    QWidget *verticalLayoutWidget;
    QVBoxLayout *verticalLayout;
    QLabel *sampleLabel;
    QPushButton *btn1;

    void setupUi(QDialog *PatternDlg)
    {
        if (PatternDlg->objectName().isEmpty())
            PatternDlg->setObjectName(QString::fromUtf8("PatternDlg"));
        PatternDlg->resize(640, 480);
        buttonBox = new QDialogButtonBox(PatternDlg);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setGeometry(QRect(10, 440, 621, 32));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
        verticalLayoutWidget = new QWidget(PatternDlg);
        verticalLayoutWidget->setObjectName(QString::fromUtf8("verticalLayoutWidget"));
        verticalLayoutWidget->setGeometry(QRect(-11, -11, 651, 441));
        verticalLayout = new QVBoxLayout(verticalLayoutWidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        sampleLabel = new QLabel(verticalLayoutWidget);
        sampleLabel->setObjectName(QString::fromUtf8("sampleLabel"));
        sampleLabel->setFrameShape(QFrame::Box);
        sampleLabel->setFrameShadow(QFrame::Raised);
        sampleLabel->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(sampleLabel);

        btn1 = new QPushButton(PatternDlg);
        btn1->setObjectName(QString::fromUtf8("btn1"));
        btn1->setGeometry(QRect(330, 440, 80, 21));

        retranslateUi(PatternDlg);
        QObject::connect(buttonBox, SIGNAL(accepted()), PatternDlg, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), PatternDlg, SLOT(reject()));

        QMetaObject::connectSlotsByName(PatternDlg);
    } // setupUi

    void retranslateUi(QDialog *PatternDlg)
    {
        PatternDlg->setWindowTitle(QCoreApplication::translate("PatternDlg", "Dialog", nullptr));
        sampleLabel->setText(QCoreApplication::translate("PatternDlg", "TextLabel", nullptr));
        btn1->setText(QCoreApplication::translate("PatternDlg", "PushButton", nullptr));
    } // retranslateUi

};

namespace Ui {
    class PatternDlg: public Ui_PatternDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PATTERNDLG_H
