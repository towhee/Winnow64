/********************************************************************************
** Form generated from reading UI file 'copypickdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_COPYPICKDLG_H
#define UI_COPYPICKDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_IngestDlg
{
public:
    QGroupBox *groupBox;
    QLabel *folderPathLabel;
    QLabel *folderPathLabel_2;
    QLabel *folderPathLabel_3;
    QLabel *folderPathLabel_4;
    QProgressBar *progressBar;
    QGroupBox *groupBox_2;
    QPushButton *selectFolderBtn;
    QLabel *destinationFolderLabel;
    QRadioButton *autoRadio;
    QLineEdit *descriptionLineEdit;
    QRadioButton *manualRadio;
    QLabel *manualFolderLabel;
    QPushButton *selectRootFolderBtn;
    QLabel *rootFolderLabel;
    QLabel *folderLabel;
    QLabel *folderDescription;
    QFrame *line;
    QLabel *templateLabel1;
    QComboBox *pathTemplatesCB;
    QPushButton *pathTemplatesBtn;
    QPushButton *helpBtn;
    QGroupBox *groupBox_4;
    QLabel *existingSequenceLabel;
    QLabel *label_3;
    QSpinBox *spinBoxStartNumber;
    QLabel *label_5;
    QComboBox *filenameTemplatesCB;
    QPushButton *filenameTemplatesBtn;
    QLabel *statsLabel;
    QWidget *layoutWidget;
    QHBoxLayout *horizontalLayout;
    QPushButton *cancelBtn;
    QPushButton *okBtn;
    QCheckBox *combinedIncludeJpgChk;
    QCheckBox *ejectChk;

    void setupUi(QDialog *IngestDlg)
    {
        if (IngestDlg->objectName().isEmpty())
            IngestDlg->setObjectName(QStringLiteral("IngestDlg"));
        IngestDlg->resize(704, 797);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(IngestDlg->sizePolicy().hasHeightForWidth());
        IngestDlg->setSizePolicy(sizePolicy);
        IngestDlg->setModal(false);
        groupBox = new QGroupBox(IngestDlg);
        groupBox->setObjectName(QStringLiteral("groupBox"));
        groupBox->setGeometry(QRect(20, 520, 661, 111));
        groupBox->setMinimumSize(QSize(381, 0));
        QFont font;
        font.setPointSize(10);
        groupBox->setFont(font);
        folderPathLabel = new QLabel(groupBox);
        folderPathLabel->setObjectName(QStringLiteral("folderPathLabel"));
        folderPathLabel->setGeometry(QRect(20, 20, 640, 20));
        QPalette palette;
        QBrush brush(QColor(180, 180, 120, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Active, QPalette::Text, brush);
        palette.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        folderPathLabel->setPalette(palette);
        folderPathLabel->setFont(font);
        folderPathLabel->setStyleSheet(QStringLiteral("color:rgb(180,180,120)"));
        folderPathLabel->setFrameShape(QFrame::Box);
        folderPathLabel_2 = new QLabel(groupBox);
        folderPathLabel_2->setObjectName(QStringLiteral("folderPathLabel_2"));
        folderPathLabel_2->setGeometry(QRect(20, 40, 640, 20));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Text, brush);
        palette1.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        folderPathLabel_2->setPalette(palette1);
        folderPathLabel_2->setFont(font);
        folderPathLabel_2->setStyleSheet(QStringLiteral("color:rgb(180,180,120)"));
        folderPathLabel_2->setFrameShape(QFrame::Box);
        folderPathLabel_3 = new QLabel(groupBox);
        folderPathLabel_3->setObjectName(QStringLiteral("folderPathLabel_3"));
        folderPathLabel_3->setGeometry(QRect(20, 60, 640, 20));
        QPalette palette2;
        QBrush brush1(QColor(0, 0, 0, 255));
        brush1.setStyle(Qt::SolidPattern);
        palette2.setBrush(QPalette::Active, QPalette::Text, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Text, brush1);
        QBrush brush2(QColor(120, 120, 120, 255));
        brush2.setStyle(Qt::SolidPattern);
        palette2.setBrush(QPalette::Disabled, QPalette::Text, brush2);
        folderPathLabel_3->setPalette(palette2);
        folderPathLabel_3->setFont(font);
        folderPathLabel_3->setFrameShape(QFrame::Box);
        folderPathLabel_4 = new QLabel(groupBox);
        folderPathLabel_4->setObjectName(QStringLiteral("folderPathLabel_4"));
        folderPathLabel_4->setGeometry(QRect(20, 80, 640, 20));
        QPalette palette3;
        palette3.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Active, QPalette::Text, brush);
        palette3.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette3.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette3.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        folderPathLabel_4->setPalette(palette3);
        folderPathLabel_4->setFont(font);
        folderPathLabel_4->setStyleSheet(QStringLiteral("color:rgb(180,180,120)"));
        folderPathLabel_4->setFrameShape(QFrame::Box);
        progressBar = new QProgressBar(IngestDlg);
        progressBar->setObjectName(QStringLiteral("progressBar"));
        progressBar->setGeometry(QRect(20, 710, 661, 20));
        progressBar->setFont(font);
        progressBar->setValue(0);
        progressBar->setAlignment(Qt::AlignCenter);
        groupBox_2 = new QGroupBox(IngestDlg);
        groupBox_2->setObjectName(QStringLiteral("groupBox_2"));
        groupBox_2->setGeometry(QRect(20, 20, 660, 341));
        groupBox_2->setFont(font);
        selectFolderBtn = new QPushButton(groupBox_2);
        selectFolderBtn->setObjectName(QStringLiteral("selectFolderBtn"));
        selectFolderBtn->setEnabled(true);
        selectFolderBtn->setGeometry(QRect(65, 286, 101, 25));
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(false);
        font1.setWeight(50);
        selectFolderBtn->setFont(font1);
        selectFolderBtn->setAutoDefault(false);
        selectFolderBtn->setFlat(false);
        destinationFolderLabel = new QLabel(groupBox_2);
        destinationFolderLabel->setObjectName(QStringLiteral("destinationFolderLabel"));
        destinationFolderLabel->setGeometry(QRect(25, 170, 140, 20));
        destinationFolderLabel->setFont(font);
        destinationFolderLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        autoRadio = new QRadioButton(groupBox_2);
        autoRadio->setObjectName(QStringLiteral("autoRadio"));
        autoRadio->setGeometry(QRect(20, 30, 471, 17));
        autoRadio->setFont(font1);
        descriptionLineEdit = new QLineEdit(groupBox_2);
        descriptionLineEdit->setObjectName(QStringLiteral("descriptionLineEdit"));
        descriptionLineEdit->setGeometry(QRect(187, 140, 271, 20));
        descriptionLineEdit->setFont(font);
        manualRadio = new QRadioButton(groupBox_2);
        manualRadio->setObjectName(QStringLiteral("manualRadio"));
        manualRadio->setGeometry(QRect(20, 250, 511, 17));
        manualRadio->setFont(font1);
        manualFolderLabel = new QLabel(groupBox_2);
        manualFolderLabel->setObjectName(QStringLiteral("manualFolderLabel"));
        manualFolderLabel->setGeometry(QRect(190, 290, 460, 20));
        manualFolderLabel->setFont(font);
        manualFolderLabel->setStyleSheet(QStringLiteral(""));
        manualFolderLabel->setFrameShape(QFrame::Box);
        selectRootFolderBtn = new QPushButton(groupBox_2);
        selectRootFolderBtn->setObjectName(QStringLiteral("selectRootFolderBtn"));
        selectRootFolderBtn->setGeometry(QRect(65, 66, 101, 25));
        selectRootFolderBtn->setFont(font1);
        selectRootFolderBtn->setAutoDefault(false);
        selectRootFolderBtn->setFlat(false);
        rootFolderLabel = new QLabel(groupBox_2);
        rootFolderLabel->setObjectName(QStringLiteral("rootFolderLabel"));
        rootFolderLabel->setGeometry(QRect(190, 70, 460, 20));
        rootFolderLabel->setFont(font);
        rootFolderLabel->setFrameShape(QFrame::Box);
        folderLabel = new QLabel(groupBox_2);
        folderLabel->setObjectName(QStringLiteral("folderLabel"));
        folderLabel->setGeometry(QRect(190, 170, 460, 20));
        folderLabel->setFont(font);
        folderLabel->setStyleSheet(QStringLiteral(""));
        folderLabel->setFrameShape(QFrame::Box);
        folderDescription = new QLabel(groupBox_2);
        folderDescription->setObjectName(QStringLiteral("folderDescription"));
        folderDescription->setGeometry(QRect(25, 141, 140, 20));
        folderDescription->setFont(font);
        folderDescription->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        line = new QFrame(groupBox_2);
        line->setObjectName(QStringLiteral("line"));
        line->setGeometry(QRect(40, 220, 610, 1));
        line->setStyleSheet(QStringLiteral("background:rgb(150,150,150)"));
        line->setFrameShadow(QFrame::Plain);
        line->setFrameShape(QFrame::HLine);
        templateLabel1 = new QLabel(groupBox_2);
        templateLabel1->setObjectName(QStringLiteral("templateLabel1"));
        templateLabel1->setGeometry(QRect(20, 100, 140, 20));
        templateLabel1->setFont(font);
        templateLabel1->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        pathTemplatesCB = new QComboBox(groupBox_2);
        pathTemplatesCB->setObjectName(QStringLiteral("pathTemplatesCB"));
        pathTemplatesCB->setGeometry(QRect(190, 100, 321, 26));
        pathTemplatesBtn = new QPushButton(groupBox_2);
        pathTemplatesBtn->setObjectName(QStringLiteral("pathTemplatesBtn"));
        pathTemplatesBtn->setGeometry(QRect(530, 100, 101, 25));
        pathTemplatesBtn->setFont(font1);
        pathTemplatesBtn->setToolTipDuration(-3);
        pathTemplatesBtn->setAutoDefault(false);
        pathTemplatesBtn->setFlat(false);
        helpBtn = new QPushButton(groupBox_2);
        helpBtn->setObjectName(QStringLiteral("helpBtn"));
        helpBtn->setGeometry(QRect(530, 30, 101, 25));
        helpBtn->setAutoDefault(false);
        groupBox_4 = new QGroupBox(IngestDlg);
        groupBox_4->setObjectName(QStringLiteral("groupBox_4"));
        groupBox_4->setGeometry(QRect(20, 380, 660, 121));
        groupBox_4->setFont(font);
        existingSequenceLabel = new QLabel(groupBox_4);
        existingSequenceLabel->setObjectName(QStringLiteral("existingSequenceLabel"));
        existingSequenceLabel->setGeometry(QRect(250, 80, 391, 20));
        existingSequenceLabel->setStyleSheet(QStringLiteral(""));
        existingSequenceLabel->setFrameShape(QFrame::Box);
        label_3 = new QLabel(groupBox_4);
        label_3->setObjectName(QStringLiteral("label_3"));
        label_3->setGeometry(QRect(60, 81, 121, 16));
        label_3->setFont(font);
        spinBoxStartNumber = new QSpinBox(groupBox_4);
        spinBoxStartNumber->setObjectName(QStringLiteral("spinBoxStartNumber"));
        spinBoxStartNumber->setGeometry(QRect(190, 78, 51, 22));
        spinBoxStartNumber->setFont(font);
        spinBoxStartNumber->setMinimum(1);
        spinBoxStartNumber->setMaximum(9999);
        label_5 = new QLabel(groupBox_4);
        label_5->setObjectName(QStringLiteral("label_5"));
        label_5->setGeometry(QRect(20, 40, 140, 20));
        label_5->setFont(font);
        label_5->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        filenameTemplatesCB = new QComboBox(groupBox_4);
        filenameTemplatesCB->setObjectName(QStringLiteral("filenameTemplatesCB"));
        filenameTemplatesCB->setGeometry(QRect(190, 40, 321, 26));
        filenameTemplatesBtn = new QPushButton(groupBox_4);
        filenameTemplatesBtn->setObjectName(QStringLiteral("filenameTemplatesBtn"));
        filenameTemplatesBtn->setGeometry(QRect(530, 40, 101, 25));
        filenameTemplatesBtn->setFont(font1);
        filenameTemplatesBtn->setAutoDefault(false);
        filenameTemplatesBtn->setFlat(false);
        statsLabel = new QLabel(IngestDlg);
        statsLabel->setObjectName(QStringLiteral("statsLabel"));
        statsLabel->setGeometry(QRect(20, 750, 241, 16));
        statsLabel->setFont(font);
        layoutWidget = new QWidget(IngestDlg);
        layoutWidget->setObjectName(QStringLiteral("layoutWidget"));
        layoutWidget->setGeometry(QRect(430, 740, 251, 32));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QStringLiteral("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        cancelBtn = new QPushButton(layoutWidget);
        cancelBtn->setObjectName(QStringLiteral("cancelBtn"));
        cancelBtn->setMinimumSize(QSize(93, 0));
        cancelBtn->setAutoDefault(false);

        horizontalLayout->addWidget(cancelBtn);

        okBtn = new QPushButton(layoutWidget);
        okBtn->setObjectName(QStringLiteral("okBtn"));
        okBtn->setMinimumSize(QSize(93, 0));
        okBtn->setAutoDefault(false);

        horizontalLayout->addWidget(okBtn);

        combinedIncludeJpgChk = new QCheckBox(IngestDlg);
        combinedIncludeJpgChk->setObjectName(QStringLiteral("combinedIncludeJpgChk"));
        combinedIncludeJpgChk->setGeometry(QRect(20, 650, 661, 20));
        combinedIncludeJpgChk->setFont(font);
        ejectChk = new QCheckBox(IngestDlg);
        ejectChk->setObjectName(QStringLiteral("ejectChk"));
        ejectChk->setGeometry(QRect(20, 680, 661, 20));
        ejectChk->setFont(font);

        retranslateUi(IngestDlg);

        selectFolderBtn->setDefault(false);
        selectRootFolderBtn->setDefault(false);
        pathTemplatesBtn->setDefault(false);
        filenameTemplatesBtn->setDefault(false);


        QMetaObject::connectSlotsByName(IngestDlg);
    } // setupUi

    void retranslateUi(QDialog *IngestDlg)
    {
        IngestDlg->setWindowTitle(QApplication::translate("IngestDlg", "Copy picked images to destination folder", nullptr));
#ifndef QT_NO_TOOLTIP
        groupBox->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        groupBox->setTitle(QApplication::translate("IngestDlg", "Images will be copied and renamed to", nullptr));
#ifndef QT_NO_TOOLTIP
        folderPathLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderPathLabel->setText(QString());
#ifndef QT_NO_TOOLTIP
        folderPathLabel_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderPathLabel_2->setText(QString());
#ifndef QT_NO_TOOLTIP
        folderPathLabel_3->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderPathLabel_3->setText(QApplication::translate("IngestDlg", "...", nullptr));
#ifndef QT_NO_TOOLTIP
        folderPathLabel_4->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderPathLabel_4->setText(QString());
#ifndef QT_NO_TOOLTIP
        progressBar->setToolTip(QString());
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        groupBox_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        groupBox_2->setTitle(QApplication::translate("IngestDlg", "Select destination folder", nullptr));
#ifndef QT_NO_TOOLTIP
        selectFolderBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        selectFolderBtn->setText(QApplication::translate("IngestDlg", " Folder", nullptr));
#ifndef QT_NO_TOOLTIP
        destinationFolderLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        destinationFolderLabel->setText(QApplication::translate("IngestDlg", "Destination folder:", nullptr));
#ifndef QT_NO_TOOLTIP
        autoRadio->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        autoRadio->setText(QApplication::translate("IngestDlg", "Automatically generate destination folder from root folder", nullptr));
#ifndef QT_NO_TOOLTIP
        descriptionLineEdit->setToolTip(QString());
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        manualRadio->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        manualRadio->setText(QApplication::translate("IngestDlg", "Manually select destination folder", nullptr));
#ifndef QT_NO_TOOLTIP
        manualFolderLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        manualFolderLabel->setText(QString());
#ifndef QT_NO_TOOLTIP
        selectRootFolderBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        selectRootFolderBtn->setText(QApplication::translate("IngestDlg", "Root Folder", nullptr));
#ifndef QT_NO_TOOLTIP
        rootFolderLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        rootFolderLabel->setText(QString());
#ifndef QT_NO_TOOLTIP
        folderLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderLabel->setText(QString());
#ifndef QT_NO_TOOLTIP
        folderDescription->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderDescription->setText(QApplication::translate("IngestDlg", "Append Description:", nullptr));
#ifndef QT_NO_TOOLTIP
        line->setToolTip(QString());
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        templateLabel1->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        templateLabel1->setText(QApplication::translate("IngestDlg", "Template:", nullptr));
#ifndef QT_NO_TOOLTIP
        pathTemplatesCB->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        pathTemplatesCB->setCurrentText(QString());
#ifndef QT_NO_TOOLTIP
        pathTemplatesBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        pathTemplatesBtn->setText(QApplication::translate("IngestDlg", "Edit template(s)", nullptr));
#ifndef QT_NO_TOOLTIP
        helpBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        helpBtn->setText(QApplication::translate("IngestDlg", "How this works", nullptr));
#ifndef QT_NO_TOOLTIP
        groupBox_4->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        groupBox_4->setTitle(QApplication::translate("IngestDlg", "File name and sequencing", nullptr));
#ifndef QT_NO_TOOLTIP
        existingSequenceLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        existingSequenceLabel->setText(QString());
#ifndef QT_NO_TOOLTIP
        label_3->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        label_3->setText(QApplication::translate("IngestDlg", "Start sequence from:", nullptr));
#ifndef QT_NO_TOOLTIP
        spinBoxStartNumber->setToolTip(QString());
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        label_5->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        label_5->setText(QApplication::translate("IngestDlg", "Template:", nullptr));
#ifndef QT_NO_TOOLTIP
        filenameTemplatesCB->setToolTip(QString());
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        filenameTemplatesBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        filenameTemplatesBtn->setText(QApplication::translate("IngestDlg", "Edit template(s)", nullptr));
#ifndef QT_NO_TOOLTIP
        statsLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        statsLabel->setText(QApplication::translate("IngestDlg", "632 files using 830MB", nullptr));
#ifndef QT_NO_TOOLTIP
        layoutWidget->setToolTip(QString());
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        cancelBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        cancelBtn->setText(QApplication::translate("IngestDlg", "Cancel", nullptr));
#ifndef QT_NO_TOOLTIP
        okBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        okBtn->setText(QApplication::translate("IngestDlg", "Ingest", nullptr));
        combinedIncludeJpgChk->setText(QApplication::translate("IngestDlg", "If Raw + JPG are combined also copy the JPG files", nullptr));
        ejectChk->setText(QApplication::translate("IngestDlg", "Eject drive after ingesting", nullptr));
    } // retranslateUi

};

namespace Ui {
    class IngestDlg: public Ui_IngestDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_COPYPICKDLG_H
