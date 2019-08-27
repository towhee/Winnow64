/********************************************************************************
** Form generated from reading UI file 'aboutdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ABOUTDLG_H
#define UI_ABOUTDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_About
{
public:
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout_3;
    QLabel *title;
    QHBoxLayout *horizontalLayout_4;
    QSpacerItem *horizontalSpacer_5;
    QVBoxLayout *verticalLayout_3;
    QSpacerItem *verticalSpacer_3;
    QLabel *label;
    QSpacerItem *verticalSpacer_5;
    QSpacerItem *horizontalSpacer_6;
    QVBoxLayout *verticalLayout_2;
    QLabel *version;
    QSpacerItem *verticalSpacer_2;
    QLabel *webSite;
    QSpacerItem *verticalSpacer_4;
    QLabel *Author;
    QLabel *email;
    QSpacerItem *verticalSpacer_7;
    QLabel *qtVersion;
    QLabel *license;
    QSpacerItem *horizontalSpacer_4;
    QFrame *frame;
    QHBoxLayout *horizontalLayout_5;
    QSpacerItem *horizontalSpacer_3;
    QPushButton *okButton;
    QSpacerItem *horizontalSpacer_2;

    void setupUi(QDialog *About)
    {
        if (About->objectName().isEmpty())
            About->setObjectName(QString::fromUtf8("About"));
        About->resize(811, 584);
        QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(About->sizePolicy().hasHeightForWidth());
        About->setSizePolicy(sizePolicy);
        QFont font;
        font.setPointSize(20);
        About->setFont(font);
        verticalLayout = new QVBoxLayout(About);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalLayout_3->setContentsMargins(-1, -1, -1, 20);
        title = new QLabel(About);
        title->setObjectName(QString::fromUtf8("title"));
        sizePolicy.setHeightForWidth(title->sizePolicy().hasHeightForWidth());
        title->setSizePolicy(sizePolicy);
        title->setMaximumSize(QSize(900, 16777215));
        QFont font1;
        font1.setFamily(QString::fromUtf8("Tahoma"));
        font1.setPointSize(30);
        font1.setBold(false);
        font1.setItalic(false);
        font1.setWeight(50);
        title->setFont(font1);
        title->setAlignment(Qt::AlignCenter);

        horizontalLayout_3->addWidget(title, 0, Qt::AlignHCenter|Qt::AlignVCenter);


        verticalLayout->addLayout(horizontalLayout_3);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(-1, -1, -1, 10);
        horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Maximum, QSizePolicy::Minimum);

        horizontalLayout_4->addItem(horizontalSpacer_5);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        verticalSpacer_3 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Maximum);

        verticalLayout_3->addItem(verticalSpacer_3);

        label = new QLabel(About);
        label->setObjectName(QString::fromUtf8("label"));
        QSizePolicy sizePolicy1(QSizePolicy::Maximum, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy1);
        label->setMinimumSize(QSize(100, 100));
        label->setMaximumSize(QSize(100, 100));
        label->setPixmap(QPixmap(QString::fromUtf8(":/images/winnow.png")));
        label->setScaledContents(true);

        verticalLayout_3->addWidget(label);

        verticalSpacer_5 = new QSpacerItem(10, 68, QSizePolicy::Minimum, QSizePolicy::Maximum);

        verticalLayout_3->addItem(verticalSpacer_5);


        horizontalLayout_4->addLayout(verticalLayout_3);

        horizontalSpacer_6 = new QSpacerItem(40, 20, QSizePolicy::Maximum, QSizePolicy::Minimum);

        horizontalLayout_4->addItem(horizontalSpacer_6);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        version = new QLabel(About);
        version->setObjectName(QString::fromUtf8("version"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Maximum);
        sizePolicy2.setHorizontalStretch(10);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(version->sizePolicy().hasHeightForWidth());
        version->setSizePolicy(sizePolicy2);
        QFont font2;
        font2.setFamily(QString::fromUtf8("MS Shell Dlg 2"));
        font2.setPointSize(20);
        version->setFont(font2);

        verticalLayout_2->addWidget(version);

        verticalSpacer_2 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout_2->addItem(verticalSpacer_2);

        webSite = new QLabel(About);
        webSite->setObjectName(QString::fromUtf8("webSite"));
        QSizePolicy sizePolicy3(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(webSite->sizePolicy().hasHeightForWidth());
        webSite->setSizePolicy(sizePolicy3);
        webSite->setFont(font2);
        webSite->setStyleSheet(QString::fromUtf8(""));
        webSite->setWordWrap(true);

        verticalLayout_2->addWidget(webSite);

        verticalSpacer_4 = new QSpacerItem(10, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout_2->addItem(verticalSpacer_4);

        Author = new QLabel(About);
        Author->setObjectName(QString::fromUtf8("Author"));
        QSizePolicy sizePolicy4(QSizePolicy::Expanding, QSizePolicy::Maximum);
        sizePolicy4.setHorizontalStretch(0);
        sizePolicy4.setVerticalStretch(0);
        sizePolicy4.setHeightForWidth(Author->sizePolicy().hasHeightForWidth());
        Author->setSizePolicy(sizePolicy4);
        Author->setFont(font2);

        verticalLayout_2->addWidget(Author);

        email = new QLabel(About);
        email->setObjectName(QString::fromUtf8("email"));
        sizePolicy4.setHeightForWidth(email->sizePolicy().hasHeightForWidth());
        email->setSizePolicy(sizePolicy4);
        email->setFont(font2);

        verticalLayout_2->addWidget(email);

        verticalSpacer_7 = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout_2->addItem(verticalSpacer_7);

        qtVersion = new QLabel(About);
        qtVersion->setObjectName(QString::fromUtf8("qtVersion"));
        sizePolicy4.setHeightForWidth(qtVersion->sizePolicy().hasHeightForWidth());
        qtVersion->setSizePolicy(sizePolicy4);
        qtVersion->setFont(font);
        qtVersion->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        qtVersion->setWordWrap(true);

        verticalLayout_2->addWidget(qtVersion);

        license = new QLabel(About);
        license->setObjectName(QString::fromUtf8("license"));
        sizePolicy4.setHeightForWidth(license->sizePolicy().hasHeightForWidth());
        license->setSizePolicy(sizePolicy4);
        license->setFont(font);
        license->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        license->setWordWrap(true);

        verticalLayout_2->addWidget(license);


        horizontalLayout_4->addLayout(verticalLayout_2);

        horizontalSpacer_4 = new QSpacerItem(40, 20, QSizePolicy::Maximum, QSizePolicy::Minimum);

        horizontalLayout_4->addItem(horizontalSpacer_4);


        verticalLayout->addLayout(horizontalLayout_4);

        frame = new QFrame(About);
        frame->setObjectName(QString::fromUtf8("frame"));
        QSizePolicy sizePolicy5(QSizePolicy::Maximum, QSizePolicy::Minimum);
        sizePolicy5.setHorizontalStretch(0);
        sizePolicy5.setVerticalStretch(0);
        sizePolicy5.setHeightForWidth(frame->sizePolicy().hasHeightForWidth());
        frame->setSizePolicy(sizePolicy5);
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);

        verticalLayout->addWidget(frame);

        horizontalLayout_5 = new QHBoxLayout();
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        horizontalLayout_5->setContentsMargins(-1, -1, -1, 10);
        horizontalSpacer_3 = new QSpacerItem(533, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_5->addItem(horizontalSpacer_3);

        okButton = new QPushButton(About);
        okButton->setObjectName(QString::fromUtf8("okButton"));
        QSizePolicy sizePolicy6(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy6.setHorizontalStretch(1);
        sizePolicy6.setVerticalStretch(0);
        sizePolicy6.setHeightForWidth(okButton->sizePolicy().hasHeightForWidth());
        okButton->setSizePolicy(sizePolicy6);
        okButton->setMinimumSize(QSize(100, 0));
        okButton->setMaximumSize(QSize(100, 16777215));
        okButton->setFont(font);

        horizontalLayout_5->addWidget(okButton);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_5->addItem(horizontalSpacer_2);


        verticalLayout->addLayout(horizontalLayout_5);


        retranslateUi(About);

        QMetaObject::connectSlotsByName(About);
    } // setupUi

    void retranslateUi(QDialog *About)
    {
        About->setWindowTitle(QCoreApplication::translate("About", "About", nullptr));
        title->setText(QCoreApplication::translate("About", "Winnow Image Viewer and Ingester", nullptr));
        label->setText(QString());
        version->setText(QCoreApplication::translate("About", "<html><head/><body><p>Version 1.0 updated 2019-08-25</p></body></html>", nullptr));
        webSite->setText(QCoreApplication::translate("About", "<html><head/><body><p>Website: <a href=\"http://165.227.46.158/winnow/winnow.html\"><span style=\" text-decoration: underline; color:#e5e5e5;\">Winnow main page</span></a>&nbsp;&nbsp;(Includes links to download the application and video tutorials)</p></body></html>", nullptr));
        Author->setText(QCoreApplication::translate("About", "Author:  Rory Hill", nullptr));
        email->setText(QCoreApplication::translate("About", "<html><head/><body><p>Email:  <a href=\"http://165.227.46.158/winnow/winnow.html\"><span style=\" text-decoration: underline; color:#e5e5e5;\">winnowimageviewer@outlook.com</span></a>", nullptr));
        qtVersion->setText(QCoreApplication::translate("About", "Qt 5.12", nullptr));
        license->setText(QCoreApplication::translate("About", "Winnow is licensed under the GNU General Public License version 3", nullptr));
        okButton->setText(QCoreApplication::translate("About", "OK", nullptr));
    } // retranslateUi

};

namespace Ui {
    class About: public Ui_About {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ABOUTDLG_H
