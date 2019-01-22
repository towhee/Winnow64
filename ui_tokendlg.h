/********************************************************************************
** Form generated from reading UI file 'tokendlg.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TOKENDLG_H
#define UI_TOKENDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
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
    QLabel *label_5;
    QLabel *uniqueWarningLabel;

    void setupUi(QDialog *TokenDlg)
    {
        if (TokenDlg->objectName().isEmpty())
            TokenDlg->setObjectName(QStringLiteral("TokenDlg"));
        TokenDlg->resize(766, 452);
        TokenDlg->setStyleSheet(QStringLiteral(""));
        tokenList = new TokenList(TokenDlg);
        tokenList->setObjectName(QStringLiteral("tokenList"));
        tokenList->setGeometry(QRect(30, 50, 141, 381));
        tokenEdit = new TokenEdit(TokenDlg);
        tokenEdit->setObjectName(QStringLiteral("tokenEdit"));
        tokenEdit->setGeometry(QRect(180, 130, 561, 61));
        QFont font;
        font.setPointSize(16);
        tokenEdit->setFont(font);
        tokenEdit->setStyleSheet(QStringLiteral(""));
        label = new QLabel(TokenDlg);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(30, 30, 91, 16));
        label_2 = new QLabel(TokenDlg);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setGeometry(QRect(180, 110, 91, 16));
        label_3 = new QLabel(TokenDlg);
        label_3->setObjectName(QStringLiteral("label_3"));
        label_3->setGeometry(QRect(180, 220, 141, 16));
        templatesCB = new QComboBox(TokenDlg);
        templatesCB->setObjectName(QStringLiteral("templatesCB"));
        templatesCB->setGeometry(QRect(180, 50, 561, 26));
        templatesCB->setEditable(false);
        templatesCB->setInsertPolicy(QComboBox::InsertAtBottom);
        label_4 = new QLabel(TokenDlg);
        label_4->setObjectName(QStringLiteral("label_4"));
        label_4->setGeometry(QRect(180, 30, 91, 16));
        layoutWidget = new QWidget(TokenDlg);
        layoutWidget->setObjectName(QStringLiteral("layoutWidget"));
        layoutWidget->setGeometry(QRect(210, 400, 521, 32));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalLayout->setSizeConstraint(QLayout::SetDefaultConstraint);
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        renameBtn = new QPushButton(layoutWidget);
        renameBtn->setObjectName(QStringLiteral("renameBtn"));
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(renameBtn->sizePolicy().hasHeightForWidth());
        renameBtn->setSizePolicy(sizePolicy);
        renameBtn->setMinimumSize(QSize(93, 0));
        renameBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(renameBtn);

        newBtn = new QPushButton(layoutWidget);
        newBtn->setObjectName(QStringLiteral("newBtn"));
        sizePolicy.setHeightForWidth(newBtn->sizePolicy().hasHeightForWidth());
        newBtn->setSizePolicy(sizePolicy);
        newBtn->setMinimumSize(QSize(93, 0));
        newBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(newBtn);

        deleteBtn = new QPushButton(layoutWidget);
        deleteBtn->setObjectName(QStringLiteral("deleteBtn"));
        sizePolicy.setHeightForWidth(deleteBtn->sizePolicy().hasHeightForWidth());
        deleteBtn->setSizePolicy(sizePolicy);
        deleteBtn->setMinimumSize(QSize(93, 0));
        deleteBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(deleteBtn);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        okBtn = new QPushButton(layoutWidget);
        okBtn->setObjectName(QStringLiteral("okBtn"));
        sizePolicy.setHeightForWidth(okBtn->sizePolicy().hasHeightForWidth());
        okBtn->setSizePolicy(sizePolicy);
        okBtn->setMinimumSize(QSize(125, 0));
        okBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(okBtn);

        resultText = new QTextBrowser(TokenDlg);
        resultText->setObjectName(QStringLiteral("resultText"));
        resultText->setGeometry(QRect(180, 240, 561, 61));
        resultText->setStyleSheet(QStringLiteral(""));
        label_5 = new QLabel(TokenDlg);
        label_5->setObjectName(QStringLiteral("label_5"));
        label_5->setGeometry(QRect(180, 320, 551, 21));
        label_5->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        uniqueWarningLabel = new QLabel(TokenDlg);
        uniqueWarningLabel->setObjectName(QStringLiteral("uniqueWarningLabel"));
        uniqueWarningLabel->setGeometry(QRect(180, 350, 551, 41));
        uniqueWarningLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        uniqueWarningLabel->setWordWrap(true);

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
        label_5->setText(QApplication::translate("TokenDlg", "<html><head/><body><p>Edits to the token string are automatically saved to the currently selected template.</p></body></html>", nullptr));
        uniqueWarningLabel->setText(QApplication::translate("TokenDlg", "<html><head/><body><p><span style=\" font-weight:600; color:#ffff00;\">Warning:</span> The tokenized file name is unlikely to produce unique file names. Consider including the ORIGINAL FILENAME, time MINUTE and SECOND or a sequence XX... </p></body></html>", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TokenDlg: public Ui_TokenDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TOKENDLG_H
