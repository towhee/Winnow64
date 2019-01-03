/********************************************************************************
** Form generated from reading UI file 'helpform.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_HELPFORM_H
#define UI_HELPFORM_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHeaderView>
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
            helpForm->setObjectName(QStringLiteral("helpForm"));
        helpForm->setWindowModality(Qt::NonModal);
        helpForm->resize(824, 690);
        helpForm->setStyleSheet(QStringLiteral("font-size: 14px;"));
        gridLayout = new QGridLayout(helpForm);
        gridLayout->setObjectName(QStringLiteral("gridLayout"));
        gridLayout->setContentsMargins(0, 0, 0, 0);
        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        textBrowser = new QTextBrowser(helpForm);
        textBrowser->setObjectName(QStringLiteral("textBrowser"));
        textBrowser->setSource(QUrl(QStringLiteral("qrc:/Docs/Winnow help.html")));
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
