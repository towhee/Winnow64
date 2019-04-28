/********************************************************************************
** Form generated from reading UI file 'ingestdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.12.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_INGESTDLG_H
#define UI_INGESTDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_IngestDlg
{
public:
    QGroupBox *seqGroupBox;
    QLabel *folderPathLabel;
    QLabel *folderPathLabel_2;
    QLabel *folderPathLabel_3;
    QLabel *folderPathLabel_4;
    QProgressBar *progressBar;
    QGroupBox *groupBox_2;
    QPushButton *selectFolderBtn;
    QRadioButton *autoRadio;
    QRadioButton *manualRadio;
    QLabel *manualFolderLabel;
    QTabWidget *autoIngestTab;
    QWidget *primaryTab;
    QLabel *destinationFolderLabel;
    QLabel *folderLabel;
    QLabel *templateLabel1;
    QPushButton *selectRootFolderBtn;
    QComboBox *pathTemplatesCB;
    QLineEdit *descriptionLineEdit;
    QLabel *folderDescription;
    QLabel *rootFolderLabel;
    QPushButton *pathTemplatesBtn;
    QWidget *backupTab;
    QLabel *folderLabel_2;
    QLabel *templateLabel1_2;
    QPushButton *selectRootFolderBtn_2;
    QComboBox *pathTemplatesCB_2;
    QLineEdit *descriptionLineEdit_2;
    QLabel *folderDescription_2;
    QLabel *destinationFolderLabel_2;
    QLabel *rootFolderLabel_2;
    QPushButton *pathTemplatesBtn_2;
    QPushButton *helpBtn;
    QPushButton *selectFolderBtn_2;
    QLabel *manualFolderLabel_2;
    QGroupBox *groupBox_4;
    QLabel *existingSequenceLabel;
    QLabel *startSeqLabel;
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
    QCheckBox *openIngestFolderChk;
    QCheckBox *backupChk;

    void setupUi(QDialog *IngestDlg)
    {
        if (IngestDlg->objectName().isEmpty())
            IngestDlg->setObjectName(QString::fromUtf8("IngestDlg"));
        IngestDlg->resize(732, 783);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(IngestDlg->sizePolicy().hasHeightForWidth());
        IngestDlg->setSizePolicy(sizePolicy);
        IngestDlg->setModal(false);
        seqGroupBox = new QGroupBox(IngestDlg);
        seqGroupBox->setObjectName(QString::fromUtf8("seqGroupBox"));
        seqGroupBox->setGeometry(QRect(20, 510, 691, 111));
        seqGroupBox->setMinimumSize(QSize(381, 0));
        QFont font;
        font.setPointSize(10);
        seqGroupBox->setFont(font);
        folderPathLabel = new QLabel(seqGroupBox);
        folderPathLabel->setObjectName(QString::fromUtf8("folderPathLabel"));
        folderPathLabel->setGeometry(QRect(20, 20, 651, 20));
        QPalette palette;
        QBrush brush(QColor(180, 180, 120, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Active, QPalette::Text, brush);
        palette.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        QBrush brush1(QColor(180, 180, 120, 128));
        brush1.setStyle(Qt::NoBrush);
        palette.setBrush(QPalette::Active, QPalette::PlaceholderText, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        QBrush brush2(QColor(180, 180, 120, 128));
        brush2.setStyle(Qt::NoBrush);
        palette.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        QBrush brush3(QColor(180, 180, 120, 128));
        brush3.setStyle(Qt::NoBrush);
        palette.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush3);
        folderPathLabel->setPalette(palette);
        folderPathLabel->setFont(font);
        folderPathLabel->setStyleSheet(QString::fromUtf8("color:rgb(180,180,120)"));
        folderPathLabel->setFrameShape(QFrame::Box);
        folderPathLabel_2 = new QLabel(seqGroupBox);
        folderPathLabel_2->setObjectName(QString::fromUtf8("folderPathLabel_2"));
        folderPathLabel_2->setGeometry(QRect(20, 40, 651, 20));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Text, brush);
        palette1.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        QBrush brush4(QColor(180, 180, 120, 128));
        brush4.setStyle(Qt::NoBrush);
        palette1.setBrush(QPalette::Active, QPalette::PlaceholderText, brush4);
        palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        QBrush brush5(QColor(180, 180, 120, 128));
        brush5.setStyle(Qt::NoBrush);
        palette1.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush5);
        palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        QBrush brush6(QColor(180, 180, 120, 128));
        brush6.setStyle(Qt::NoBrush);
        palette1.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush6);
        folderPathLabel_2->setPalette(palette1);
        folderPathLabel_2->setFont(font);
        folderPathLabel_2->setStyleSheet(QString::fromUtf8("color:rgb(180,180,120)"));
        folderPathLabel_2->setFrameShape(QFrame::Box);
        folderPathLabel_3 = new QLabel(seqGroupBox);
        folderPathLabel_3->setObjectName(QString::fromUtf8("folderPathLabel_3"));
        folderPathLabel_3->setGeometry(QRect(20, 60, 651, 20));
        QPalette palette2;
        QBrush brush7(QColor(0, 0, 0, 255));
        brush7.setStyle(Qt::SolidPattern);
        palette2.setBrush(QPalette::Active, QPalette::Text, brush7);
        QBrush brush8(QColor(0, 0, 0, 128));
        brush8.setStyle(Qt::NoBrush);
        palette2.setBrush(QPalette::Active, QPalette::PlaceholderText, brush8);
        palette2.setBrush(QPalette::Inactive, QPalette::Text, brush7);
        QBrush brush9(QColor(0, 0, 0, 128));
        brush9.setStyle(Qt::NoBrush);
        palette2.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush9);
        QBrush brush10(QColor(120, 120, 120, 255));
        brush10.setStyle(Qt::SolidPattern);
        palette2.setBrush(QPalette::Disabled, QPalette::Text, brush10);
        QBrush brush11(QColor(0, 0, 0, 128));
        brush11.setStyle(Qt::NoBrush);
        palette2.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush11);
        folderPathLabel_3->setPalette(palette2);
        folderPathLabel_3->setFont(font);
        folderPathLabel_3->setFrameShape(QFrame::Box);
        folderPathLabel_4 = new QLabel(seqGroupBox);
        folderPathLabel_4->setObjectName(QString::fromUtf8("folderPathLabel_4"));
        folderPathLabel_4->setGeometry(QRect(20, 80, 651, 20));
        QPalette palette3;
        palette3.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Active, QPalette::Text, brush);
        palette3.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        QBrush brush12(QColor(180, 180, 120, 128));
        brush12.setStyle(Qt::NoBrush);
        palette3.setBrush(QPalette::Active, QPalette::PlaceholderText, brush12);
        palette3.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        QBrush brush13(QColor(180, 180, 120, 128));
        brush13.setStyle(Qt::NoBrush);
        palette3.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush13);
        palette3.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette3.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        QBrush brush14(QColor(180, 180, 120, 128));
        brush14.setStyle(Qt::NoBrush);
        palette3.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush14);
        folderPathLabel_4->setPalette(palette3);
        folderPathLabel_4->setFont(font);
        folderPathLabel_4->setStyleSheet(QString::fromUtf8("color:rgb(180,180,120)"));
        folderPathLabel_4->setFrameShape(QFrame::Box);
        progressBar = new QProgressBar(IngestDlg);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setGeometry(QRect(20, 700, 691, 20));
        progressBar->setFont(font);
        progressBar->setValue(0);
        progressBar->setAlignment(Qt::AlignCenter);
        groupBox_2 = new QGroupBox(IngestDlg);
        groupBox_2->setObjectName(QString::fromUtf8("groupBox_2"));
        groupBox_2->setGeometry(QRect(20, 20, 691, 351));
        groupBox_2->setFont(font);
        selectFolderBtn = new QPushButton(groupBox_2);
        selectFolderBtn->setObjectName(QString::fromUtf8("selectFolderBtn"));
        selectFolderBtn->setEnabled(true);
        selectFolderBtn->setGeometry(QRect(90, 286, 101, 25));
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(false);
        font1.setWeight(50);
        selectFolderBtn->setFont(font1);
        selectFolderBtn->setAutoDefault(false);
        selectFolderBtn->setFlat(false);
        autoRadio = new QRadioButton(groupBox_2);
        autoRadio->setObjectName(QString::fromUtf8("autoRadio"));
        autoRadio->setGeometry(QRect(20, 30, 471, 17));
        autoRadio->setFont(font1);
        manualRadio = new QRadioButton(groupBox_2);
        manualRadio->setObjectName(QString::fromUtf8("manualRadio"));
        manualRadio->setGeometry(QRect(20, 260, 511, 17));
        manualRadio->setFont(font1);
        manualFolderLabel = new QLabel(groupBox_2);
        manualFolderLabel->setObjectName(QString::fromUtf8("manualFolderLabel"));
        manualFolderLabel->setGeometry(QRect(210, 290, 461, 20));
        manualFolderLabel->setFont(font);
        manualFolderLabel->setStyleSheet(QString::fromUtf8(""));
        manualFolderLabel->setFrameShape(QFrame::Box);
        autoIngestTab = new QTabWidget(groupBox_2);
        autoIngestTab->setObjectName(QString::fromUtf8("autoIngestTab"));
        autoIngestTab->setGeometry(QRect(50, 60, 631, 181));
        primaryTab = new QWidget();
        primaryTab->setObjectName(QString::fromUtf8("primaryTab"));
        destinationFolderLabel = new QLabel(primaryTab);
        destinationFolderLabel->setObjectName(QString::fromUtf8("destinationFolderLabel"));
        destinationFolderLabel->setGeometry(QRect(0, 120, 140, 20));
        destinationFolderLabel->setFont(font);
        destinationFolderLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        folderLabel = new QLabel(primaryTab);
        folderLabel->setObjectName(QString::fromUtf8("folderLabel"));
        folderLabel->setGeometry(QRect(160, 120, 460, 20));
        folderLabel->setFont(font);
        folderLabel->setStyleSheet(QString::fromUtf8(""));
        folderLabel->setFrameShape(QFrame::Box);
        templateLabel1 = new QLabel(primaryTab);
        templateLabel1->setObjectName(QString::fromUtf8("templateLabel1"));
        templateLabel1->setGeometry(QRect(0, 50, 140, 20));
        templateLabel1->setFont(font);
        templateLabel1->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        selectRootFolderBtn = new QPushButton(primaryTab);
        selectRootFolderBtn->setObjectName(QString::fromUtf8("selectRootFolderBtn"));
        selectRootFolderBtn->setGeometry(QRect(20, 16, 120, 25));
        selectRootFolderBtn->setFont(font1);
        selectRootFolderBtn->setAutoDefault(false);
        selectRootFolderBtn->setFlat(false);
        pathTemplatesCB = new QComboBox(primaryTab);
        pathTemplatesCB->setObjectName(QString::fromUtf8("pathTemplatesCB"));
        pathTemplatesCB->setGeometry(QRect(160, 50, 320, 26));
        descriptionLineEdit = new QLineEdit(primaryTab);
        descriptionLineEdit->setObjectName(QString::fromUtf8("descriptionLineEdit"));
        descriptionLineEdit->setGeometry(QRect(155, 90, 325, 20));
        descriptionLineEdit->setFont(font);
        folderDescription = new QLabel(primaryTab);
        folderDescription->setObjectName(QString::fromUtf8("folderDescription"));
        folderDescription->setGeometry(QRect(0, 91, 140, 20));
        folderDescription->setFont(font);
        folderDescription->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        rootFolderLabel = new QLabel(primaryTab);
        rootFolderLabel->setObjectName(QString::fromUtf8("rootFolderLabel"));
        rootFolderLabel->setGeometry(QRect(160, 20, 460, 20));
        rootFolderLabel->setFont(font);
        rootFolderLabel->setFrameShape(QFrame::Box);
        pathTemplatesBtn = new QPushButton(primaryTab);
        pathTemplatesBtn->setObjectName(QString::fromUtf8("pathTemplatesBtn"));
        pathTemplatesBtn->setGeometry(QRect(500, 50, 120, 25));
        pathTemplatesBtn->setFont(font1);
        pathTemplatesBtn->setToolTipDuration(-3);
        pathTemplatesBtn->setAutoDefault(false);
        pathTemplatesBtn->setFlat(false);
        autoIngestTab->addTab(primaryTab, QString());
        backupTab = new QWidget();
        backupTab->setObjectName(QString::fromUtf8("backupTab"));
        folderLabel_2 = new QLabel(backupTab);
        folderLabel_2->setObjectName(QString::fromUtf8("folderLabel_2"));
        folderLabel_2->setGeometry(QRect(160, 120, 461, 20));
        folderLabel_2->setFont(font);
        folderLabel_2->setStyleSheet(QString::fromUtf8(""));
        folderLabel_2->setFrameShape(QFrame::Box);
        templateLabel1_2 = new QLabel(backupTab);
        templateLabel1_2->setObjectName(QString::fromUtf8("templateLabel1_2"));
        templateLabel1_2->setGeometry(QRect(0, 50, 140, 20));
        templateLabel1_2->setFont(font);
        templateLabel1_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        selectRootFolderBtn_2 = new QPushButton(backupTab);
        selectRootFolderBtn_2->setObjectName(QString::fromUtf8("selectRootFolderBtn_2"));
        selectRootFolderBtn_2->setGeometry(QRect(20, 16, 120, 25));
        selectRootFolderBtn_2->setFont(font1);
        selectRootFolderBtn_2->setAutoDefault(false);
        selectRootFolderBtn_2->setFlat(false);
        pathTemplatesCB_2 = new QComboBox(backupTab);
        pathTemplatesCB_2->setObjectName(QString::fromUtf8("pathTemplatesCB_2"));
        pathTemplatesCB_2->setGeometry(QRect(160, 50, 320, 26));
        descriptionLineEdit_2 = new QLineEdit(backupTab);
        descriptionLineEdit_2->setObjectName(QString::fromUtf8("descriptionLineEdit_2"));
        descriptionLineEdit_2->setGeometry(QRect(155, 90, 325, 20));
        descriptionLineEdit_2->setFont(font);
        folderDescription_2 = new QLabel(backupTab);
        folderDescription_2->setObjectName(QString::fromUtf8("folderDescription_2"));
        folderDescription_2->setGeometry(QRect(0, 91, 140, 20));
        folderDescription_2->setFont(font);
        folderDescription_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        destinationFolderLabel_2 = new QLabel(backupTab);
        destinationFolderLabel_2->setObjectName(QString::fromUtf8("destinationFolderLabel_2"));
        destinationFolderLabel_2->setGeometry(QRect(0, 120, 140, 20));
        destinationFolderLabel_2->setFont(font);
        destinationFolderLabel_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        rootFolderLabel_2 = new QLabel(backupTab);
        rootFolderLabel_2->setObjectName(QString::fromUtf8("rootFolderLabel_2"));
        rootFolderLabel_2->setGeometry(QRect(160, 20, 461, 20));
        rootFolderLabel_2->setFont(font);
        rootFolderLabel_2->setFrameShape(QFrame::Box);
        pathTemplatesBtn_2 = new QPushButton(backupTab);
        pathTemplatesBtn_2->setObjectName(QString::fromUtf8("pathTemplatesBtn_2"));
        pathTemplatesBtn_2->setGeometry(QRect(500, 50, 120, 25));
        pathTemplatesBtn_2->setFont(font1);
        pathTemplatesBtn_2->setToolTipDuration(-3);
        pathTemplatesBtn_2->setAutoDefault(false);
        pathTemplatesBtn_2->setFlat(false);
        autoIngestTab->addTab(backupTab, QString());
        helpBtn = new QPushButton(groupBox_2);
        helpBtn->setObjectName(QString::fromUtf8("helpBtn"));
        helpBtn->setGeometry(QRect(560, 30, 111, 25));
        helpBtn->setAutoDefault(false);
        selectFolderBtn_2 = new QPushButton(groupBox_2);
        selectFolderBtn_2->setObjectName(QString::fromUtf8("selectFolderBtn_2"));
        selectFolderBtn_2->setEnabled(true);
        selectFolderBtn_2->setGeometry(QRect(90, 316, 101, 25));
        selectFolderBtn_2->setFont(font1);
        selectFolderBtn_2->setAutoDefault(false);
        selectFolderBtn_2->setFlat(false);
        manualFolderLabel_2 = new QLabel(groupBox_2);
        manualFolderLabel_2->setObjectName(QString::fromUtf8("manualFolderLabel_2"));
        manualFolderLabel_2->setGeometry(QRect(210, 320, 461, 20));
        manualFolderLabel_2->setFont(font);
        manualFolderLabel_2->setStyleSheet(QString::fromUtf8(""));
        manualFolderLabel_2->setFrameShape(QFrame::Box);
        groupBox_4 = new QGroupBox(IngestDlg);
        groupBox_4->setObjectName(QString::fromUtf8("groupBox_4"));
        groupBox_4->setGeometry(QRect(20, 390, 691, 101));
        groupBox_4->setFont(font);
        existingSequenceLabel = new QLabel(groupBox_4);
        existingSequenceLabel->setObjectName(QString::fromUtf8("existingSequenceLabel"));
        existingSequenceLabel->setGeometry(QRect(270, 70, 401, 20));
        existingSequenceLabel->setStyleSheet(QString::fromUtf8(""));
        existingSequenceLabel->setFrameShape(QFrame::Box);
        startSeqLabel = new QLabel(groupBox_4);
        startSeqLabel->setObjectName(QString::fromUtf8("startSeqLabel"));
        startSeqLabel->setGeometry(QRect(20, 71, 171, 20));
        startSeqLabel->setFont(font);
        startSeqLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        spinBoxStartNumber = new QSpinBox(groupBox_4);
        spinBoxStartNumber->setObjectName(QString::fromUtf8("spinBoxStartNumber"));
        spinBoxStartNumber->setGeometry(QRect(210, 70, 51, 22));
        spinBoxStartNumber->setFont(font);
        spinBoxStartNumber->setMinimum(1);
        spinBoxStartNumber->setMaximum(9999);
        label_5 = new QLabel(groupBox_4);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setGeometry(QRect(50, 30, 140, 20));
        label_5->setFont(font);
        label_5->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        filenameTemplatesCB = new QComboBox(groupBox_4);
        filenameTemplatesCB->setObjectName(QString::fromUtf8("filenameTemplatesCB"));
        filenameTemplatesCB->setGeometry(QRect(210, 30, 321, 26));
        filenameTemplatesBtn = new QPushButton(groupBox_4);
        filenameTemplatesBtn->setObjectName(QString::fromUtf8("filenameTemplatesBtn"));
        filenameTemplatesBtn->setGeometry(QRect(550, 30, 120, 25));
        filenameTemplatesBtn->setFont(font1);
        filenameTemplatesBtn->setAutoDefault(false);
        filenameTemplatesBtn->setFlat(false);
        statsLabel = new QLabel(IngestDlg);
        statsLabel->setObjectName(QString::fromUtf8("statsLabel"));
        statsLabel->setGeometry(QRect(20, 740, 241, 16));
        statsLabel->setFont(font);
        layoutWidget = new QWidget(IngestDlg);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(460, 730, 251, 32));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        cancelBtn = new QPushButton(layoutWidget);
        cancelBtn->setObjectName(QString::fromUtf8("cancelBtn"));
        cancelBtn->setMinimumSize(QSize(93, 0));
        cancelBtn->setAutoDefault(false);

        horizontalLayout->addWidget(cancelBtn);

        okBtn = new QPushButton(layoutWidget);
        okBtn->setObjectName(QString::fromUtf8("okBtn"));
        okBtn->setMinimumSize(QSize(93, 0));
        okBtn->setFocusPolicy(Qt::StrongFocus);
        okBtn->setAutoDefault(false);

        horizontalLayout->addWidget(okBtn);

        combinedIncludeJpgChk = new QCheckBox(IngestDlg);
        combinedIncludeJpgChk->setObjectName(QString::fromUtf8("combinedIncludeJpgChk"));
        combinedIncludeJpgChk->setGeometry(QRect(380, 640, 330, 20));
        combinedIncludeJpgChk->setFont(font);
        ejectChk = new QCheckBox(IngestDlg);
        ejectChk->setObjectName(QString::fromUtf8("ejectChk"));
        ejectChk->setGeometry(QRect(380, 660, 330, 20));
        ejectChk->setFont(font);
        openIngestFolderChk = new QCheckBox(IngestDlg);
        openIngestFolderChk->setObjectName(QString::fromUtf8("openIngestFolderChk"));
        openIngestFolderChk->setGeometry(QRect(20, 660, 330, 20));
        openIngestFolderChk->setFont(font);
        backupChk = new QCheckBox(IngestDlg);
        backupChk->setObjectName(QString::fromUtf8("backupChk"));
        backupChk->setGeometry(QRect(20, 640, 311, 0));
        backupChk->setFont(font);

        retranslateUi(IngestDlg);

        selectFolderBtn->setDefault(false);
        autoIngestTab->setCurrentIndex(1);
        selectRootFolderBtn->setDefault(false);
        pathTemplatesBtn->setDefault(false);
        selectRootFolderBtn_2->setDefault(false);
        pathTemplatesBtn_2->setDefault(false);
        selectFolderBtn_2->setDefault(false);
        filenameTemplatesBtn->setDefault(false);
        okBtn->setDefault(false);


        QMetaObject::connectSlotsByName(IngestDlg);
    } // setupUi

    void retranslateUi(QDialog *IngestDlg)
    {
        IngestDlg->setWindowTitle(QApplication::translate("IngestDlg", "Copy picked images to destination folder", nullptr));
#ifndef QT_NO_TOOLTIP
        seqGroupBox->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        seqGroupBox->setTitle(QApplication::translate("IngestDlg", "Images will be copied and renamed to", nullptr));
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
        selectFolderBtn->setToolTip(QApplication::translate("IngestDlg", "Select and create the primary location for the files to be copied to", nullptr));
#endif // QT_NO_TOOLTIP
        selectFolderBtn->setText(QApplication::translate("IngestDlg", "Primary", nullptr));
#ifndef QT_NO_TOOLTIP
        autoRadio->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        autoRadio->setText(QApplication::translate("IngestDlg", "Automatically generate destination folder from root folder", nullptr));
#ifndef QT_NO_TOOLTIP
        manualRadio->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        manualRadio->setText(QApplication::translate("IngestDlg", "Manually select destination folder", nullptr));
#ifndef QT_NO_TOOLTIP
        manualFolderLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        manualFolderLabel->setText(QString());
#ifndef QT_NO_TOOLTIP
        destinationFolderLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        destinationFolderLabel->setText(QApplication::translate("IngestDlg", "Destination folder:", nullptr));
#ifndef QT_NO_TOOLTIP
        folderLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderLabel->setText(QString());
#ifndef QT_NO_TOOLTIP
        templateLabel1->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        templateLabel1->setText(QApplication::translate("IngestDlg", "Template:", nullptr));
#ifndef QT_NO_TOOLTIP
        selectRootFolderBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        selectRootFolderBtn->setText(QApplication::translate("IngestDlg", "Root Folder", nullptr));
#ifndef QT_NO_TOOLTIP
        pathTemplatesCB->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        pathTemplatesCB->setCurrentText(QString());
#ifndef QT_NO_TOOLTIP
        descriptionLineEdit->setToolTip(QString());
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        folderDescription->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderDescription->setText(QApplication::translate("IngestDlg", "Append Description:", nullptr));
#ifndef QT_NO_TOOLTIP
        rootFolderLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        rootFolderLabel->setText(QString());
#ifndef QT_NO_TOOLTIP
        pathTemplatesBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        pathTemplatesBtn->setText(QApplication::translate("IngestDlg", "Edit template(s)", nullptr));
        autoIngestTab->setTabText(autoIngestTab->indexOf(primaryTab), QApplication::translate("IngestDlg", "  Primary Location Parameters  ", nullptr));
#ifndef QT_NO_TOOLTIP
        folderLabel_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderLabel_2->setText(QString());
#ifndef QT_NO_TOOLTIP
        templateLabel1_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        templateLabel1_2->setText(QApplication::translate("IngestDlg", "Template:", nullptr));
#ifndef QT_NO_TOOLTIP
        selectRootFolderBtn_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        selectRootFolderBtn_2->setText(QApplication::translate("IngestDlg", "Root Folder", nullptr));
#ifndef QT_NO_TOOLTIP
        pathTemplatesCB_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        pathTemplatesCB_2->setCurrentText(QString());
#ifndef QT_NO_TOOLTIP
        descriptionLineEdit_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        folderDescription_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        folderDescription_2->setText(QApplication::translate("IngestDlg", "Append Description:", nullptr));
#ifndef QT_NO_TOOLTIP
        destinationFolderLabel_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        destinationFolderLabel_2->setText(QApplication::translate("IngestDlg", "Destination folder:", nullptr));
#ifndef QT_NO_TOOLTIP
        rootFolderLabel_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        rootFolderLabel_2->setText(QString());
#ifndef QT_NO_TOOLTIP
        pathTemplatesBtn_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        pathTemplatesBtn_2->setText(QApplication::translate("IngestDlg", "Edit template(s)", nullptr));
        autoIngestTab->setTabText(autoIngestTab->indexOf(backupTab), QApplication::translate("IngestDlg", "Backup Location Parameters", nullptr));
#ifndef QT_NO_TOOLTIP
        helpBtn->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        helpBtn->setText(QApplication::translate("IngestDlg", "How this works", nullptr));
#ifndef QT_NO_TOOLTIP
        selectFolderBtn_2->setToolTip(QApplication::translate("IngestDlg", "Select and create the backup location for the files to be copied to", nullptr));
#endif // QT_NO_TOOLTIP
        selectFolderBtn_2->setText(QApplication::translate("IngestDlg", "Backup", nullptr));
#ifndef QT_NO_TOOLTIP
        manualFolderLabel_2->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        manualFolderLabel_2->setText(QString());
#ifndef QT_NO_TOOLTIP
        groupBox_4->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        groupBox_4->setTitle(QApplication::translate("IngestDlg", "File naming and sequencing", nullptr));
#ifndef QT_NO_TOOLTIP
        existingSequenceLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        existingSequenceLabel->setText(QString());
#ifndef QT_NO_TOOLTIP
        startSeqLabel->setToolTip(QString());
#endif // QT_NO_TOOLTIP
        startSeqLabel->setText(QApplication::translate("IngestDlg", "Start sequence from:", nullptr));
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
        combinedIncludeJpgChk->setText(QApplication::translate("IngestDlg", "If Raw+JPG also copy the JPG files", nullptr));
        ejectChk->setText(QApplication::translate("IngestDlg", "Eject drive after ingesting", nullptr));
        openIngestFolderChk->setText(QApplication::translate("IngestDlg", "Open ingest folder after ingest", nullptr));
        backupChk->setText(QApplication::translate("IngestDlg", "Backup to second location while ingesting", nullptr));
    } // retranslateUi

};

namespace Ui {
    class IngestDlg: public Ui_IngestDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INGESTDLG_H
