/********************************************************************************
** Form generated from reading UI file 'copystyledlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_COPYSTYLEDLG_H
#define UI_COPYSTYLEDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QLabel>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_CopyStyleDlg
{
public:
    QVBoxLayout *verticalLayout;
    QFrame *frame;
    QVBoxLayout *verticalLayout_2;
    QSpacerItem *verticalSpacer_2;
    QLabel *label;
    QComboBox *templateList;
    QSpacerItem *verticalSpacer;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *CopyStyleDlg)
    {
        if (CopyStyleDlg->objectName().isEmpty())
            CopyStyleDlg->setObjectName(QString::fromUtf8("CopyStyleDlg"));
        CopyStyleDlg->resize(368, 163);
        verticalLayout = new QVBoxLayout(CopyStyleDlg);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        frame = new QFrame(CopyStyleDlg);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setFrameShape(QFrame::NoFrame);
        frame->setFrameShadow(QFrame::Raised);
        frame->setLineWidth(0);
        verticalLayout_2 = new QVBoxLayout(frame);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalSpacer_2 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer_2);

        label = new QLabel(frame);
        label->setObjectName(QString::fromUtf8("label"));

        verticalLayout_2->addWidget(label);

        templateList = new QComboBox(frame);
        templateList->setObjectName(QString::fromUtf8("templateList"));
        QSizePolicy sizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(templateList->sizePolicy().hasHeightForWidth());
        templateList->setSizePolicy(sizePolicy);

        verticalLayout_2->addWidget(templateList);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);


        verticalLayout->addWidget(frame);

        buttonBox = new QDialogButtonBox(CopyStyleDlg);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(CopyStyleDlg);
        QObject::connect(buttonBox, SIGNAL(accepted()), CopyStyleDlg, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), CopyStyleDlg, SLOT(reject()));

        QMetaObject::connectSlotsByName(CopyStyleDlg);
    } // setupUi

    void retranslateUi(QDialog *CopyStyleDlg)
    {
        CopyStyleDlg->setWindowTitle(QCoreApplication::translate("CopyStyleDlg", "Copy Style", nullptr));
        label->setText(QCoreApplication::translate("CopyStyleDlg", "Copy style to template:", nullptr));
    } // retranslateUi

};

namespace Ui {
    class CopyStyleDlg: public Ui_CopyStyleDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_COPYSTYLEDLG_H
