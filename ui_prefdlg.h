/********************************************************************************
** Form generated from reading UI file 'prefdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.13.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PREFDLG_H
#define UI_PREFDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QButtonGroup>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSlider>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QTableView>
#include <QtWidgets/QTreeView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_Prefdlg
{
public:
    QPushButton *closeBtn;
    QStackedWidget *stackedWidget;
    QWidget *pageGeneral;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QGroupBox *groupBoxFolders;
    QCheckBox *rememberFolderChk;
    QCheckBox *updateAppChk;
    QGroupBox *groupBoxTrackpadAction;
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout_3;
    QRadioButton *trackpadIterateRadio;
    QRadioButton *trackpadScrollRadio;
    QLabel *label_23;
    QWidget *pageAppearance;
    QGroupBox *groupBoxFontSize;
    QSlider *globalFontSizeSlider;
    QLabel *globalFontSizeLbl;
    QSlider *infoFontSizeSlider;
    QLabel *label_21;
    QLabel *label_22;
    QLabel *infoFontSizeLbl;
    QGroupBox *groupBoxClassificationSize;
    QSlider *classificationBadgeImageDiamSlider;
    QLabel *classificationBadgeImageDiamLbl;
    QLabel *label_13;
    QLabel *label_20;
    QLabel *classificationBadgeThumbDiamLbl;
    QSlider *classificationBadgeThumbDiamSlider;
    QWidget *pageCache;
    QLabel *label_9;
    QSpinBox *cacheDelaySpinbox;
    QGroupBox *groupBox_3;
    QCheckBox *showCacheStatusChk;
    QLabel *label_10;
    QSlider *progressWidthSlider;
    QCheckBox *showCacheThreadActivityChk;
    QCheckBox *cachePreviewsChk;
    QRadioButton *cache100AheadRadio;
    QRadioButton *cache70AheadRadio;
    QRadioButton *cache90AheadRadio;
    QRadioButton *cache50AheadRadio;
    QRadioButton *cache60AheadRadio;
    QRadioButton *cache80AheadRadio;
    QSpinBox *cacheSizeSpinbox;
    QLabel *label_8;
    QLabel *label;
    QGroupBox *metadataCacheGroupBox;
    QRadioButton *metadataCacheAllRadio;
    QRadioButton *metadataCacheChunkRadio;
    QSpinBox *metadataCacheSizeSpinbox;
    QLabel *label_14;
    QGroupBox *thumbnailCacheGroupBoc;
    QRadioButton *thumbnailCachAllRadio;
    QRadioButton *thumbnailCacheChunkRadio;
    QSpinBox *thumbnailCacheSizeSpinbox;
    QLabel *label_15;
    QLabel *label_18;
    QWidget *pageSlideshow;
    QCheckBox *slideshowRandomChk;
    QLabel *label_12;
    QSpinBox *slideshowDelaySpinbox;
    QLabel *label_11;
    QWidget *pageThumbnails;
    QGroupBox *groupBox;
    QCheckBox *showThumbLabelChk;
    QLabel *label_7;
    QPushButton *thumbSmallerBtn;
    QLabel *fontSizeLbl;
    QLabel *label_2;
    QSlider *fontSizeSlider;
    QPushButton *thumbLargerBtn;
    QGroupBox *groupBox_2;
    QLabel *fontSizeLbl_2;
    QLabel *label_17;
    QPushButton *gridSmallerBtn;
    QLabel *label_19;
    QSlider *fontSizeSlider_2;
    QPushButton *gridLargerBtn;
    QCheckBox *showThumbLabelChk_2;
    QLabel *label_3;
    QLabel *fontSizeLbl_21;
    QLabel *label_171;
    QPushButton *gridSmallerBtn1;
    QLabel *label_191;
    QSlider *fontSizeSlider_21;
    QPushButton *gridLargerBtn1;
    QCheckBox *showThumbLabelChk_21;
    QSpinBox *maxIconSizeSpinBox;
    QLabel *label_4;
    QLabel *label_5;
    QWidget *pageFullScreen;
    QLabel *fullScreenExplanationLbl;
    QWidget *layoutWidget1;
    QVBoxLayout *verticalLayout_2;
    QCheckBox *foldersChk;
    QCheckBox *favsChk;
    QCheckBox *filtersChk;
    QCheckBox *metadataChk;
    QCheckBox *thumbsChk;
    QCheckBox *statusBarChk;
    QWidget *pageShowHideFields;
    QFrame *frame_2;
    QFrame *frame;
    QWidget *layoutWidget2;
    QVBoxLayout *verticalLayout_4;
    QLabel *label_6;
    QTreeView *infoFieldsTreeView;
    QFrame *frame_3;
    QWidget *layoutWidget3;
    QVBoxLayout *verticalLayout_5;
    QLabel *label_16;
    QTableView *tableFieldsTable;
    QStackedWidget *listContainer;
    QWidget *page;
    QListWidget *listWidget;
    QWidget *page_2;
    QButtonGroup *buttonGroup;

    void setupUi(QDialog *Prefdlg)
    {
        if (Prefdlg->objectName().isEmpty())
            Prefdlg->setObjectName(QString::fromUtf8("Prefdlg"));
        Prefdlg->resize(705, 588);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(Prefdlg->sizePolicy().hasHeightForWidth());
        Prefdlg->setSizePolicy(sizePolicy);
        Prefdlg->setModal(true);
        closeBtn = new QPushButton(Prefdlg);
        buttonGroup = new QButtonGroup(Prefdlg);
        buttonGroup->setObjectName(QString::fromUtf8("buttonGroup"));
        buttonGroup->addButton(closeBtn);
        closeBtn->setObjectName(QString::fromUtf8("closeBtn"));
        closeBtn->setGeometry(QRect(570, 530, 121, 31));
        stackedWidget = new QStackedWidget(Prefdlg);
        stackedWidget->setObjectName(QString::fromUtf8("stackedWidget"));
        stackedWidget->setGeometry(QRect(200, 20, 491, 491));
        QFont font;
        font.setPointSize(12);
        stackedWidget->setFont(font);
        stackedWidget->setFrameShape(QFrame::Panel);
        pageGeneral = new QWidget();
        pageGeneral->setObjectName(QString::fromUtf8("pageGeneral"));
        scrollArea = new QScrollArea(pageGeneral);
        scrollArea->setObjectName(QString::fromUtf8("scrollArea"));
        scrollArea->setGeometry(QRect(0, 0, 491, 751));
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 489, 749));
        groupBoxFolders = new QGroupBox(scrollAreaWidgetContents);
        groupBoxFolders->setObjectName(QString::fromUtf8("groupBoxFolders"));
        groupBoxFolders->setGeometry(QRect(20, 20, 431, 111));
        rememberFolderChk = new QCheckBox(groupBoxFolders);
        rememberFolderChk->setObjectName(QString::fromUtf8("rememberFolderChk"));
        rememberFolderChk->setGeometry(QRect(20, 30, 261, 20));
        rememberFolderChk->setFont(font);
        updateAppChk = new QCheckBox(groupBoxFolders);
        updateAppChk->setObjectName(QString::fromUtf8("updateAppChk"));
        updateAppChk->setGeometry(QRect(20, 60, 261, 20));
        updateAppChk->setFont(font);
        groupBoxTrackpadAction = new QGroupBox(scrollAreaWidgetContents);
        groupBoxTrackpadAction->setObjectName(QString::fromUtf8("groupBoxTrackpadAction"));
        groupBoxTrackpadAction->setGeometry(QRect(20, 150, 431, 101));
        layoutWidget = new QWidget(groupBoxTrackpadAction);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(20, 27, 337, 65));
        verticalLayout_3 = new QVBoxLayout(layoutWidget);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        verticalLayout_3->setContentsMargins(0, 0, 0, 0);
        trackpadIterateRadio = new QRadioButton(layoutWidget);
        trackpadIterateRadio->setObjectName(QString::fromUtf8("trackpadIterateRadio"));
        trackpadIterateRadio->setFont(font);

        verticalLayout_3->addWidget(trackpadIterateRadio);

        trackpadScrollRadio = new QRadioButton(layoutWidget);
        trackpadScrollRadio->setObjectName(QString::fromUtf8("trackpadScrollRadio"));
        trackpadScrollRadio->setFont(font);

        verticalLayout_3->addWidget(trackpadScrollRadio);

        label_23 = new QLabel(scrollAreaWidgetContents);
        label_23->setObjectName(QString::fromUtf8("label_23"));
        label_23->setGeometry(QRect(30, 580, 47, 13));
        label_23->setFont(font);
        scrollArea->setWidget(scrollAreaWidgetContents);
        stackedWidget->addWidget(pageGeneral);
        pageAppearance = new QWidget();
        pageAppearance->setObjectName(QString::fromUtf8("pageAppearance"));
        groupBoxFontSize = new QGroupBox(pageAppearance);
        groupBoxFontSize->setObjectName(QString::fromUtf8("groupBoxFontSize"));
        groupBoxFontSize->setGeometry(QRect(20, 20, 431, 111));
        globalFontSizeSlider = new QSlider(groupBoxFontSize);
        globalFontSizeSlider->setObjectName(QString::fromUtf8("globalFontSizeSlider"));
        globalFontSizeSlider->setGeometry(QRect(130, 40, 251, 22));
        globalFontSizeSlider->setMinimum(8);
        globalFontSizeSlider->setMaximum(20);
        globalFontSizeSlider->setPageStep(1);
        globalFontSizeSlider->setValue(14);
        globalFontSizeSlider->setOrientation(Qt::Horizontal);
        globalFontSizeSlider->setTickPosition(QSlider::TicksAbove);
        globalFontSizeSlider->setTickInterval(1);
        globalFontSizeLbl = new QLabel(groupBoxFontSize);
        globalFontSizeLbl->setObjectName(QString::fromUtf8("globalFontSizeLbl"));
        globalFontSizeLbl->setGeometry(QRect(385, 40, 41, 20));
        infoFontSizeSlider = new QSlider(groupBoxFontSize);
        infoFontSizeSlider->setObjectName(QString::fromUtf8("infoFontSizeSlider"));
        infoFontSizeSlider->setGeometry(QRect(130, 70, 251, 22));
        infoFontSizeSlider->setMinimum(8);
        infoFontSizeSlider->setMaximum(30);
        infoFontSizeSlider->setPageStep(1);
        infoFontSizeSlider->setValue(14);
        infoFontSizeSlider->setOrientation(Qt::Horizontal);
        infoFontSizeSlider->setTickPosition(QSlider::TicksAbove);
        infoFontSizeSlider->setTickInterval(1);
        label_21 = new QLabel(groupBoxFontSize);
        label_21->setObjectName(QString::fromUtf8("label_21"));
        label_21->setGeometry(QRect(10, 40, 81, 20));
        QFont font1;
        font1.setPointSize(10);
        label_21->setFont(font1);
        label_22 = new QLabel(groupBoxFontSize);
        label_22->setObjectName(QString::fromUtf8("label_22"));
        label_22->setGeometry(QRect(10, 70, 111, 20));
        label_22->setFont(font1);
        infoFontSizeLbl = new QLabel(groupBoxFontSize);
        infoFontSizeLbl->setObjectName(QString::fromUtf8("infoFontSizeLbl"));
        infoFontSizeLbl->setGeometry(QRect(385, 70, 41, 20));
        groupBoxClassificationSize = new QGroupBox(pageAppearance);
        groupBoxClassificationSize->setObjectName(QString::fromUtf8("groupBoxClassificationSize"));
        groupBoxClassificationSize->setGeometry(QRect(20, 150, 431, 121));
        classificationBadgeImageDiamSlider = new QSlider(groupBoxClassificationSize);
        classificationBadgeImageDiamSlider->setObjectName(QString::fromUtf8("classificationBadgeImageDiamSlider"));
        classificationBadgeImageDiamSlider->setGeometry(QRect(130, 40, 251, 22));
        classificationBadgeImageDiamSlider->setMinimum(0);
        classificationBadgeImageDiamSlider->setMaximum(100);
        classificationBadgeImageDiamSlider->setPageStep(10);
        classificationBadgeImageDiamSlider->setValue(20);
        classificationBadgeImageDiamSlider->setOrientation(Qt::Horizontal);
        classificationBadgeImageDiamSlider->setTickPosition(QSlider::TicksAbove);
        classificationBadgeImageDiamSlider->setTickInterval(10);
        classificationBadgeImageDiamLbl = new QLabel(groupBoxClassificationSize);
        classificationBadgeImageDiamLbl->setObjectName(QString::fromUtf8("classificationBadgeImageDiamLbl"));
        classificationBadgeImageDiamLbl->setGeometry(QRect(385, 40, 41, 20));
        label_13 = new QLabel(groupBoxClassificationSize);
        label_13->setObjectName(QString::fromUtf8("label_13"));
        label_13->setGeometry(QRect(10, 40, 81, 20));
        label_13->setFont(font1);
        label_20 = new QLabel(groupBoxClassificationSize);
        label_20->setObjectName(QString::fromUtf8("label_20"));
        label_20->setGeometry(QRect(10, 70, 81, 20));
        label_20->setFont(font1);
        classificationBadgeThumbDiamLbl = new QLabel(groupBoxClassificationSize);
        classificationBadgeThumbDiamLbl->setObjectName(QString::fromUtf8("classificationBadgeThumbDiamLbl"));
        classificationBadgeThumbDiamLbl->setGeometry(QRect(385, 70, 41, 20));
        classificationBadgeThumbDiamSlider = new QSlider(groupBoxClassificationSize);
        classificationBadgeThumbDiamSlider->setObjectName(QString::fromUtf8("classificationBadgeThumbDiamSlider"));
        classificationBadgeThumbDiamSlider->setGeometry(QRect(130, 70, 251, 22));
        classificationBadgeThumbDiamSlider->setMinimum(0);
        classificationBadgeThumbDiamSlider->setMaximum(50);
        classificationBadgeThumbDiamSlider->setPageStep(5);
        classificationBadgeThumbDiamSlider->setValue(12);
        classificationBadgeThumbDiamSlider->setOrientation(Qt::Horizontal);
        classificationBadgeThumbDiamSlider->setTickPosition(QSlider::TicksAbove);
        classificationBadgeThumbDiamSlider->setTickInterval(5);
        stackedWidget->addWidget(pageAppearance);
        pageCache = new QWidget();
        pageCache->setObjectName(QString::fromUtf8("pageCache"));
        label_9 = new QLabel(pageCache);
        label_9->setObjectName(QString::fromUtf8("label_9"));
        label_9->setGeometry(QRect(260, 24, 91, 0));
        QFont font2;
        font2.setPointSize(9);
        label_9->setFont(font2);
        label_9->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        cacheDelaySpinbox = new QSpinBox(pageCache);
        cacheDelaySpinbox->setObjectName(QString::fromUtf8("cacheDelaySpinbox"));
        cacheDelaySpinbox->setGeometry(QRect(360, 23, 71, 0));
        cacheDelaySpinbox->setFont(font2);
        cacheDelaySpinbox->setMinimum(0);
        cacheDelaySpinbox->setMaximum(500);
        cacheDelaySpinbox->setSingleStep(25);
        cacheDelaySpinbox->setValue(250);
        groupBox_3 = new QGroupBox(pageCache);
        groupBox_3->setObjectName(QString::fromUtf8("groupBox_3"));
        groupBox_3->setGeometry(QRect(20, 240, 451, 221));
        groupBox_3->setFont(font2);
        showCacheStatusChk = new QCheckBox(groupBox_3);
        showCacheStatusChk->setObjectName(QString::fromUtf8("showCacheStatusChk"));
        showCacheStatusChk->setGeometry(QRect(20, 189, 191, 21));
        showCacheStatusChk->setFont(font2);
        label_10 = new QLabel(groupBox_3);
        label_10->setObjectName(QString::fromUtf8("label_10"));
        label_10->setGeometry(QRect(210, 190, 41, 20));
        label_10->setFont(font2);
        label_10->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        progressWidthSlider = new QSlider(groupBox_3);
        progressWidthSlider->setObjectName(QString::fromUtf8("progressWidthSlider"));
        progressWidthSlider->setGeometry(QRect(260, 190, 181, 24));
        progressWidthSlider->setMinimum(100);
        progressWidthSlider->setMaximum(1000);
        progressWidthSlider->setSingleStep(100);
        progressWidthSlider->setPageStep(200);
        progressWidthSlider->setValue(200);
        progressWidthSlider->setOrientation(Qt::Horizontal);
        progressWidthSlider->setTickPosition(QSlider::TicksAbove);
        progressWidthSlider->setTickInterval(100);
        showCacheThreadActivityChk = new QCheckBox(groupBox_3);
        showCacheThreadActivityChk->setObjectName(QString::fromUtf8("showCacheThreadActivityChk"));
        showCacheThreadActivityChk->setGeometry(QRect(20, 169, 231, 17));
        showCacheThreadActivityChk->setFont(font2);
        cachePreviewsChk = new QCheckBox(groupBox_3);
        cachePreviewsChk->setObjectName(QString::fromUtf8("cachePreviewsChk"));
        cachePreviewsChk->setGeometry(QRect(20, 144, 341, 20));
        cachePreviewsChk->setFont(font2);
        cache100AheadRadio = new QRadioButton(groupBox_3);
        cache100AheadRadio->setObjectName(QString::fromUtf8("cache100AheadRadio"));
        cache100AheadRadio->setGeometry(QRect(320, 110, 129, 17));
        cache100AheadRadio->setFont(font2);
        cache70AheadRadio = new QRadioButton(groupBox_3);
        cache70AheadRadio->setObjectName(QString::fromUtf8("cache70AheadRadio"));
        cache70AheadRadio->setGeometry(QRect(320, 85, 129, 17));
        cache70AheadRadio->setFont(font2);
        cache90AheadRadio = new QRadioButton(groupBox_3);
        cache90AheadRadio->setObjectName(QString::fromUtf8("cache90AheadRadio"));
        cache90AheadRadio->setGeometry(QRect(180, 110, 129, 17));
        cache90AheadRadio->setFont(font2);
        cache50AheadRadio = new QRadioButton(groupBox_3);
        cache50AheadRadio->setObjectName(QString::fromUtf8("cache50AheadRadio"));
        cache50AheadRadio->setGeometry(QRect(40, 85, 129, 18));
        cache50AheadRadio->setFont(font2);
        cache60AheadRadio = new QRadioButton(groupBox_3);
        cache60AheadRadio->setObjectName(QString::fromUtf8("cache60AheadRadio"));
        cache60AheadRadio->setGeometry(QRect(180, 85, 129, 17));
        cache60AheadRadio->setFont(font2);
        cache80AheadRadio = new QRadioButton(groupBox_3);
        cache80AheadRadio->setObjectName(QString::fromUtf8("cache80AheadRadio"));
        cache80AheadRadio->setGeometry(QRect(40, 110, 129, 18));
        cache80AheadRadio->setFont(font2);
        cacheSizeSpinbox = new QSpinBox(groupBox_3);
        cacheSizeSpinbox->setObjectName(QString::fromUtf8("cacheSizeSpinbox"));
        cacheSizeSpinbox->setGeometry(QRect(130, 30, 51, 18));
        cacheSizeSpinbox->setFont(font2);
        cacheSizeSpinbox->setMinimum(1);
        cacheSizeSpinbox->setMaximum(64);
        label_8 = new QLabel(groupBox_3);
        label_8->setObjectName(QString::fromUtf8("label_8"));
        label_8->setGeometry(QRect(20, 30, 91, 16));
        label_8->setFont(font2);
        label_8->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        label = new QLabel(groupBox_3);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(20, 60, 111, 16));
        label->setFont(font2);
        label->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        metadataCacheGroupBox = new QGroupBox(pageCache);
        metadataCacheGroupBox->setObjectName(QString::fromUtf8("metadataCacheGroupBox"));
        metadataCacheGroupBox->setGeometry(QRect(20, 20, 451, 81));
        metadataCacheGroupBox->setFont(font2);
        metadataCacheAllRadio = new QRadioButton(metadataCacheGroupBox);
        metadataCacheAllRadio->setObjectName(QString::fromUtf8("metadataCacheAllRadio"));
        metadataCacheAllRadio->setGeometry(QRect(20, 25, 411, 20));
        metadataCacheAllRadio->setFont(font2);
        metadataCacheChunkRadio = new QRadioButton(metadataCacheGroupBox);
        metadataCacheChunkRadio->setObjectName(QString::fromUtf8("metadataCacheChunkRadio"));
        metadataCacheChunkRadio->setGeometry(QRect(20, 50, 71, 20));
        metadataCacheChunkRadio->setFont(font2);
        metadataCacheSizeSpinbox = new QSpinBox(metadataCacheGroupBox);
        metadataCacheSizeSpinbox->setObjectName(QString::fromUtf8("metadataCacheSizeSpinbox"));
        metadataCacheSizeSpinbox->setGeometry(QRect(90, 51, 51, 18));
        metadataCacheSizeSpinbox->setFont(font2);
        metadataCacheSizeSpinbox->setMinimum(100);
        metadataCacheSizeSpinbox->setMaximum(1000000);
        metadataCacheSizeSpinbox->setValue(250);
        label_14 = new QLabel(metadataCacheGroupBox);
        label_14->setObjectName(QString::fromUtf8("label_14"));
        label_14->setGeometry(QRect(150, 51, 251, 16));
        label_14->setFont(font2);
        thumbnailCacheGroupBoc = new QGroupBox(pageCache);
        thumbnailCacheGroupBoc->setObjectName(QString::fromUtf8("thumbnailCacheGroupBoc"));
        thumbnailCacheGroupBoc->setGeometry(QRect(20, 120, 451, 101));
        thumbnailCacheGroupBoc->setFont(font2);
        thumbnailCachAllRadio = new QRadioButton(thumbnailCacheGroupBoc);
        thumbnailCachAllRadio->setObjectName(QString::fromUtf8("thumbnailCachAllRadio"));
        thumbnailCachAllRadio->setGeometry(QRect(20, 25, 21, 21));
        thumbnailCachAllRadio->setFont(font2);
        thumbnailCacheChunkRadio = new QRadioButton(thumbnailCacheGroupBoc);
        thumbnailCacheChunkRadio->setObjectName(QString::fromUtf8("thumbnailCacheChunkRadio"));
        thumbnailCacheChunkRadio->setGeometry(QRect(20, 69, 71, 20));
        thumbnailCacheChunkRadio->setFont(font2);
        thumbnailCacheSizeSpinbox = new QSpinBox(thumbnailCacheGroupBoc);
        thumbnailCacheSizeSpinbox->setObjectName(QString::fromUtf8("thumbnailCacheSizeSpinbox"));
        thumbnailCacheSizeSpinbox->setGeometry(QRect(90, 70, 51, 18));
        thumbnailCacheSizeSpinbox->setFont(font2);
        thumbnailCacheSizeSpinbox->setMinimum(1);
        thumbnailCacheSizeSpinbox->setMaximum(1000);
        label_15 = new QLabel(thumbnailCacheGroupBoc);
        label_15->setObjectName(QString::fromUtf8("label_15"));
        label_15->setGeometry(QRect(150, 70, 251, 16));
        label_15->setFont(font2);
        label_18 = new QLabel(thumbnailCacheGroupBoc);
        label_18->setObjectName(QString::fromUtf8("label_18"));
        label_18->setGeometry(QRect(43, 26, 401, 41));
        label_18->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        label_18->setWordWrap(true);
        stackedWidget->addWidget(pageCache);
        pageSlideshow = new QWidget();
        pageSlideshow->setObjectName(QString::fromUtf8("pageSlideshow"));
        slideshowRandomChk = new QCheckBox(pageSlideshow);
        slideshowRandomChk->setObjectName(QString::fromUtf8("slideshowRandomChk"));
        slideshowRandomChk->setGeometry(QRect(200, 80, 19, 18));
        slideshowRandomChk->setFont(font);
        label_12 = new QLabel(pageSlideshow);
        label_12->setObjectName(QString::fromUtf8("label_12"));
        label_12->setGeometry(QRect(20, 80, 161, 16));
        label_12->setFont(font);
        slideshowDelaySpinbox = new QSpinBox(pageSlideshow);
        slideshowDelaySpinbox->setObjectName(QString::fromUtf8("slideshowDelaySpinbox"));
        slideshowDelaySpinbox->setGeometry(QRect(200, 40, 48, 18));
        slideshowDelaySpinbox->setFont(font);
        slideshowDelaySpinbox->setMaximum(3600);
        label_11 = new QLabel(pageSlideshow);
        label_11->setObjectName(QString::fromUtf8("label_11"));
        label_11->setGeometry(QRect(20, 40, 171, 18));
        label_11->setFont(font);
        stackedWidget->addWidget(pageSlideshow);
        pageThumbnails = new QWidget();
        pageThumbnails->setObjectName(QString::fromUtf8("pageThumbnails"));
        groupBox = new QGroupBox(pageThumbnails);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        groupBox->setGeometry(QRect(10, 20, 461, 121));
        showThumbLabelChk = new QCheckBox(groupBox);
        showThumbLabelChk->setObjectName(QString::fromUtf8("showThumbLabelChk"));
        showThumbLabelChk->setGeometry(QRect(20, 60, 221, 17));
        showThumbLabelChk->setFont(font2);
        label_7 = new QLabel(groupBox);
        label_7->setObjectName(QString::fromUtf8("label_7"));
        label_7->setGeometry(QRect(50, 80, 141, 16));
        label_7->setFont(font2);
        label_7->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        thumbSmallerBtn = new QPushButton(groupBox);
        thumbSmallerBtn->setObjectName(QString::fromUtf8("thumbSmallerBtn"));
        thumbSmallerBtn->setGeometry(QRect(200, 30, 40, 20));
        sizePolicy.setHeightForWidth(thumbSmallerBtn->sizePolicy().hasHeightForWidth());
        thumbSmallerBtn->setSizePolicy(sizePolicy);
        thumbSmallerBtn->setMinimumSize(QSize(0, 7));
        thumbSmallerBtn->setMaximumSize(QSize(400, 20));
        thumbSmallerBtn->setFont(font2);
        thumbSmallerBtn->setAutoRepeat(true);
        thumbSmallerBtn->setAutoDefault(false);
        fontSizeLbl = new QLabel(groupBox);
        fontSizeLbl->setObjectName(QString::fromUtf8("fontSizeLbl"));
        fontSizeLbl->setGeometry(QRect(420, 80, 30, 16));
        fontSizeLbl->setFont(font);
        fontSizeLbl->setFrameShape(QFrame::NoFrame);
        fontSizeLbl->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        label_2 = new QLabel(groupBox);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setGeometry(QRect(20, 30, 121, 16));
        label_2->setFont(font2);
        fontSizeSlider = new QSlider(groupBox);
        fontSizeSlider->setObjectName(QString::fromUtf8("fontSizeSlider"));
        fontSizeSlider->setGeometry(QRect(200, 80, 211, 22));
        fontSizeSlider->setFont(font);
        fontSizeSlider->setMinimum(6);
        fontSizeSlider->setMaximum(20);
        fontSizeSlider->setSingleStep(1);
        fontSizeSlider->setPageStep(5);
        fontSizeSlider->setValue(6);
        fontSizeSlider->setSliderPosition(6);
        fontSizeSlider->setOrientation(Qt::Horizontal);
        fontSizeSlider->setInvertedAppearance(false);
        fontSizeSlider->setTickPosition(QSlider::NoTicks);
        fontSizeSlider->setTickInterval(5);
        thumbLargerBtn = new QPushButton(groupBox);
        thumbLargerBtn->setObjectName(QString::fromUtf8("thumbLargerBtn"));
        thumbLargerBtn->setGeometry(QRect(260, 30, 40, 20));
        sizePolicy.setHeightForWidth(thumbLargerBtn->sizePolicy().hasHeightForWidth());
        thumbLargerBtn->setSizePolicy(sizePolicy);
        thumbLargerBtn->setMinimumSize(QSize(0, 7));
        thumbLargerBtn->setMaximumSize(QSize(400, 20));
        thumbLargerBtn->setFont(font2);
        thumbLargerBtn->setAutoRepeat(true);
        thumbLargerBtn->setAutoDefault(false);
        groupBox_2 = new QGroupBox(pageThumbnails);
        groupBox_2->setObjectName(QString::fromUtf8("groupBox_2"));
        groupBox_2->setGeometry(QRect(10, 160, 461, 121));
        fontSizeLbl_2 = new QLabel(groupBox_2);
        fontSizeLbl_2->setObjectName(QString::fromUtf8("fontSizeLbl_2"));
        fontSizeLbl_2->setGeometry(QRect(420, 80, 30, 16));
        fontSizeLbl_2->setFont(font);
        fontSizeLbl_2->setFrameShape(QFrame::NoFrame);
        fontSizeLbl_2->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        label_17 = new QLabel(groupBox_2);
        label_17->setObjectName(QString::fromUtf8("label_17"));
        label_17->setGeometry(QRect(20, 30, 131, 16));
        label_17->setFont(font2);
        gridSmallerBtn = new QPushButton(groupBox_2);
        gridSmallerBtn->setObjectName(QString::fromUtf8("gridSmallerBtn"));
        gridSmallerBtn->setGeometry(QRect(200, 30, 40, 20));
        sizePolicy.setHeightForWidth(gridSmallerBtn->sizePolicy().hasHeightForWidth());
        gridSmallerBtn->setSizePolicy(sizePolicy);
        gridSmallerBtn->setMinimumSize(QSize(0, 7));
        gridSmallerBtn->setMaximumSize(QSize(400, 20));
        gridSmallerBtn->setFont(font2);
        gridSmallerBtn->setAutoRepeat(true);
        gridSmallerBtn->setAutoDefault(false);
        label_19 = new QLabel(groupBox_2);
        label_19->setObjectName(QString::fromUtf8("label_19"));
        label_19->setGeometry(QRect(50, 80, 141, 20));
        label_19->setFont(font2);
        label_19->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        fontSizeSlider_2 = new QSlider(groupBox_2);
        fontSizeSlider_2->setObjectName(QString::fromUtf8("fontSizeSlider_2"));
        fontSizeSlider_2->setGeometry(QRect(200, 80, 211, 22));
        fontSizeSlider_2->setFont(font);
        fontSizeSlider_2->setMinimum(6);
        fontSizeSlider_2->setMaximum(20);
        fontSizeSlider_2->setSingleStep(1);
        fontSizeSlider_2->setPageStep(5);
        fontSizeSlider_2->setValue(6);
        fontSizeSlider_2->setSliderPosition(6);
        fontSizeSlider_2->setOrientation(Qt::Horizontal);
        fontSizeSlider_2->setInvertedAppearance(false);
        fontSizeSlider_2->setTickPosition(QSlider::NoTicks);
        fontSizeSlider_2->setTickInterval(5);
        gridLargerBtn = new QPushButton(groupBox_2);
        gridLargerBtn->setObjectName(QString::fromUtf8("gridLargerBtn"));
        gridLargerBtn->setGeometry(QRect(260, 30, 40, 20));
        sizePolicy.setHeightForWidth(gridLargerBtn->sizePolicy().hasHeightForWidth());
        gridLargerBtn->setSizePolicy(sizePolicy);
        gridLargerBtn->setMinimumSize(QSize(0, 7));
        gridLargerBtn->setMaximumSize(QSize(400, 20));
        gridLargerBtn->setFont(font2);
        gridLargerBtn->setAutoRepeat(true);
        gridLargerBtn->setAutoDefault(false);
        showThumbLabelChk_2 = new QCheckBox(groupBox_2);
        showThumbLabelChk_2->setObjectName(QString::fromUtf8("showThumbLabelChk_2"));
        showThumbLabelChk_2->setGeometry(QRect(20, 60, 201, 17));
        showThumbLabelChk_2->setFont(font2);
        label_3 = new QLabel(pageThumbnails);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setGeometry(QRect(30, 380, 171, 22));
        label_3->setFont(font2);
        label_3->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        fontSizeLbl_21 = new QLabel(label_3);
        fontSizeLbl_21->setObjectName(QString::fromUtf8("fontSizeLbl_21"));
        fontSizeLbl_21->setGeometry(QRect(420, 80, 30, 16));
        fontSizeLbl_21->setFont(font);
        fontSizeLbl_21->setFrameShape(QFrame::NoFrame);
        fontSizeLbl_21->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        label_171 = new QLabel(label_3);
        label_171->setObjectName(QString::fromUtf8("label_171"));
        label_171->setGeometry(QRect(20, 30, 131, 16));
        label_171->setFont(font2);
        gridSmallerBtn1 = new QPushButton(label_3);
        gridSmallerBtn1->setObjectName(QString::fromUtf8("gridSmallerBtn1"));
        gridSmallerBtn1->setGeometry(QRect(200, 30, 40, 20));
        sizePolicy.setHeightForWidth(gridSmallerBtn1->sizePolicy().hasHeightForWidth());
        gridSmallerBtn1->setSizePolicy(sizePolicy);
        gridSmallerBtn1->setMinimumSize(QSize(0, 7));
        gridSmallerBtn1->setMaximumSize(QSize(400, 20));
        gridSmallerBtn1->setFont(font2);
        gridSmallerBtn1->setAutoRepeat(true);
        gridSmallerBtn1->setAutoDefault(false);
        label_191 = new QLabel(label_3);
        label_191->setObjectName(QString::fromUtf8("label_191"));
        label_191->setGeometry(QRect(50, 80, 141, 20));
        label_191->setFont(font2);
        label_191->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        fontSizeSlider_21 = new QSlider(label_3);
        fontSizeSlider_21->setObjectName(QString::fromUtf8("fontSizeSlider_21"));
        fontSizeSlider_21->setGeometry(QRect(200, 80, 211, 22));
        fontSizeSlider_21->setFont(font);
        fontSizeSlider_21->setMinimum(6);
        fontSizeSlider_21->setMaximum(20);
        fontSizeSlider_21->setSingleStep(1);
        fontSizeSlider_21->setPageStep(5);
        fontSizeSlider_21->setValue(6);
        fontSizeSlider_21->setSliderPosition(6);
        fontSizeSlider_21->setOrientation(Qt::Horizontal);
        fontSizeSlider_21->setInvertedAppearance(false);
        fontSizeSlider_21->setTickPosition(QSlider::NoTicks);
        fontSizeSlider_21->setTickInterval(5);
        gridLargerBtn1 = new QPushButton(label_3);
        gridLargerBtn1->setObjectName(QString::fromUtf8("gridLargerBtn1"));
        gridLargerBtn1->setGeometry(QRect(260, 30, 40, 20));
        sizePolicy.setHeightForWidth(gridLargerBtn1->sizePolicy().hasHeightForWidth());
        gridLargerBtn1->setSizePolicy(sizePolicy);
        gridLargerBtn1->setMinimumSize(QSize(0, 7));
        gridLargerBtn1->setMaximumSize(QSize(400, 20));
        gridLargerBtn1->setFont(font2);
        gridLargerBtn1->setAutoRepeat(true);
        gridLargerBtn1->setAutoDefault(false);
        showThumbLabelChk_21 = new QCheckBox(label_3);
        showThumbLabelChk_21->setObjectName(QString::fromUtf8("showThumbLabelChk_21"));
        showThumbLabelChk_21->setGeometry(QRect(20, 60, 201, 17));
        showThumbLabelChk_21->setFont(font2);
        maxIconSizeSpinBox = new QSpinBox(pageThumbnails);
        maxIconSizeSpinBox->setObjectName(QString::fromUtf8("maxIconSizeSpinBox"));
        maxIconSizeSpinBox->setGeometry(QRect(210, 380, 41, 22));
        maxIconSizeSpinBox->setFont(font2);
        maxIconSizeSpinBox->setMinimum(40);
        maxIconSizeSpinBox->setMaximum(480);
        maxIconSizeSpinBox->setValue(256);
        label_4 = new QLabel(pageThumbnails);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setGeometry(QRect(30, 410, 441, 71));
        label_4->setFont(font2);
        label_4->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        label_4->setWordWrap(true);
        label_5 = new QLabel(pageThumbnails);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setGeometry(QRect(260, 380, 141, 22));
        label_5->setFont(font2);
        stackedWidget->addWidget(pageThumbnails);
        pageFullScreen = new QWidget();
        pageFullScreen->setObjectName(QString::fromUtf8("pageFullScreen"));
        fullScreenExplanationLbl = new QLabel(pageFullScreen);
        fullScreenExplanationLbl->setObjectName(QString::fromUtf8("fullScreenExplanationLbl"));
        fullScreenExplanationLbl->setGeometry(QRect(20, 30, 421, 61));
        fullScreenExplanationLbl->setFont(font2);
        fullScreenExplanationLbl->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        fullScreenExplanationLbl->setWordWrap(true);
        layoutWidget1 = new QWidget(pageFullScreen);
        layoutWidget1->setObjectName(QString::fromUtf8("layoutWidget1"));
        layoutWidget1->setGeometry(QRect(20, 110, 221, 205));
        layoutWidget1->setFont(font);
        verticalLayout_2 = new QVBoxLayout(layoutWidget1);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        foldersChk = new QCheckBox(layoutWidget1);
        foldersChk->setObjectName(QString::fromUtf8("foldersChk"));
        foldersChk->setFont(font2);

        verticalLayout_2->addWidget(foldersChk);

        favsChk = new QCheckBox(layoutWidget1);
        favsChk->setObjectName(QString::fromUtf8("favsChk"));
        favsChk->setFont(font2);

        verticalLayout_2->addWidget(favsChk);

        filtersChk = new QCheckBox(layoutWidget1);
        filtersChk->setObjectName(QString::fromUtf8("filtersChk"));
        filtersChk->setFont(font2);

        verticalLayout_2->addWidget(filtersChk);

        metadataChk = new QCheckBox(layoutWidget1);
        metadataChk->setObjectName(QString::fromUtf8("metadataChk"));
        metadataChk->setFont(font2);

        verticalLayout_2->addWidget(metadataChk);

        thumbsChk = new QCheckBox(layoutWidget1);
        thumbsChk->setObjectName(QString::fromUtf8("thumbsChk"));
        thumbsChk->setFont(font2);

        verticalLayout_2->addWidget(thumbsChk);

        statusBarChk = new QCheckBox(layoutWidget1);
        statusBarChk->setObjectName(QString::fromUtf8("statusBarChk"));
        statusBarChk->setFont(font2);

        verticalLayout_2->addWidget(statusBarChk);

        stackedWidget->addWidget(pageFullScreen);
        pageShowHideFields = new QWidget();
        pageShowHideFields->setObjectName(QString::fromUtf8("pageShowHideFields"));
        frame_2 = new QFrame(pageShowHideFields);
        frame_2->setObjectName(QString::fromUtf8("frame_2"));
        frame_2->setGeometry(QRect(10, 10, 460, 481));
        frame_2->setFrameShape(QFrame::NoFrame);
        frame_2->setFrameShadow(QFrame::Raised);
        frame = new QFrame(frame_2);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setGeometry(QRect(20, 0, 211, 471));
        frame->setFrameShape(QFrame::NoFrame);
        frame->setFrameShadow(QFrame::Raised);
        layoutWidget2 = new QWidget(frame);
        layoutWidget2->setObjectName(QString::fromUtf8("layoutWidget2"));
        layoutWidget2->setGeometry(QRect(0, 0, 198, 471));
        verticalLayout_4 = new QVBoxLayout(layoutWidget2);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        verticalLayout_4->setContentsMargins(0, 0, 0, 0);
        label_6 = new QLabel(layoutWidget2);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        verticalLayout_4->addWidget(label_6);

        infoFieldsTreeView = new QTreeView(layoutWidget2);
        infoFieldsTreeView->setObjectName(QString::fromUtf8("infoFieldsTreeView"));
        infoFieldsTreeView->setMinimumSize(QSize(0, 0));
        infoFieldsTreeView->setMaximumSize(QSize(16777215, 16777215));
        infoFieldsTreeView->setStyleSheet(QString::fromUtf8(""));
        infoFieldsTreeView->setFrameShape(QFrame::StyledPanel);
        infoFieldsTreeView->setFrameShadow(QFrame::Plain);
        infoFieldsTreeView->setLineWidth(0);
        infoFieldsTreeView->setIconSize(QSize(1, 0));
        infoFieldsTreeView->setHeaderHidden(true);

        verticalLayout_4->addWidget(infoFieldsTreeView);

        frame_3 = new QFrame(frame_2);
        frame_3->setObjectName(QString::fromUtf8("frame_3"));
        frame_3->setGeometry(QRect(240, 0, 221, 471));
        frame_3->setFrameShape(QFrame::NoFrame);
        frame_3->setFrameShadow(QFrame::Raised);
        layoutWidget3 = new QWidget(frame_3);
        layoutWidget3->setObjectName(QString::fromUtf8("layoutWidget3"));
        layoutWidget3->setGeometry(QRect(10, 0, 201, 471));
        verticalLayout_5 = new QVBoxLayout(layoutWidget3);
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        verticalLayout_5->setContentsMargins(0, 0, 0, 0);
        label_16 = new QLabel(layoutWidget3);
        label_16->setObjectName(QString::fromUtf8("label_16"));

        verticalLayout_5->addWidget(label_16);

        tableFieldsTable = new QTableView(layoutWidget3);
        tableFieldsTable->setObjectName(QString::fromUtf8("tableFieldsTable"));
        tableFieldsTable->setMaximumSize(QSize(200, 16777215));
        tableFieldsTable->setIconSize(QSize(0, 0));
        tableFieldsTable->verticalHeader()->setMinimumSectionSize(1);
        tableFieldsTable->verticalHeader()->setDefaultSectionSize(34);

        verticalLayout_5->addWidget(tableFieldsTable);

        stackedWidget->addWidget(pageShowHideFields);
        listContainer = new QStackedWidget(Prefdlg);
        listContainer->setObjectName(QString::fromUtf8("listContainer"));
        listContainer->setGeometry(QRect(10, 20, 191, 491));
        listContainer->setFrameShape(QFrame::Box);
        page = new QWidget();
        page->setObjectName(QString::fromUtf8("page"));
        listWidget = new QListWidget(page);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        listWidget->setObjectName(QString::fromUtf8("listWidget"));
        listWidget->setGeometry(QRect(0, 9, 191, 481));
        listWidget->setStyleSheet(QString::fromUtf8("border: none;"));
        listWidget->setFrameShape(QFrame::NoFrame);
        listWidget->setFrameShadow(QFrame::Plain);
        listWidget->setSpacing(4);
        listContainer->addWidget(page);
        page_2 = new QWidget();
        page_2->setObjectName(QString::fromUtf8("page_2"));
        listContainer->addWidget(page_2);

        retranslateUi(Prefdlg);
        QObject::connect(closeBtn, SIGNAL(clicked()), Prefdlg, SLOT(reject()));
        QObject::connect(fontSizeSlider, SIGNAL(valueChanged(int)), fontSizeLbl, SLOT(setNum(int)));
        QObject::connect(classificationBadgeImageDiamSlider, SIGNAL(valueChanged(int)), classificationBadgeImageDiamLbl, SLOT(setNum(int)));
        QObject::connect(classificationBadgeThumbDiamSlider, SIGNAL(valueChanged(int)), classificationBadgeThumbDiamLbl, SLOT(setNum(int)));
        QObject::connect(globalFontSizeSlider, SIGNAL(valueChanged(int)), globalFontSizeLbl, SLOT(setNum(int)));
        QObject::connect(fontSizeSlider_2, SIGNAL(valueChanged(int)), fontSizeLbl_2, SLOT(setNum(int)));

        stackedWidget->setCurrentIndex(0);
        listContainer->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(Prefdlg);
    } // setupUi

    void retranslateUi(QDialog *Prefdlg)
    {
        Prefdlg->setWindowTitle(QCoreApplication::translate("Prefdlg", "Winnow Preferences", nullptr));
#if QT_CONFIG(tooltip)
        closeBtn->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>When closing the preferences dialog all changes will by retained.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        closeBtn->setText(QCoreApplication::translate("Prefdlg", "Okay", nullptr));
        groupBoxFolders->setTitle(QCoreApplication::translate("Prefdlg", "When open program", nullptr));
        rememberFolderChk->setText(QCoreApplication::translate("Prefdlg", "Remember previous folder", nullptr));
        updateAppChk->setText(QCoreApplication::translate("Prefdlg", "Check for program updates", nullptr));
        groupBoxTrackpadAction->setTitle(QCoreApplication::translate("Prefdlg", "Trackpad Swipe / Mouse Wheel", nullptr));
        trackpadIterateRadio->setText(QCoreApplication::translate("Prefdlg", "Next / previous image", nullptr));
        trackpadScrollRadio->setText(QCoreApplication::translate("Prefdlg", "Scroll current image when zoomed", nullptr));
        label_23->setText(QCoreApplication::translate("Prefdlg", "Test", nullptr));
        groupBoxFontSize->setTitle(QCoreApplication::translate("Prefdlg", "Font size", nullptr));
#if QT_CONFIG(tooltip)
        globalFontSizeSlider->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Changes the global font. The font size in the thumbnails for the file name is controlled independently in &quot;Thumbs in Dock&quot; and &quot;Thumbs in Grid View&quot;.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        globalFontSizeLbl->setText(QString());
#if QT_CONFIG(tooltip)
        infoFontSizeSlider->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Changes the global font. The font size in the thumbnails for the file name is controlled independently in &quot;Thumbs in Dock&quot; and &quot;Thumbs in Grid View&quot;.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label_21->setText(QCoreApplication::translate("Prefdlg", "Global", nullptr));
        label_22->setText(QCoreApplication::translate("Prefdlg", "Info overlay", nullptr));
        infoFontSizeLbl->setText(QString());
        groupBoxClassificationSize->setTitle(QCoreApplication::translate("Prefdlg", "Classification badge diameter", nullptr));
#if QT_CONFIG(tooltip)
        classificationBadgeImageDiamSlider->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Changes the global font. The font size in the thumbnails for the file name is controlled independently in &quot;Thumbs in Dock&quot; and &quot;Thumbs in Grid View&quot;.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        classificationBadgeImageDiamLbl->setText(QString());
        label_13->setText(QCoreApplication::translate("Prefdlg", "Loupe", nullptr));
        label_20->setText(QCoreApplication::translate("Prefdlg", "Thumb", nullptr));
        classificationBadgeThumbDiamLbl->setText(QString());
#if QT_CONFIG(tooltip)
        classificationBadgeThumbDiamSlider->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Changes the global font. The font size in the thumbnails for the file name is controlled independently in &quot;Thumbs in Dock&quot; and &quot;Thumbs in Grid View&quot;.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        label_9->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Caching images and rendering the selected image simultaneously can degrade performance when rapidly moving through images.  The delay allows the next image selected to be rendered without any caching activity.  Try experimenting with the delay value if you are experiencing hesitation rendering the selected image.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label_9->setText(QCoreApplication::translate("Prefdlg", "Delay (ms)", nullptr));
#if QT_CONFIG(tooltip)
        cacheDelaySpinbox->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Caching images and rendering the selected image simultaneously can degrade performance when rapidly moving through images.  The delay allows the next image selected to be rendered without any caching activity.  Try experimenting with the delay value if you are experiencing hesitation rendering the selected image.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        groupBox_3->setTitle(QCoreApplication::translate("Prefdlg", "Full size loupe image cache", nullptr));
#if QT_CONFIG(tooltip)
        showCacheStatusChk->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>This will show the cache status in the status bar at the bottom of the application.  If you do not see the status bar you can enable it in the Windows menu or by pressing F10.  If you hover over the cache status another tooltip will explain the colour codes for the current file, target cache, current cache and total images.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        showCacheStatusChk->setText(QCoreApplication::translate("Prefdlg", "Show cache progress bar", nullptr));
#if QT_CONFIG(tooltip)
        label_10->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>This sets the width of the cache status shown on the application status bar.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label_10->setText(QCoreApplication::translate("Prefdlg", "width", nullptr));
#if QT_CONFIG(tooltip)
        showCacheThreadActivityChk->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>This will display two small activity lights on the status bar at the bottom of the application window. The activity lights turn green when there is caching activity for building metadata/thumbnails and full scale images.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        showCacheThreadActivityChk->setText(QCoreApplication::translate("Prefdlg", "Show cache thread activity", nullptr));
#if QT_CONFIG(tooltip)
        cachePreviewsChk->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>The largest preview embedded in the image file will be cached.  However, for large files this can take time.  This option tells the application to first create a smaller preview that will fit your monitor while the larger one is being built.  Try checking this option if you are experiencing some latency in loupe mode.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        cachePreviewsChk->setText(QCoreApplication::translate("Prefdlg", "Cache previews scaled to monitor resolution", nullptr));
#if QT_CONFIG(tooltip)
        cache100AheadRadio->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        cache100AheadRadio->setText(QCoreApplication::translate("Prefdlg", "100% ahead", nullptr));
#if QT_CONFIG(tooltip)
        cache70AheadRadio->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        cache70AheadRadio->setText(QCoreApplication::translate("Prefdlg", "70% ahead", nullptr));
#if QT_CONFIG(tooltip)
        cache90AheadRadio->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        cache90AheadRadio->setText(QCoreApplication::translate("Prefdlg", "90% ahead", nullptr));
#if QT_CONFIG(tooltip)
        cache50AheadRadio->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        cache50AheadRadio->setText(QCoreApplication::translate("Prefdlg", "50% ahead", nullptr));
#if QT_CONFIG(tooltip)
        cache60AheadRadio->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        cache60AheadRadio->setText(QCoreApplication::translate("Prefdlg", "60% ahead", nullptr));
#if QT_CONFIG(tooltip)
        cache80AheadRadio->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        cache80AheadRadio->setText(QCoreApplication::translate("Prefdlg", "80% ahead", nullptr));
#if QT_CONFIG(tooltip)
        cacheSizeSpinbox->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        label_8->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label_8->setText(QCoreApplication::translate("Prefdlg", "Cache size (GB)", nullptr));
#if QT_CONFIG(tooltip)
        label->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label->setText(QCoreApplication::translate("Prefdlg", "Cache strategy:", nullptr));
        metadataCacheGroupBox->setTitle(QCoreApplication::translate("Prefdlg", "Metadata cache", nullptr));
        metadataCacheAllRadio->setText(QCoreApplication::translate("Prefdlg", "Cache all items when open folder (could be a slight delay)", nullptr));
        metadataCacheChunkRadio->setText(QCoreApplication::translate("Prefdlg", "Cache", nullptr));
#if QT_CONFIG(tooltip)
        metadataCacheSizeSpinbox->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label_14->setText(QCoreApplication::translate("Prefdlg", "items at a time", nullptr));
        thumbnailCacheGroupBoc->setTitle(QCoreApplication::translate("Prefdlg", "Thumbnail cache", nullptr));
        thumbnailCachAllRadio->setText(QString());
        thumbnailCacheChunkRadio->setText(QCoreApplication::translate("Prefdlg", "Cache", nullptr));
#if QT_CONFIG(tooltip)
        thumbnailCacheSizeSpinbox->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label_15->setText(QCoreApplication::translate("Prefdlg", "viewports ahead", nullptr));
        label_18->setText(QCoreApplication::translate("Prefdlg", "Cache all thumbnails in folder(s).  This can cause an initial delay and consume a lot of memory.", nullptr));
        slideshowRandomChk->setText(QString());
        label_12->setText(QCoreApplication::translate("Prefdlg", "Show random order", nullptr));
#if QT_CONFIG(tooltip)
        slideshowDelaySpinbox->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>This sets the amount of time each image is displayed before the next image appears.  If set to zero then each image will be displayed until you manually move forward or backwards.  Note that you can also change this delay while the slideshow is active by pressing the zero to nine (0 - 9) keys.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        label_11->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>This sets the amount of time each image is displayed before the next image appears.  If set to zero then each image will be displayed until you manually move forward or backwards.  Note that you can also change this delay while the slideshow is active by pressing the zero to nine (0 - 9) keys.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label_11->setText(QCoreApplication::translate("Prefdlg", "Delay (1 - 3600 sec)", nullptr));
        groupBox->setTitle(QCoreApplication::translate("Prefdlg", "Docked", nullptr));
        showThumbLabelChk->setText(QCoreApplication::translate("Prefdlg", "Show thumbnail filename", nullptr));
        label_7->setText(QCoreApplication::translate("Prefdlg", "Font size", nullptr));
        thumbSmallerBtn->setText(QCoreApplication::translate("Prefdlg", "<", nullptr));
        fontSizeLbl->setText(QString());
        label_2->setText(QCoreApplication::translate("Prefdlg", "Thumbnail size", nullptr));
        thumbLargerBtn->setText(QCoreApplication::translate("Prefdlg", ">", nullptr));
        groupBox_2->setTitle(QCoreApplication::translate("Prefdlg", "Grid", nullptr));
        fontSizeLbl_2->setText(QString());
        label_17->setText(QCoreApplication::translate("Prefdlg", "Thumbnail size", nullptr));
        gridSmallerBtn->setText(QCoreApplication::translate("Prefdlg", "<", nullptr));
        label_19->setText(QCoreApplication::translate("Prefdlg", "Font size", nullptr));
        gridLargerBtn->setText(QCoreApplication::translate("Prefdlg", ">", nullptr));
        showThumbLabelChk_2->setText(QCoreApplication::translate("Prefdlg", "Show thumbnail filename", nullptr));
        label_3->setText(QCoreApplication::translate("Prefdlg", "Maximum thumbnail size", nullptr));
        fontSizeLbl_21->setText(QString());
        label_171->setText(QCoreApplication::translate("Prefdlg", "Thumbnail size", nullptr));
        gridSmallerBtn1->setText(QCoreApplication::translate("Prefdlg", "<", nullptr));
        label_191->setText(QCoreApplication::translate("Prefdlg", "Font size", nullptr));
        gridLargerBtn1->setText(QCoreApplication::translate("Prefdlg", ">", nullptr));
        showThumbLabelChk_21->setText(QCoreApplication::translate("Prefdlg", "Show thumbnail filename", nullptr));
        label_4->setText(QCoreApplication::translate("Prefdlg", "<html><head/><body><p><span style=\" font-weight:600; color:#ff0000;\">WARNING</span><span style=\" color:#ff0000;\">: </span><span style=\" color:#e5e5e5;\">Larger thumbnail sizes can consume huge amounts of memory if you load a folder with a lot of images. </span></p></body></html>", nullptr));
        label_5->setText(QCoreApplication::translate("Prefdlg", "pixels (40 - 480)", nullptr));
        fullScreenExplanationLbl->setText(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>When full screen is chosen, select which panels should be visible. Hiding all the docks will result in only the loupe view or grid of all the thumbnails being visible.  You can still show/hide panels in the usual way, so this is just a customization to shortcut to your usual full screen preference.</p><p>When you leave full screen view the application will return to its prior state.</p></body></html>", nullptr));
        foldersChk->setText(QCoreApplication::translate("Prefdlg", "Folders", nullptr));
        favsChk->setText(QCoreApplication::translate("Prefdlg", "Favourites", nullptr));
        filtersChk->setText(QCoreApplication::translate("Prefdlg", "Filters", nullptr));
        metadataChk->setText(QCoreApplication::translate("Prefdlg", "Metadata", nullptr));
        thumbsChk->setText(QCoreApplication::translate("Prefdlg", "Thumbnails", nullptr));
        statusBarChk->setText(QCoreApplication::translate("Prefdlg", "Status Bar", nullptr));
#if QT_CONFIG(tooltip)
        frame->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Select which items to show or hide in the metadata panel.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label_6->setText(QCoreApplication::translate("Prefdlg", "Metadata Panel Items", nullptr));
#if QT_CONFIG(tooltip)
        frame_3->setToolTip(QCoreApplication::translate("Prefdlg", "<html><head/><body><p>Select which items to show or hide in the table mode view.</p></body></html>", nullptr));
#endif // QT_CONFIG(tooltip)
        label_16->setText(QCoreApplication::translate("Prefdlg", "Table Mode Columns", nullptr));

        const bool __sortingEnabled = listWidget->isSortingEnabled();
        listWidget->setSortingEnabled(false);
        QListWidgetItem *___qlistwidgetitem = listWidget->item(0);
        ___qlistwidgetitem->setText(QCoreApplication::translate("Prefdlg", "General", nullptr));
        QListWidgetItem *___qlistwidgetitem1 = listWidget->item(1);
        ___qlistwidgetitem1->setText(QCoreApplication::translate("Prefdlg", "Appearance", nullptr));
        QListWidgetItem *___qlistwidgetitem2 = listWidget->item(2);
        ___qlistwidgetitem2->setText(QCoreApplication::translate("Prefdlg", "Cache", nullptr));
        QListWidgetItem *___qlistwidgetitem3 = listWidget->item(3);
        ___qlistwidgetitem3->setText(QCoreApplication::translate("Prefdlg", "Slideshow", nullptr));
        QListWidgetItem *___qlistwidgetitem4 = listWidget->item(4);
        ___qlistwidgetitem4->setText(QCoreApplication::translate("Prefdlg", "Thumbnails", nullptr));
        QListWidgetItem *___qlistwidgetitem5 = listWidget->item(5);
        ___qlistwidgetitem5->setText(QCoreApplication::translate("Prefdlg", "Full screen default docks", nullptr));
        QListWidgetItem *___qlistwidgetitem6 = listWidget->item(6);
        ___qlistwidgetitem6->setText(QCoreApplication::translate("Prefdlg", "Show or hide fields", nullptr));
        listWidget->setSortingEnabled(__sortingEnabled);

    } // retranslateUi

};

namespace Ui {
    class Prefdlg: public Ui_Prefdlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PREFDLG_H
