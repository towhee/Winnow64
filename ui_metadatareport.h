/********************************************************************************
** Form generated from reading UI file 'metadatareport.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_METADATAREPORT_H
#define UI_METADATAREPORT_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHeaderView>
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
            metadataReporttDlg->setObjectName(QStringLiteral("metadataReporttDlg"));
        metadataReporttDlg->resize(1189, 769);
        verticalLayout = new QVBoxLayout(metadataReporttDlg);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        textBrowser = new QTextBrowser(metadataReporttDlg);
        textBrowser->setObjectName(QStringLiteral("textBrowser"));
        QFont font;
        font.setFamily(QStringLiteral("Courier New"));
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
