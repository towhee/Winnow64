/********************************************************************************
** Form generated from reading UI file 'metadatareport.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_METADATAREPORT_H
#define UI_METADATAREPORT_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_metadataReporttDlg
{
public:
    QVBoxLayout *verticalLayout;
    QTextBrowser *textBrowser;

    void setupUi(QDialog *metadataReporttDlg)
    {
        if (metadataReporttDlg->objectName().isEmpty())
            metadataReporttDlg->setObjectName(QString::fromUtf8("metadataReporttDlg"));
        metadataReporttDlg->resize(1385, 769);
        verticalLayout = new QVBoxLayout(metadataReporttDlg);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        textBrowser = new QTextBrowser(metadataReporttDlg);
        textBrowser->setObjectName(QString::fromUtf8("textBrowser"));
        QFont font;
        font.setFamily(QString::fromUtf8("Courier New"));
        font.setPointSize(12);
        textBrowser->setFont(font);

        verticalLayout->addWidget(textBrowser);


        retranslateUi(metadataReporttDlg);

        QMetaObject::connectSlotsByName(metadataReporttDlg);
    } // setupUi

    void retranslateUi(QDialog *metadataReporttDlg)
    {
        metadataReporttDlg->setWindowTitle(QApplication::translate("metadataReporttDlg", " Winnow Metadata Repot", nullptr));
    } // retranslateUi

};

namespace Ui {
    class metadataReporttDlg: public Ui_metadataReporttDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_METADATAREPORT_H
