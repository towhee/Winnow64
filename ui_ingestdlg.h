/********************************************************************************
** Form generated from reading UI file 'ingestdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.1
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
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_IngestDlg
{
public:
    QGroupBox *seqGroupBox;
    QVBoxLayout *verticalLayout;
    QLabel *folderPathLabel;
    QLabel *folderPathLabel_2;
    QLabel *folderPathLabel_3;
    QLabel *folderPathLabel_4;
    QProgressBar *progressBar;
    QGroupBox *ingestToGroupBox;
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
    QGroupBox *filenameGroupBox;
    QLabel *existingSequenceLabel;
    QLabel *startSeqLabel;
    QSpinBox *spinBoxStartNumber;
    QLabel *templateLabel2;
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
    QCheckBox *includeXmpChk;

    void setupUi(QDialog *IngestDlg)
    {
        if (IngestDlg->objectName().isEmpty())
            IngestDlg->setObjectName(QString::fromUtf8("IngestDlg"));
        IngestDlg->resize(873, 874);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(IngestDlg->sizePolicy().hasHeightForWidth());
        IngestDlg->setSizePolicy(sizePolicy);
        QFont font;
        font.setPointSize(14);
        IngestDlg->setFont(font);
        IngestDlg->setModal(false);
        seqGroupBox = new QGroupBox(IngestDlg);
        seqGroupBox->setObjectName(QString::fromUtf8("seqGroupBox"));
        seqGroupBox->setGeometry(QRect(20, 550, 831, 131));
        seqGroupBox->setMinimumSize(QSize(381, 0));
        seqGroupBox->setFont(font);
        verticalLayout = new QVBoxLayout(seqGroupBox);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(-1, 9, -1, 9);
        folderPathLabel = new QLabel(seqGroupBox);
        folderPathLabel->setObjectName(QString::fromUtf8("folderPathLabel"));
        QPalette palette;
        QBrush brush(QColor(180, 180, 120, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Active, QPalette::Text, brush);
        palette.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        QBrush brush1(QColor(180, 180, 120, 128));
        brush1.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette.setBrush(QPalette::Active, QPalette::PlaceholderText, brush1);
#endif
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        QBrush brush2(QColor(180, 180, 120, 128));
        brush2.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush2);
#endif
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        QBrush brush3(QColor(180, 180, 120, 128));
        brush3.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush3);
#endif
        folderPathLabel->setPalette(palette);
        folderPathLabel->setFont(font);
        folderPathLabel->setStyleSheet(QString::fromUtf8("color:rgb(180,180,120)"));
        folderPathLabel->setFrameShape(QFrame::Box);

        verticalLayout->addWidget(folderPathLabel);

        folderPathLabel_2 = new QLabel(seqGroupBox);
        folderPathLabel_2->setObjectName(QString::fromUtf8("folderPathLabel_2"));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Text, brush);
        palette1.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        QBrush brush4(QColor(180, 180, 120, 128));
        brush4.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette1.setBrush(QPalette::Active, QPalette::PlaceholderText, brush4);
#endif
        palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        QBrush brush5(QColor(180, 180, 120, 128));
        brush5.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette1.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush5);
#endif
        palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        QBrush brush6(QColor(180, 180, 120, 128));
        brush6.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette1.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush6);
#endif
        folderPathLabel_2->setPalette(palette1);
        folderPathLabel_2->setFont(font);
        folderPathLabel_2->setStyleSheet(QString::fromUtf8("color:rgb(180,180,120)"));
        folderPathLabel_2->setFrameShape(QFrame::Box);

        verticalLayout->addWidget(folderPathLabel_2);

        folderPathLabel_3 = new QLabel(seqGroupBox);
        folderPathLabel_3->setObjectName(QString::fromUtf8("folderPathLabel_3"));
        QPalette palette2;
        QBrush brush7(QColor(0, 0, 0, 255));
        brush7.setStyle(Qt::SolidPattern);
        palette2.setBrush(QPalette::Active, QPalette::Text, brush7);
        QBrush brush8(QColor(0, 0, 0, 128));
        brush8.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette2.setBrush(QPalette::Active, QPalette::PlaceholderText, brush8);
#endif
        palette2.setBrush(QPalette::Inactive, QPalette::Text, brush7);
        QBrush brush9(QColor(0, 0, 0, 128));
        brush9.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette2.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush9);
#endif
        QBrush brush10(QColor(120, 120, 120, 255));
        brush10.setStyle(Qt::SolidPattern);
        palette2.setBrush(QPalette::Disabled, QPalette::Text, brush10);
        QBrush brush11(QColor(0, 0, 0, 128));
        brush11.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette2.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush11);
#endif
        folderPathLabel_3->setPalette(palette2);
        folderPathLabel_3->setFont(font);
        folderPathLabel_3->setFrameShape(QFrame::Box);

        verticalLayout->addWidget(folderPathLabel_3);

        folderPathLabel_4 = new QLabel(seqGroupBox);
        folderPathLabel_4->setObjectName(QString::fromUtf8("folderPathLabel_4"));
        QPalette palette3;
        palette3.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Active, QPalette::Text, brush);
        palette3.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        QBrush brush12(QColor(180, 180, 120, 128));
        brush12.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette3.setBrush(QPalette::Active, QPalette::PlaceholderText, brush12);
#endif
        palette3.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        QBrush brush13(QColor(180, 180, 120, 128));
        brush13.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette3.setBrush(QPalette::Inactive, QPalette::PlaceholderText, brush13);
#endif
        palette3.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette3.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        QBrush brush14(QColor(180, 180, 120, 128));
        brush14.setStyle(Qt::NoBrush);
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
        palette3.setBrush(QPalette::Disabled, QPalette::PlaceholderText, brush14);
#endif
        folderPathLabel_4->setPalette(palette3);
        folderPathLabel_4->setFont(font);
        folderPathLabel_4->setStyleSheet(QString::fromUtf8("color:rgb(180,180,120)"));
        folderPathLabel_4->setFrameShape(QFrame::Box);

        verticalLayout->addWidget(folderPathLabel_4);

        progressBar = new QProgressBar(IngestDlg);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setGeometry(QRect(20, 780, 831, 20));
        progressBar->setFont(font);
        progressBar->setValue(0);
        progressBar->setAlignment(Qt::AlignCenter);
        ingestToGroupBox = new QGroupBox(IngestDlg);
        ingestToGroupBox->setObjectName(QString::fromUtf8("ingestToGroupBox"));
        ingestToGroupBox->setGeometry(QRect(20, 20, 831, 391));
        ingestToGroupBox->setFont(font);
        selectFolderBtn = new QPushButton(ingestToGroupBox);
        selectFolderBtn->setObjectName(QString::fromUtf8("selectFolderBtn"));
        selectFolderBtn->setEnabled(true);
        selectFolderBtn->setGeometry(QRect(120, 320, 101, 25));
        QFont font1;
        font1.setPointSize(14);
        font1.setBold(false);
        font1.setWeight(50);
        selectFolderBtn->setFont(font1);
        selectFolderBtn->setAutoDefault(false);
        selectFolderBtn->setFlat(false);
        autoRadio = new QRadioButton(ingestToGroupBox);
        autoRadio->setObjectName(QString::fromUtf8("autoRadio"));
        autoRadio->setGeometry(QRect(20, 36, 531, 21));
        autoRadio->setFont(font1);
        manualRadio = new QRadioButton(ingestToGroupBox);
        manualRadio->setObjectName(QString::fromUtf8("manualRadio"));
        manualRadio->setGeometry(QRect(20, 284, 511, 17));
        manualRadio->setFont(font1);
        manualFolderLabel = new QLabel(ingestToGroupBox);
        manualFolderLabel->setObjectName(QString::fromUtf8("manualFolderLabel"));
        manualFolderLabel->setGeometry(QRect(240, 320, 561, 25));
        manualFolderLabel->setFont(font);
        manualFolderLabel->setStyleSheet(QString::fromUtf8(""));
        manualFolderLabel->setFrameShape(QFrame::Box);
        autoIngestTab = new QTabWidget(ingestToGroupBox);
        autoIngestTab->setObjectName(QString::fromUtf8("autoIngestTab"));
        autoIngestTab->setGeometry(QRect(40, 70, 771, 191));
        autoIngestTab->setFont(font);
        primaryTab = new QWidget();
        primaryTab->setObjectName(QString::fromUtf8("primaryTab"));
        destinationFolderLabel = new QLabel(primaryTab);
        destinationFolderLabel->setObjectName(QString::fromUtf8("destinationFolderLabel"));
        destinationFolderLabel->setGeometry(QRect(20, 120, 161, 20));
        destinationFolderLabel->setFont(font);
        destinationFolderLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        folderLabel = new QLabel(primaryTab);
        folderLabel->setObjectName(QString::fromUtf8("folderLabel"));
        folderLabel->setGeometry(QRect(200, 120, 561, 25));
        folderLabel->setFont(font);
        folderLabel->setStyleSheet(QString::fromUtf8(""));
        folderLabel->setFrameShape(QFrame::Box);
        templateLabel1 = new QLabel(primaryTab);
        templateLabel1->setObjectName(QString::fromUtf8("templateLabel1"));
        templateLabel1->setGeometry(QRect(41, 50, 140, 20));
        templateLabel1->setFont(font);
        templateLabel1->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        selectRootFolderBtn = new QPushButton(primaryTab);
        selectRootFolderBtn->setObjectName(QString::fromUtf8("selectRootFolderBtn"));
        selectRootFolderBtn->setGeometry(QRect(61, 16, 120, 25));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(selectRootFolderBtn->sizePolicy().hasHeightForWidth());
        selectRootFolderBtn->setSizePolicy(sizePolicy1);
        selectRootFolderBtn->setFont(font1);
        selectRootFolderBtn->setAutoDefault(false);
        selectRootFolderBtn->setFlat(false);
        pathTemplatesCB = new QComboBox(primaryTab);
        pathTemplatesCB->setObjectName(QString::fromUtf8("pathTemplatesCB"));
        pathTemplatesCB->setGeometry(QRect(200, 50, 320, 26));
        pathTemplatesCB->setFont(font);
        descriptionLineEdit = new QLineEdit(primaryTab);
        descriptionLineEdit->setObjectName(QString::fromUtf8("descriptionLineEdit"));
        descriptionLineEdit->setGeometry(QRect(195, 88, 561, 25));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(descriptionLineEdit->sizePolicy().hasHeightForWidth());
        descriptionLineEdit->setSizePolicy(sizePolicy2);
        descriptionLineEdit->setFont(font);
        folderDescription = new QLabel(primaryTab);
        folderDescription->setObjectName(QString::fromUtf8("folderDescription"));
        folderDescription->setGeometry(QRect(10, 91, 171, 20));
        folderDescription->setFont(font);
        folderDescription->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        rootFolderLabel = new QLabel(primaryTab);
        rootFolderLabel->setObjectName(QString::fromUtf8("rootFolderLabel"));
        rootFolderLabel->setGeometry(QRect(200, 16, 561, 25));
        rootFolderLabel->setFont(font);
        rootFolderLabel->setFrameShape(QFrame::Box);
        pathTemplatesBtn = new QPushButton(primaryTab);
        pathTemplatesBtn->setObjectName(QString::fromUtf8("pathTemplatesBtn"));
        pathTemplatesBtn->setGeometry(QRect(600, 50, 151, 25));
        pathTemplatesBtn->setFont(font1);
        pathTemplatesBtn->setToolTipDuration(-3);
        pathTemplatesBtn->setAutoDefault(false);
        pathTemplatesBtn->setFlat(false);
        autoIngestTab->addTab(primaryTab, QString());
        backupTab = new QWidget();
        backupTab->setObjectName(QString::fromUtf8("backupTab"));
        folderLabel_2 = new QLabel(backupTab);
        folderLabel_2->setObjectName(QString::fromUtf8("folderLabel_2"));
        folderLabel_2->setGeometry(QRect(200, 120, 561, 25));
        folderLabel_2->setFont(font);
        folderLabel_2->setStyleSheet(QString::fromUtf8(""));
        folderLabel_2->setFrameShape(QFrame::Box);
        templateLabel1_2 = new QLabel(backupTab);
        templateLabel1_2->setObjectName(QString::fromUtf8("templateLabel1_2"));
        templateLabel1_2->setGeometry(QRect(-10, 50, 191, 20));
        templateLabel1_2->setFont(font);
        templateLabel1_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        selectRootFolderBtn_2 = new QPushButton(backupTab);
        selectRootFolderBtn_2->setObjectName(QString::fromUtf8("selectRootFolderBtn_2"));
        selectRootFolderBtn_2->setGeometry(QRect(61, 16, 120, 25));
        selectRootFolderBtn_2->setFont(font1);
        selectRootFolderBtn_2->setAutoDefault(false);
        selectRootFolderBtn_2->setFlat(false);
        pathTemplatesCB_2 = new QComboBox(backupTab);
        pathTemplatesCB_2->setObjectName(QString::fromUtf8("pathTemplatesCB_2"));
        pathTemplatesCB_2->setGeometry(QRect(200, 50, 320, 26));
        pathTemplatesCB_2->setFont(font);
        descriptionLineEdit_2 = new QLineEdit(backupTab);
        descriptionLineEdit_2->setObjectName(QString::fromUtf8("descriptionLineEdit_2"));
        descriptionLineEdit_2->setGeometry(QRect(195, 90, 561, 25));
        sizePolicy1.setHeightForWidth(descriptionLineEdit_2->sizePolicy().hasHeightForWidth());
        descriptionLineEdit_2->setSizePolicy(sizePolicy1);
        descriptionLineEdit_2->setFont(font);
        folderDescription_2 = new QLabel(backupTab);
        folderDescription_2->setObjectName(QString::fromUtf8("folderDescription_2"));
        folderDescription_2->setGeometry(QRect(-10, 91, 191, 20));
        folderDescription_2->setFont(font);
        folderDescription_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        destinationFolderLabel_2 = new QLabel(backupTab);
        destinationFolderLabel_2->setObjectName(QString::fromUtf8("destinationFolderLabel_2"));
        destinationFolderLabel_2->setGeometry(QRect(-10, 120, 191, 20));
        destinationFolderLabel_2->setFont(font);
        destinationFolderLabel_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        rootFolderLabel_2 = new QLabel(backupTab);
        rootFolderLabel_2->setObjectName(QString::fromUtf8("rootFolderLabel_2"));
        rootFolderLabel_2->setGeometry(QRect(200, 16, 561, 25));
        rootFolderLabel_2->setFont(font);
        rootFolderLabel_2->setFrameShape(QFrame::Box);
        pathTemplatesBtn_2 = new QPushButton(backupTab);
        pathTemplatesBtn_2->setObjectName(QString::fromUtf8("pathTemplatesBtn_2"));
        pathTemplatesBtn_2->setGeometry(QRect(600, 50, 151, 25));
        QSizePolicy sizePolicy3(QSizePolicy::Minimum, QSizePolicy::Preferred);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(pathTemplatesBtn_2->sizePolicy().hasHeightForWidth());
        pathTemplatesBtn_2->setSizePolicy(sizePolicy3);
        pathTemplatesBtn_2->setFont(font1);
        pathTemplatesBtn_2->setToolTipDuration(-3);
        pathTemplatesBtn_2->setAutoDefault(false);
        pathTemplatesBtn_2->setFlat(false);
        autoIngestTab->addTab(backupTab, QString());
        helpBtn = new QPushButton(ingestToGroupBox);
        helpBtn->setObjectName(QString::fromUtf8("helpBtn"));
        helpBtn->setGeometry(QRect(640, 36, 151, 25));
        sizePolicy.setHeightForWidth(helpBtn->sizePolicy().hasHeightForWidth());
        helpBtn->setSizePolicy(sizePolicy);
        QFont font2;
        font2.setPointSize(13);
        helpBtn->setFont(font2);
        helpBtn->setAutoDefault(false);
        selectFolderBtn_2 = new QPushButton(ingestToGroupBox);
        selectFolderBtn_2->setObjectName(QString::fromUtf8("selectFolderBtn_2"));
        selectFolderBtn_2->setEnabled(true);
        selectFolderBtn_2->setGeometry(QRect(120, 350, 101, 25));
        selectFolderBtn_2->setFont(font1);
        selectFolderBtn_2->setAutoDefault(false);
        selectFolderBtn_2->setFlat(false);
        manualFolderLabel_2 = new QLabel(ingestToGroupBox);
        manualFolderLabel_2->setObjectName(QString::fromUtf8("manualFolderLabel_2"));
        manualFolderLabel_2->setGeometry(QRect(240, 350, 561, 25));
        manualFolderLabel_2->setFont(font);
        manualFolderLabel_2->setStyleSheet(QString::fromUtf8(""));
        manualFolderLabel_2->setFrameShape(QFrame::Box);
        filenameGroupBox = new QGroupBox(IngestDlg);
        filenameGroupBox->setObjectName(QString::fromUtf8("filenameGroupBox"));
        filenameGroupBox->setGeometry(QRect(20, 430, 831, 101));
        filenameGroupBox->setFont(font);
        existingSequenceLabel = new QLabel(filenameGroupBox);
        existingSequenceLabel->setObjectName(QString::fromUtf8("existingSequenceLabel"));
        existingSequenceLabel->setGeometry(QRect(310, 70, 491, 25));
        existingSequenceLabel->setFont(font);
        existingSequenceLabel->setStyleSheet(QString::fromUtf8(""));
        existingSequenceLabel->setFrameShape(QFrame::Box);
        startSeqLabel = new QLabel(filenameGroupBox);
        startSeqLabel->setObjectName(QString::fromUtf8("startSeqLabel"));
        startSeqLabel->setGeometry(QRect(20, 71, 201, 20));
        startSeqLabel->setFont(font);
        startSeqLabel->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        spinBoxStartNumber = new QSpinBox(filenameGroupBox);
        spinBoxStartNumber->setObjectName(QString::fromUtf8("spinBoxStartNumber"));
        spinBoxStartNumber->setGeometry(QRect(240, 70, 51, 22));
        spinBoxStartNumber->setFont(font);
        spinBoxStartNumber->setMinimum(1);
        spinBoxStartNumber->setMaximum(9999);
        templateLabel2 = new QLabel(filenameGroupBox);
        templateLabel2->setObjectName(QString::fromUtf8("templateLabel2"));
        templateLabel2->setGeometry(QRect(80, 35, 140, 20));
        templateLabel2->setFont(font);
        templateLabel2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        filenameTemplatesCB = new QComboBox(filenameGroupBox);
        filenameTemplatesCB->setObjectName(QString::fromUtf8("filenameTemplatesCB"));
        filenameTemplatesCB->setGeometry(QRect(240, 35, 351, 26));
        filenameTemplatesCB->setFont(font);
        filenameTemplatesBtn = new QPushButton(filenameGroupBox);
        filenameTemplatesBtn->setObjectName(QString::fromUtf8("filenameTemplatesBtn"));
        filenameTemplatesBtn->setGeometry(QRect(630, 35, 161, 25));
        sizePolicy3.setHeightForWidth(filenameTemplatesBtn->sizePolicy().hasHeightForWidth());
        filenameTemplatesBtn->setSizePolicy(sizePolicy3);
        filenameTemplatesBtn->setFont(font1);
        filenameTemplatesBtn->setAutoDefault(false);
        filenameTemplatesBtn->setFlat(false);
        statsLabel = new QLabel(IngestDlg);
        statsLabel->setObjectName(QString::fromUtf8("statsLabel"));
        statsLabel->setGeometry(QRect(20, 820, 351, 31));
        statsLabel->setFont(font);
        layoutWidget = new QWidget(IngestDlg);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(600, 810, 251, 43));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        cancelBtn = new QPushButton(layoutWidget);
        cancelBtn->setObjectName(QString::fromUtf8("cancelBtn"));
        cancelBtn->setMinimumSize(QSize(93, 0));
        cancelBtn->setFont(font);
        cancelBtn->setAutoDefault(false);

        horizontalLayout->addWidget(cancelBtn);

        okBtn = new QPushButton(layoutWidget);
        okBtn->setObjectName(QString::fromUtf8("okBtn"));
        okBtn->setMinimumSize(QSize(93, 0));
        okBtn->setFont(font);
        okBtn->setFocusPolicy(Qt::StrongFocus);
        okBtn->setAutoDefault(false);

        horizontalLayout->addWidget(okBtn);

        combinedIncludeJpgChk = new QCheckBox(IngestDlg);
        combinedIncludeJpgChk->setObjectName(QString::fromUtf8("combinedIncludeJpgChk"));
        combinedIncludeJpgChk->setGeometry(QRect(450, 700, 391, 25));
        sizePolicy.setHeightForWidth(combinedIncludeJpgChk->sizePolicy().hasHeightForWidth());
        combinedIncludeJpgChk->setSizePolicy(sizePolicy);
        combinedIncludeJpgChk->setFont(font);
        ejectChk = new QCheckBox(IngestDlg);
        ejectChk->setObjectName(QString::fromUtf8("ejectChk"));
        ejectChk->setGeometry(QRect(450, 725, 391, 25));
        sizePolicy.setHeightForWidth(ejectChk->sizePolicy().hasHeightForWidth());
        ejectChk->setSizePolicy(sizePolicy);
        ejectChk->setFont(font);
        openIngestFolderChk = new QCheckBox(IngestDlg);
        openIngestFolderChk->setObjectName(QString::fromUtf8("openIngestFolderChk"));
        openIngestFolderChk->setGeometry(QRect(20, 725, 421, 25));
        sizePolicy.setHeightForWidth(openIngestFolderChk->sizePolicy().hasHeightForWidth());
        openIngestFolderChk->setSizePolicy(sizePolicy);
        openIngestFolderChk->setFont(font);
        backupChk = new QCheckBox(IngestDlg);
        backupChk->setObjectName(QString::fromUtf8("backupChk"));
        backupChk->setGeometry(QRect(20, 700, 411, 25));
        sizePolicy.setHeightForWidth(backupChk->sizePolicy().hasHeightForWidth());
        backupChk->setSizePolicy(sizePolicy);
        backupChk->setFont(font);
        includeXmpChk = new QCheckBox(IngestDlg);
        includeXmpChk->setObjectName(QString::fromUtf8("includeXmpChk"));
        includeXmpChk->setGeometry(QRect(20, 750, 421, 25));
        sizePolicy.setHeightForWidth(includeXmpChk->sizePolicy().hasHeightForWidth());
        includeXmpChk->setSizePolicy(sizePolicy);
        includeXmpChk->setFont(font);

        retranslateUi(IngestDlg);

        selectFolderBtn->setDefault(false);
        autoIngestTab->setCurrentIndex(0);
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
        IngestDlg->setWindowTitle(QCoreApplication::translate("IngestDlg", "Copy picked images to destination folder", nullptr));
#if QT_CONFIG(tooltip)
        seqGroupBox->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        seqGroupBox->setTitle(QCoreApplication::translate("IngestDlg", "Images will be copied and renamed to", nullptr));
#if QT_CONFIG(tooltip)
        folderPathLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        folderPathLabel->setText(QString());
#if QT_CONFIG(tooltip)
        folderPathLabel_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        folderPathLabel_2->setText(QString());
#if QT_CONFIG(tooltip)
        folderPathLabel_3->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        folderPathLabel_3->setText(QCoreApplication::translate("IngestDlg", "...", nullptr));
#if QT_CONFIG(tooltip)
        folderPathLabel_4->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        folderPathLabel_4->setText(QString());
#if QT_CONFIG(tooltip)
        progressBar->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        ingestToGroupBox->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        ingestToGroupBox->setTitle(QCoreApplication::translate("IngestDlg", "Select destination folder", nullptr));
#if QT_CONFIG(tooltip)
        selectFolderBtn->setToolTip(QCoreApplication::translate("IngestDlg", "Select and create the primary location for the files to be copied to", nullptr));
#endif // QT_CONFIG(tooltip)
        selectFolderBtn->setText(QCoreApplication::translate("IngestDlg", "Primary", nullptr));
#if QT_CONFIG(tooltip)
        autoRadio->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        autoRadio->setText(QCoreApplication::translate("IngestDlg", "Automatically generate destination folder from root folder", nullptr));
#if QT_CONFIG(tooltip)
        manualRadio->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        manualRadio->setText(QCoreApplication::translate("IngestDlg", "Manually select destination folder", nullptr));
#if QT_CONFIG(tooltip)
        manualFolderLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        manualFolderLabel->setText(QString());
#if QT_CONFIG(tooltip)
        destinationFolderLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        destinationFolderLabel->setText(QCoreApplication::translate("IngestDlg", "Destination folder:", nullptr));
#if QT_CONFIG(tooltip)
        folderLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        folderLabel->setText(QString());
#if QT_CONFIG(tooltip)
        templateLabel1->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        templateLabel1->setText(QCoreApplication::translate("IngestDlg", "Template:", nullptr));
#if QT_CONFIG(tooltip)
        selectRootFolderBtn->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        selectRootFolderBtn->setText(QCoreApplication::translate("IngestDlg", "Root Folder", nullptr));
#if QT_CONFIG(tooltip)
        pathTemplatesCB->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        pathTemplatesCB->setCurrentText(QString());
#if QT_CONFIG(tooltip)
        descriptionLineEdit->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        folderDescription->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        folderDescription->setText(QCoreApplication::translate("IngestDlg", "Append Description:", nullptr));
#if QT_CONFIG(tooltip)
        rootFolderLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        rootFolderLabel->setText(QString());
#if QT_CONFIG(tooltip)
        pathTemplatesBtn->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        pathTemplatesBtn->setText(QCoreApplication::translate("IngestDlg", "Edit template(s)", nullptr));
        autoIngestTab->setTabText(autoIngestTab->indexOf(primaryTab), QCoreApplication::translate("IngestDlg", "  Primary Location Parameters  ", nullptr));
#if QT_CONFIG(tooltip)
        folderLabel_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        folderLabel_2->setText(QString());
#if QT_CONFIG(tooltip)
        templateLabel1_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        templateLabel1_2->setText(QCoreApplication::translate("IngestDlg", "Template:", nullptr));
#if QT_CONFIG(tooltip)
        selectRootFolderBtn_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        selectRootFolderBtn_2->setText(QCoreApplication::translate("IngestDlg", "Root Folder", nullptr));
#if QT_CONFIG(tooltip)
        pathTemplatesCB_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        pathTemplatesCB_2->setCurrentText(QString());
#if QT_CONFIG(tooltip)
        descriptionLineEdit_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        folderDescription_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        folderDescription_2->setText(QCoreApplication::translate("IngestDlg", "Append Description:", nullptr));
#if QT_CONFIG(tooltip)
        destinationFolderLabel_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        destinationFolderLabel_2->setText(QCoreApplication::translate("IngestDlg", "Destination folder:", nullptr));
#if QT_CONFIG(tooltip)
        rootFolderLabel_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        rootFolderLabel_2->setText(QString());
#if QT_CONFIG(tooltip)
        pathTemplatesBtn_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        pathTemplatesBtn_2->setText(QCoreApplication::translate("IngestDlg", "Edit template(s)", nullptr));
        autoIngestTab->setTabText(autoIngestTab->indexOf(backupTab), QCoreApplication::translate("IngestDlg", "Backup Location Parameters", nullptr));
#if QT_CONFIG(tooltip)
        helpBtn->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        helpBtn->setText(QCoreApplication::translate("IngestDlg", "How this works", nullptr));
#if QT_CONFIG(tooltip)
        selectFolderBtn_2->setToolTip(QCoreApplication::translate("IngestDlg", "Select and create the backup location for the files to be copied to", nullptr));
#endif // QT_CONFIG(tooltip)
        selectFolderBtn_2->setText(QCoreApplication::translate("IngestDlg", "Backup", nullptr));
#if QT_CONFIG(tooltip)
        manualFolderLabel_2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        manualFolderLabel_2->setText(QString());
#if QT_CONFIG(tooltip)
        filenameGroupBox->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        filenameGroupBox->setTitle(QCoreApplication::translate("IngestDlg", "File naming and sequencing", nullptr));
#if QT_CONFIG(tooltip)
        existingSequenceLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        existingSequenceLabel->setText(QString());
#if QT_CONFIG(tooltip)
        startSeqLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        startSeqLabel->setText(QCoreApplication::translate("IngestDlg", "Start sequence from:", nullptr));
#if QT_CONFIG(tooltip)
        spinBoxStartNumber->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        templateLabel2->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        templateLabel2->setText(QCoreApplication::translate("IngestDlg", "Template:", nullptr));
#if QT_CONFIG(tooltip)
        filenameTemplatesCB->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        filenameTemplatesBtn->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        filenameTemplatesBtn->setText(QCoreApplication::translate("IngestDlg", "Edit template(s)", nullptr));
#if QT_CONFIG(tooltip)
        statsLabel->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        statsLabel->setText(QCoreApplication::translate("IngestDlg", "632 files using 830MB", nullptr));
#if QT_CONFIG(tooltip)
        layoutWidget->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        cancelBtn->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        cancelBtn->setText(QCoreApplication::translate("IngestDlg", "Cancel", nullptr));
#if QT_CONFIG(tooltip)
        okBtn->setToolTip(QString());
#endif // QT_CONFIG(tooltip)
        okBtn->setText(QCoreApplication::translate("IngestDlg", "Ingest", nullptr));
        combinedIncludeJpgChk->setText(QCoreApplication::translate("IngestDlg", "If Raw+JPG also copy the JPG files", nullptr));
        ejectChk->setText(QCoreApplication::translate("IngestDlg", "Eject drive after ingesting", nullptr));
        openIngestFolderChk->setText(QCoreApplication::translate("IngestDlg", "Open ingest folder after ingest", nullptr));
        backupChk->setText(QCoreApplication::translate("IngestDlg", "Backup to second location while ingesting", nullptr));
        includeXmpChk->setText(QCoreApplication::translate("IngestDlg", "Include XMP sidecar with Winnow metadata", nullptr));
    } // retranslateUi

};

namespace Ui {
    class IngestDlg: public Ui_IngestDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_INGESTDLG_H
