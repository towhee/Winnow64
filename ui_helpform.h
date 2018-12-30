/********************************************************************************
** Form generated from reading UI file 'helpform.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HELPFORM_H
#define UI_HELPFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_helpForm
{
public:
    QGridLayout *gridLayout;
    QVBoxLayout *verticalLayout;
    QTextBrowser *textBrowser;

    void setupUi(QWidget *helpForm)
    {
        if (helpForm->objectName().isEmpty())
            helpForm->setObjectName(QString::fromUtf8("helpForm"));
        helpForm->setWindowModality(Qt::NonModal);
        helpForm->resize(824, 690);
        helpForm->setStyleSheet(QString::fromUtf8("font-size: 14px;"));
        gridLayout = new QGridLayout(helpForm);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setContentsMargins(0, 0, 0, 0);
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        textBrowser = new QTextBrowser(helpForm);
        textBrowser->setObjectName(QString::fromUtf8("textBrowser"));
        textBrowser->setSource(QUrl(QString::fromUtf8("qrc:/Docs/Winnow help.html")));
        textBrowser->setOpenExternalLinks(false);

        verticalLayout->addWidget(textBrowser);


        gridLayout->addLayout(verticalLayout, 0, 0, 1, 1);


        retranslateUi(helpForm);

        QMetaObject::connectSlotsByName(helpForm);
    } // setupUi

    void retranslateUi(QWidget *helpForm)
    {
        helpForm->setWindowTitle(QApplication::translate("helpForm", "Winnow help", nullptr));
    } // retranslateUi

};

namespace Ui {
    class helpForm: public Ui_helpForm {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_HELPFORM_H
