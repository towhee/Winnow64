/********************************************************************************
** Form generated from reading UI file 'tokendlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TOKENDLG_H
#define UI_TOKENDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextBrowser>
#include <QtWidgets/QVBoxLayout>
#include "tokendlg.h"

QT_BEGIN_NAMESPACE

class Ui_TokenDlg
{
public:
    QHBoxLayout *horizontalLayout_2;
    QFrame *frame_2;
    QVBoxLayout *verticalLayout_2;
    QLabel *label;
    TokenList *tokenList;
    QFrame *frame;
    QVBoxLayout *verticalLayout;
    QFrame *frame_4;
    QVBoxLayout *verticalLayout_4;
    QLabel *label_4;
    QComboBox *templatesCB;
    QFrame *frame_9;
    QLabel *label_2;
    QFrame *frame_3;
    QVBoxLayout *verticalLayout_6;
    TokenEdit *tokenEdit;
    QFrame *frame_5;
    QLabel *label_3;
    QFrame *frame_6;
    QVBoxLayout *verticalLayout_5;
    QTextBrowser *resultText;
    QLabel *label_5;
    QLabel *uniqueWarningLabel;
    QFrame *frame_8;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *renameBtn;
    QPushButton *newBtn;
    QPushButton *deleteBtn;
    QSpacerItem *horizontalSpacer;
    QPushButton *okBtn;

    void setupUi(QDialog *TokenDlg)
    {
        if (TokenDlg->objectName().isEmpty())
            TokenDlg->setObjectName(QString::fromUtf8("TokenDlg"));
        TokenDlg->resize(843, 555);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(TokenDlg->sizePolicy().hasHeightForWidth());
        TokenDlg->setSizePolicy(sizePolicy);
        TokenDlg->setMinimumSize(QSize(800, 475));
        TokenDlg->setMaximumSize(QSize(16777215, 16777215));
        TokenDlg->setStyleSheet(QString::fromUtf8(""));
        horizontalLayout_2 = new QHBoxLayout(TokenDlg);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        frame_2 = new QFrame(TokenDlg);
        frame_2->setObjectName(QString::fromUtf8("frame_2"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Minimum);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(frame_2->sizePolicy().hasHeightForWidth());
        frame_2->setSizePolicy(sizePolicy1);
        frame_2->setMinimumSize(QSize(200, 0));
        frame_2->setMaximumSize(QSize(175, 16777215));
        frame_2->setFrameShape(QFrame::StyledPanel);
        frame_2->setFrameShadow(QFrame::Raised);
        verticalLayout_2 = new QVBoxLayout(frame_2);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(11, 11, -1, -1);
        label = new QLabel(frame_2);
        label->setObjectName(QString::fromUtf8("label"));
        label->setMinimumSize(QSize(0, 0));
        label->setMaximumSize(QSize(16777215, 16));

        verticalLayout_2->addWidget(label);

        tokenList = new TokenList(frame_2);
        tokenList->setObjectName(QString::fromUtf8("tokenList"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Minimum);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(tokenList->sizePolicy().hasHeightForWidth());
        tokenList->setSizePolicy(sizePolicy2);
        tokenList->setMinimumSize(QSize(0, 0));
        tokenList->setMaximumSize(QSize(16777215, 16777215));

        verticalLayout_2->addWidget(tokenList);


        horizontalLayout_2->addWidget(frame_2);

        frame = new QFrame(TokenDlg);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(frame);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        frame_4 = new QFrame(frame);
        frame_4->setObjectName(QString::fromUtf8("frame_4"));
        sizePolicy.setHeightForWidth(frame_4->sizePolicy().hasHeightForWidth());
        frame_4->setSizePolicy(sizePolicy);
        frame_4->setMinimumSize(QSize(0, 65));
        frame_4->setMaximumSize(QSize(16777215, 75));
        frame_4->setFrameShape(QFrame::StyledPanel);
        frame_4->setFrameShadow(QFrame::Raised);
        verticalLayout_4 = new QVBoxLayout(frame_4);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        verticalLayout_4->setContentsMargins(0, -1, 0, 0);
        label_4 = new QLabel(frame_4);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setMinimumSize(QSize(0, 16));
        label_4->setMaximumSize(QSize(16777215, 16));

        verticalLayout_4->addWidget(label_4);

        templatesCB = new QComboBox(frame_4);
        templatesCB->setObjectName(QString::fromUtf8("templatesCB"));
        sizePolicy.setHeightForWidth(templatesCB->sizePolicy().hasHeightForWidth());
        templatesCB->setSizePolicy(sizePolicy);
        templatesCB->setEditable(false);
        templatesCB->setInsertPolicy(QComboBox::InsertAtBottom);

        verticalLayout_4->addWidget(templatesCB);


        verticalLayout->addWidget(frame_4);

        frame_9 = new QFrame(frame);
        frame_9->setObjectName(QString::fromUtf8("frame_9"));
        QSizePolicy sizePolicy3(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(frame_9->sizePolicy().hasHeightForWidth());
        frame_9->setSizePolicy(sizePolicy3);
        frame_9->setMinimumSize(QSize(0, 20));
        frame_9->setMaximumSize(QSize(16777215, 20));
        frame_9->setFrameShape(QFrame::StyledPanel);
        frame_9->setFrameShadow(QFrame::Raised);
        label_2 = new QLabel(frame_9);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(0, 0, 200, 20));
        label_2->setMinimumSize(QSize(0, 0));
        label_2->setMaximumSize(QSize(16777215, 16777215));

        verticalLayout->addWidget(frame_9);

        frame_3 = new QFrame(frame);
        frame_3->setObjectName(QString::fromUtf8("frame_3"));
        QSizePolicy sizePolicy4(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(frame_3->sizePolicy().hasHeightForWidth());
        frame_3->setSizePolicy(sizePolicy4);
        frame_3->setMinimumSize(QSize(0, 50));
        frame_3->setMaximumSize(QSize(16777215, 16777215));
        frame_3->setFrameShape(QFrame::StyledPanel);
        frame_3->setFrameShadow(QFrame::Raised);
        verticalLayout_6 = new QVBoxLayout(frame_3);
        verticalLayout_6->setObjectName(QString::fromUtf8("verticalLayout_6"));
        verticalLayout_6->setContentsMargins(0, 0, 0, 0);
        tokenEdit = new TokenEdit(frame_3);
        tokenEdit->setObjectName(QString::fromUtf8("tokenEdit"));
        sizePolicy4.setHeightForWidth(tokenEdit->sizePolicy().hasHeightForWidth());
        tokenEdit->setSizePolicy(sizePolicy4);
        tokenEdit->setMinimumSize(QSize(0, 50));
        tokenEdit->setMaximumSize(QSize(16777215, 16777215));
        QFont font;
        font.setPointSize(16);
        tokenEdit->setFont(font);
        tokenEdit->setStyleSheet(QString::fromUtf8(""));

        verticalLayout_6->addWidget(tokenEdit);


        verticalLayout->addWidget(frame_3);

        frame_5 = new QFrame(frame);
        frame_5->setObjectName(QString::fromUtf8("frame_5"));
        sizePolicy3.setHeightForWidth(frame_5->sizePolicy().hasHeightForWidth());
        frame_5->setSizePolicy(sizePolicy3);
        frame_5->setMinimumSize(QSize(0, 20));
        frame_5->setMaximumSize(QSize(16777215, 20));
        frame_5->setFrameShape(QFrame::StyledPanel);
        frame_5->setFrameShadow(QFrame::Raised);
        label_3 = new QLabel(frame_5);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(0, 0, 141, 20));

        verticalLayout->addWidget(frame_5);

        frame_6 = new QFrame(frame);
        frame_6->setObjectName(QString::fromUtf8("frame_6"));
        sizePolicy4.setHeightForWidth(frame_6->sizePolicy().hasHeightForWidth());
        frame_6->setSizePolicy(sizePolicy4);
        frame_6->setMinimumSize(QSize(0, 50));
        frame_6->setMaximumSize(QSize(16777215, 16777215));
        frame_6->setFrameShape(QFrame::StyledPanel);
        frame_6->setFrameShadow(QFrame::Raised);
        verticalLayout_5 = new QVBoxLayout(frame_6);
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        verticalLayout_5->setContentsMargins(0, 0, 0, 0);
        resultText = new QTextBrowser(frame_6);
        resultText->setObjectName(QString::fromUtf8("resultText"));
        sizePolicy4.setHeightForWidth(resultText->sizePolicy().hasHeightForWidth());
        resultText->setSizePolicy(sizePolicy4);
        resultText->setMinimumSize(QSize(0, 50));
        resultText->setMaximumSize(QSize(16777215, 16777215));
        resultText->setStyleSheet(QString::fromUtf8(""));

        verticalLayout_5->addWidget(resultText);


        verticalLayout->addWidget(frame_6);

        label_5 = new QLabel(frame);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        QFont font1;
        font1.setPointSize(13);
        label_5->setFont(font1);
        label_5->setWordWrap(true);

        verticalLayout->addWidget(label_5);

        uniqueWarningLabel = new QLabel(frame);
        uniqueWarningLabel->setObjectName(QString::fromUtf8("uniqueWarningLabel"));
        sizePolicy.setHeightForWidth(uniqueWarningLabel->sizePolicy().hasHeightForWidth());
        uniqueWarningLabel->setSizePolicy(sizePolicy);
        uniqueWarningLabel->setFont(font1);
        uniqueWarningLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        uniqueWarningLabel->setWordWrap(true);

        verticalLayout->addWidget(uniqueWarningLabel);

        frame_8 = new QFrame(frame);
        frame_8->setObjectName(QString::fromUtf8("frame_8"));
        sizePolicy3.setHeightForWidth(frame_8->sizePolicy().hasHeightForWidth());
        frame_8->setSizePolicy(sizePolicy3);
        frame_8->setMinimumSize(QSize(0, 40));
        frame_8->setMaximumSize(QSize(16777215, 40));
        frame_8->setFrameShape(QFrame::StyledPanel);
        frame_8->setFrameShadow(QFrame::Raised);
        horizontalLayout = new QHBoxLayout(frame_8);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 20, 0);
        horizontalSpacer_2 = new QSpacerItem(67, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        renameBtn = new QPushButton(frame_8);
        renameBtn->setObjectName(QString::fromUtf8("renameBtn"));
        QSizePolicy sizePolicy5(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(renameBtn->sizePolicy().hasHeightForWidth());
        renameBtn->setSizePolicy(sizePolicy5);
        renameBtn->setMinimumSize(QSize(93, 0));
        renameBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(renameBtn);

        newBtn = new QPushButton(frame_8);
        newBtn->setObjectName(QString::fromUtf8("newBtn"));
        sizePolicy5.setHeightForWidth(newBtn->sizePolicy().hasHeightForWidth());
        newBtn->setSizePolicy(sizePolicy5);
        newBtn->setMinimumSize(QSize(93, 0));
        newBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(newBtn);

        deleteBtn = new QPushButton(frame_8);
        deleteBtn->setObjectName(QString::fromUtf8("deleteBtn"));
        sizePolicy5.setHeightForWidth(deleteBtn->sizePolicy().hasHeightForWidth());
        deleteBtn->setSizePolicy(sizePolicy5);
        deleteBtn->setMinimumSize(QSize(93, 0));
        deleteBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(deleteBtn);

        horizontalSpacer = new QSpacerItem(50, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        okBtn = new QPushButton(frame_8);
        okBtn->setObjectName(QString::fromUtf8("okBtn"));
        sizePolicy5.setHeightForWidth(okBtn->sizePolicy().hasHeightForWidth());
        okBtn->setSizePolicy(sizePolicy5);
        okBtn->setMinimumSize(QSize(125, 0));
        okBtn->setFocusPolicy(Qt::NoFocus);

        horizontalLayout->addWidget(okBtn);


        verticalLayout->addWidget(frame_8);


        horizontalLayout_2->addWidget(frame);


        retranslateUi(TokenDlg);

        QMetaObject::connectSlotsByName(TokenDlg);
    } // setupUi

    void retranslateUi(QDialog *TokenDlg)
    {
        TokenDlg->setWindowTitle(QCoreApplication::translate("TokenDlg", "Token Editor", nullptr));
        label->setText(QCoreApplication::translate("TokenDlg", "Tokens", nullptr));
        label_4->setText(QCoreApplication::translate("TokenDlg", "Templates", nullptr));
        label_2->setText(QCoreApplication::translate("TokenDlg", "Token string", nullptr));
        label_3->setText(QCoreApplication::translate("TokenDlg", "Example result:", nullptr));
        label_5->setText(QCoreApplication::translate("TokenDlg", "Edits to the token string are automatically saved to the currently selected template.\n"
"", nullptr));
        uniqueWarningLabel->setText(QCoreApplication::translate("TokenDlg", "<html><head/><body><p><span style=\" font-weight:600; color:#ffff00;\">Warning:</span> The tokenized file name is unlikely to produce unique file names. Consider including the ORIGINAL FILENAME, time MINUTE and SECOND or a sequence XXXX</p></body></html>", nullptr));
        renameBtn->setText(QCoreApplication::translate("TokenDlg", "Rename", nullptr));
        newBtn->setText(QCoreApplication::translate("TokenDlg", "New", nullptr));
        deleteBtn->setText(QCoreApplication::translate("TokenDlg", "Delete", nullptr));
        okBtn->setText(QCoreApplication::translate("TokenDlg", "Select", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TokenDlg: public Ui_TokenDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TOKENDLG_H
