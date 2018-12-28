/********************************************************************************
** Form generated from reading UI file 'tokendlg.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TOKENDLG_H
#define UI_TOKENDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QWidget>
#include "tokendlg.h"

QT_BEGIN_NAMESPACE

class Ui_TokenDlg
{
public:
    TokenList *tokenList;
    TokenEdit *tokenEdit;
    QLabel *label;
    QLabel *label_2;
    QLabel *label_3;
    QComboBox *templatesCB;
    QLabel *label_4;
    QWidget *layoutWidget;
    QHBoxLayout *horizontalLayout;
    QPushButton *renameBtn;
    QPushButton *newBtn;
    QPushButton *deleteBtn;
    QSpacerItem *horizontalSpacer;
    QPushButton *okBtn;
    QTextBrowser *resultText;

    void setupUi(QDialog *TokenDlg)
    {
        if (TokenDlg->objectName().isEmpty())
            TokenDlg->setObjectName(QString::fromUtf8("TokenDlg"));
        TokenDlg->resize(766, 402);
        TokenDlg->setStyleSheet(QString::fromUtf8(""));
        tokenList = new TokenList(TokenDlg);
        tokenList->setObjectName(QString::fromUtf8("tokenList"));
        tokenList->setGeometry(QRect(30, 50, 141, 321));
        tokenEdit = new TokenEdit(TokenDlg);
        tokenEdit->setObjectName(QString::fromUtf8("tokenEdit"));
        tokenEdit->setGeometry(QRect(180, 130, 561, 61));
        QFont font;
        font.setPointSize(16);
        tokenEdit->setFont(font);
        tokenEdit->setStyleSheet(QString::fromUtf8(""));
        label = new QLabel(TokenDlg);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(30, 30, 91, 16));
        label_2 = new QLabel(TokenDlg);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(180, 110, 91, 16));
        label_3 = new QLabel(TokenDlg);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(180, 220, 141, 16));
        templatesCB = new QComboBox(TokenDlg);
        templatesCB->setObjectName(QString::fromUtf8("templatesCB"));
        templatesCB->setGeometry(QRect(180, 50, 561, 26));
        templatesCB->setEditable(false);
        templatesCB->setInsertPolicy(QComboBox::InsertAtBottom);
        label_4 = new QLabel(TokenDlg);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setGeometry(QRect(180, 30, 91, 16));
        layoutWidget = new QWidget(TokenDlg);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(260, 340, 481, 32));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        renameBtn = new QPushButton(layoutWidget);
        renameBtn->setObjectName(QString::fromUtf8("renameBtn"));
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(renameBtn->sizePolicy().hasHeightForWidth());
        renameBtn->setSizePolicy(sizePolicy);
        renameBtn->setMinimumSize(QSize(93, 0));
        renameBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(renameBtn);

        newBtn = new QPushButton(layoutWidget);
        newBtn->setObjectName(QString::fromUtf8("newBtn"));
        sizePolicy.setHeightForWidth(newBtn->sizePolicy().hasHeightForWidth());
        newBtn->setSizePolicy(sizePolicy);
        newBtn->setMinimumSize(QSize(93, 0));
        newBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(newBtn);

        deleteBtn = new QPushButton(layoutWidget);
        deleteBtn->setObjectName(QString::fromUtf8("deleteBtn"));
        sizePolicy.setHeightForWidth(deleteBtn->sizePolicy().hasHeightForWidth());
        deleteBtn->setSizePolicy(sizePolicy);
        deleteBtn->setMinimumSize(QSize(93, 0));
        deleteBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(deleteBtn);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        okBtn = new QPushButton(layoutWidget);
        okBtn->setObjectName(QString::fromUtf8("okBtn"));
        sizePolicy.setHeightForWidth(okBtn->sizePolicy().hasHeightForWidth());
        okBtn->setSizePolicy(sizePolicy);
        okBtn->setMinimumSize(QSize(125, 0));
        okBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(okBtn);

        resultText = new QTextBrowser(TokenDlg);
        resultText->setObjectName(QString::fromUtf8("resultText"));
        resultText->setGeometry(QRect(180, 240, 561, 61));
        resultText->setStyleSheet(QString::fromUtf8(""));

        retranslateUi(TokenDlg);

        QMetaObject::connectSlotsByName(TokenDlg);
    } // setupUi

    void retranslateUi(QDialog *TokenDlg)
    {
        TokenDlg->setWindowTitle(QApplication::translate("TokenDlg", "Token Editor", nullptr));
        label->setText(QApplication::translate("TokenDlg", "Tokens", nullptr));
        label_2->setText(QApplication::translate("TokenDlg", "Token string", nullptr));
        label_3->setText(QApplication::translate("TokenDlg", "Example result:", nullptr));
        label_4->setText(QApplication::translate("TokenDlg", "Templates", nullptr));
        renameBtn->setText(QApplication::translate("TokenDlg", "Rename", nullptr));
        newBtn->setText(QApplication::translate("TokenDlg", "New", nullptr));
        deleteBtn->setText(QApplication::translate("TokenDlg", "Delete", nullptr));
        okBtn->setText(QApplication::translate("TokenDlg", "Select", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TokenDlg: public Ui_TokenDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TOKENDLG_H
