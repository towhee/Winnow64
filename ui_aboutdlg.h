/********************************************************************************
** Form generated from reading UI file 'aboutdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ABOUTDLG_H
#define UI_ABOUTDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_About
{
public:
    QLabel *label;
    QLabel *version;
    QLabel *title;
    QLabel *webSite;
    QLabel *email;
    QLabel *Author;
    QLabel *license;
    QLabel *qtVersion;
    QPushButton *okButton;
    QLabel *comment;

    void setupUi(QDialog *About)
    {
        if (About->objectName().isEmpty())
            About->setObjectName(QString::fromUtf8("About"));
        About->resize(708, 430);
        label = new QLabel(About);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(50, 90, 100, 100));
        label->setPixmap(QPixmap(QString::fromUtf8(":/images/winnow.png")));
        label->setScaledContents(true);
        version = new QLabel(About);
        version->setObjectName(QString::fromUtf8("version"));
        version->setGeometry(QRect(250, 90, 341, 20));
        QFont font;
        font.setFamily(QString::fromUtf8("MS Shell Dlg 2"));
        font.setPointSize(11);
        version->setFont(font);
        title = new QLabel(About);
        title->setObjectName(QString::fromUtf8("title"));
        title->setGeometry(QRect(0, 0, 590, 50));
        title->setStyleSheet(QString::fromUtf8("font: 20pt \"Tahoma\";\n"
""));
        title->setAlignment(Qt::AlignCenter);
        webSite = new QLabel(About);
        webSite->setObjectName(QString::fromUtf8("webSite"));
        webSite->setGeometry(QRect(250, 120, 361, 20));
        webSite->setFont(font);
        webSite->setStyleSheet(QString::fromUtf8(""));
        email = new QLabel(About);
        email->setObjectName(QString::fromUtf8("email"));
        email->setGeometry(QRect(250, 170, 361, 20));
        email->setFont(font);
        Author = new QLabel(About);
        Author->setObjectName(QString::fromUtf8("Author"));
        Author->setGeometry(QRect(250, 200, 361, 20));
        Author->setFont(font);
        Author->setStyleSheet(QString::fromUtf8("font-size = 9;"));
        license = new QLabel(About);
        license->setObjectName(QString::fromUtf8("license"));
        license->setGeometry(QRect(250, 300, 321, 41));
        license->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        license->setWordWrap(true);
        qtVersion = new QLabel(About);
        qtVersion->setObjectName(QString::fromUtf8("qtVersion"));
        qtVersion->setGeometry(QRect(250, 270, 101, 21));
        qtVersion->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        qtVersion->setWordWrap(true);
        okButton = new QPushButton(About);
        okButton->setObjectName(QString::fromUtf8("okButton"));
        okButton->setGeometry(QRect(540, 380, 121, 28));
        comment = new QLabel(About);
        comment->setObjectName(QString::fromUtf8("comment"));
        comment->setGeometry(QRect(270, 140, 391, 21));
        QFont font1;
        font1.setPointSize(9);
        comment->setFont(font1);
        comment->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        comment->setWordWrap(true);

        retranslateUi(About);

        QMetaObject::connectSlotsByName(About);
    } // setupUi

    void retranslateUi(QDialog *About)
    {
        About->setWindowTitle(QApplication::translate("About", "About", nullptr));
        label->setText(QString());
        version->setText(QApplication::translate("About", "Version 0.9.7.5 updated 2019-02-03", nullptr));
        title->setText(QApplication::translate("About", "Image Viewer and Ingester", nullptr));
        webSite->setText(QApplication::translate("About", "<html><head/><body><p>Website:  <a href=\"http://165.227.46.158/winnow/winnow.html\"><span style=\" text-decoration: underline; color:#e5e5e5;\">Winnow main page</span></a>", nullptr));
        email->setText(QApplication::translate("About", "<html><head/><body><p>Email:  <a href=\"http://165.227.46.158/winnow/winnow.html\"><span style=\" text-decoration: underline; color:#e5e5e5;\">winnowimageviewer@outlook.com</span></a>", nullptr));
        Author->setText(QApplication::translate("About", "Author:  Rory Hill", nullptr));
        license->setText(QApplication::translate("About", "Winnow is licensed under the GNU General Public License version 3", nullptr));
        qtVersion->setText(QApplication::translate("About", "Qt 5.12", nullptr));
        okButton->setText(QApplication::translate("About", "OK", nullptr));
        comment->setText(QApplication::translate("About", "Includes links to download the application and video tutorials.", nullptr));
    } // retranslateUi

};

namespace Ui {
    class About: public Ui_About {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ABOUTDLG_H
