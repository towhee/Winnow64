/********************************************************************************
** Form generated from reading UI file 'prefdlg.ui'
**
** Created by: Qt User Interface Compiler version 5.10.0
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_PREFDLG_H
#define UI_PREFDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
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
    QGroupBox *groupBoxTrackpadAction;
    QWidget *layoutWidget;
    QVBoxLayout *verticalLayout_3;
    QRadioButton *trackpadIterateRadio;
    QRadioButton *trackpadScrollRadio;
    QGroupBox *groupBoxFolders;
    QCheckBox *rememberFolderChk;
    QCheckBox *updateAppChk;
    QGroupBox *groupBoxFolders_2;
    QCheckBox *mouseClickScrollChk;
    QWidget *page_4;
    QGroupBox *groupBoxFolders_3;
    QSlider *globalFontSizeSlider;
    QLabel *globalFontSizeLbl;
    QGroupBox *groupBoxFolders_4;
    QSlider *classificationBadgeImageDiamSlider;
    QLabel *classificationBadgeImageDiamLbl;
    QLabel *label_13;
    QLabel *label_20;
    QLabel *classificationBadgeThumbDiamLbl;
    QSlider *classificationBadgeThumbDiamSlider;
    QWidget *pageCache;
    QSpinBox *cacheSizeSpinbox;
    QLabel *label_10;
    QSpinBox *cacheStatusWidthSpin;
    QLabel *label_8;
    QLabel *label;
    QCheckBox *showCacheStatusChk;
    QCheckBox *cachePreviewsChk;
    QCheckBox *showCacheThreadActivityChk;
    QLabel *label_9;
    QSpinBox *cacheDelaySpinbox;
    QFrame *frame;
    QVBoxLayout *verticalLayout;
    QRadioButton *cache50AheadRadio;
    QRadioButton *cache60AheadRadio;
    QRadioButton *cache70AheadRadio;
    QRadioButton *cache80AheadRadio;
    QRadioButton *cache90AheadRadio;
    QRadioButton *cache100AheadRadio;
    QWidget *pageSlideshow;
    QCheckBox *slideshowRandomChk;
    QLabel *label_12;
    QSpinBox *slideshowDelaySpinbox;
    QLabel *label_11;
    QWidget *pageThumbsDock;
    QLabel *iconWidthLbl;
    QSlider *fontSizeSlider;
    QLabel *iconPaddingLbl;
    QLabel *thumbSpacingLbl;
    QLabel *label_4;
    QLabel *label_5;
    QSlider *iconWidthSlider;
    QCheckBox *showThumbLabelChk;
    QLabel *fontSizeLbl;
    QSlider *iconPaddingSlider;
    QSlider *thumbSpacingSlider;
    QLabel *label_2;
    QSlider *iconHeightSlider;
    QCheckBox *lockDimChk;
    QLabel *label_3;
    QLabel *iconHeightLbl;
    QLabel *label_7;
    QCheckBox *wrapChk;
    QWidget *pageThumbsGrid;
    QLabel *iconWidthLbl_2;
    QSlider *fontSizeSlider_2;
    QLabel *iconPaddingLbl_2;
    QLabel *thumbSpacingLbl_2;
    QLabel *label_14;
    QLabel *label_15;
    QSlider *iconWidthSlider_2;
    QCheckBox *showThumbLabelChk_2;
    QLabel *fontSizeLbl_2;
    QSlider *iconPaddingSlider_2;
    QSlider *thumbSpacingSlider_2;
    QLabel *label_17;
    QSlider *iconHeightSlider_2;
    QCheckBox *lockDimChk_2;
    QLabel *label_18;
    QLabel *iconHeightLbl_2;
    QLabel *label_19;
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
    QWidget *page_3;
    QFrame *frame_2;
    QFrame *frame1;
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
            Prefdlg->setObjectName(QStringLiteral("Prefdlg"));
        Prefdlg->resize(735, 601);
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(Prefdlg->sizePolicy().hasHeightForWidth());
        Prefdlg->setSizePolicy(sizePolicy);
        Prefdlg->setModal(true);
        closeBtn = new QPushButton(Prefdlg);
        buttonGroup = new QButtonGroup(Prefdlg);
        buttonGroup->setObjectName(QStringLiteral("buttonGroup"));
        buttonGroup->addButton(closeBtn);
        closeBtn->setObjectName(QStringLiteral("closeBtn"));
        closeBtn->setGeometry(QRect(570, 540, 121, 31));
        stackedWidget = new QStackedWidget(Prefdlg);
        stackedWidget->setObjectName(QStringLiteral("stackedWidget"));
        stackedWidget->setGeometry(QRect(200, 20, 481, 491));
        QFont font;
        font.setPointSize(12);
        stackedWidget->setFont(font);
        stackedWidget->setFrameShape(QFrame::Panel);
        pageGeneral = new QWidget();
        pageGeneral->setObjectName(QStringLiteral("pageGeneral"));
        groupBoxTrackpadAction = new QGroupBox(pageGeneral);
        groupBoxTrackpadAction->setObjectName(QStringLiteral("groupBoxTrackpadAction"));
        groupBoxTrackpadAction->setGeometry(QRect(20, 140, 431, 101));
        layoutWidget = new QWidget(groupBoxTrackpadAction);
        layoutWidget->setObjectName(QStringLiteral("layoutWidget"));
        layoutWidget->setGeometry(QRect(20, 27, 337, 65));
        verticalLayout_3 = new QVBoxLayout(layoutWidget);
        verticalLayout_3->setObjectName(QStringLiteral("verticalLayout_3"));
        verticalLayout_3->setContentsMargins(0, 0, 0, 0);
        trackpadIterateRadio = new QRadioButton(layoutWidget);
        trackpadIterateRadio->setObjectName(QStringLiteral("trackpadIterateRadio"));
        trackpadIterateRadio->setFont(font);

        verticalLayout_3->addWidget(trackpadIterateRadio);

        trackpadScrollRadio = new QRadioButton(layoutWidget);
        trackpadScrollRadio->setObjectName(QStringLiteral("trackpadScrollRadio"));
        trackpadScrollRadio->setFont(font);

        verticalLayout_3->addWidget(trackpadScrollRadio);

        groupBoxFolders = new QGroupBox(pageGeneral);
        groupBoxFolders->setObjectName(QStringLiteral("groupBoxFolders"));
        groupBoxFolders->setGeometry(QRect(20, 10, 431, 111));
        rememberFolderChk = new QCheckBox(groupBoxFolders);
        rememberFolderChk->setObjectName(QStringLiteral("rememberFolderChk"));
        rememberFolderChk->setGeometry(QRect(20, 30, 261, 20));
        rememberFolderChk->setFont(font);
        updateAppChk = new QCheckBox(groupBoxFolders);
        updateAppChk->setObjectName(QStringLiteral("updateAppChk"));
        updateAppChk->setGeometry(QRect(20, 60, 261, 20));
        updateAppChk->setFont(font);
        groupBoxFolders_2 = new QGroupBox(pageGeneral);
        groupBoxFolders_2->setObjectName(QStringLiteral("groupBoxFolders_2"));
        groupBoxFolders_2->setGeometry(QRect(20, 260, 431, 71));
        mouseClickScrollChk = new QCheckBox(groupBoxFolders_2);
        mouseClickScrollChk->setObjectName(QStringLiteral("mouseClickScrollChk"));
        mouseClickScrollChk->setGeometry(QRect(20, 30, 371, 20));
        mouseClickScrollChk->setFont(font);
        stackedWidget->addWidget(pageGeneral);
        page_4 = new QWidget();
        page_4->setObjectName(QStringLiteral("page_4"));
        groupBoxFolders_3 = new QGroupBox(page_4);
        groupBoxFolders_3->setObjectName(QStringLiteral("groupBoxFolders_3"));
        groupBoxFolders_3->setGeometry(QRect(20, 20, 431, 91));
        globalFontSizeSlider = new QSlider(groupBoxFolders_3);
        globalFontSizeSlider->setObjectName(QStringLiteral("globalFontSizeSlider"));
        globalFontSizeSlider->setGeometry(QRect(40, 40, 311, 22));
        globalFontSizeSlider->setMinimum(8);
        globalFontSizeSlider->setMaximum(20);
        globalFontSizeSlider->setPageStep(1);
        globalFontSizeSlider->setValue(14);
        globalFontSizeSlider->setOrientation(Qt::Horizontal);
        globalFontSizeSlider->setTickPosition(QSlider::TicksAbove);
        globalFontSizeSlider->setTickInterval(1);
        globalFontSizeLbl = new QLabel(groupBoxFolders_3);
        globalFontSizeLbl->setObjectName(QStringLiteral("globalFontSizeLbl"));
        globalFontSizeLbl->setGeometry(QRect(360, 30, 55, 31));
        groupBoxFolders_4 = new QGroupBox(page_4);
        groupBoxFolders_4->setObjectName(QStringLiteral("groupBoxFolders_4"));
        groupBoxFolders_4->setGeometry(QRect(20, 130, 431, 121));
        classificationBadgeImageDiamSlider = new QSlider(groupBoxFolders_4);
        classificationBadgeImageDiamSlider->setObjectName(QStringLiteral("classificationBadgeImageDiamSlider"));
        classificationBadgeImageDiamSlider->setGeometry(QRect(100, 40, 251, 22));
        classificationBadgeImageDiamSlider->setMinimum(0);
        classificationBadgeImageDiamSlider->setMaximum(100);
        classificationBadgeImageDiamSlider->setPageStep(10);
        classificationBadgeImageDiamSlider->setValue(20);
        classificationBadgeImageDiamSlider->setOrientation(Qt::Horizontal);
        classificationBadgeImageDiamSlider->setTickPosition(QSlider::TicksAbove);
        classificationBadgeImageDiamSlider->setTickInterval(10);
        classificationBadgeImageDiamLbl = new QLabel(groupBoxFolders_4);
        classificationBadgeImageDiamLbl->setObjectName(QStringLiteral("classificationBadgeImageDiamLbl"));
        classificationBadgeImageDiamLbl->setGeometry(QRect(360, 30, 55, 31));
        label_13 = new QLabel(groupBoxFolders_4);
        label_13->setObjectName(QStringLiteral("label_13"));
        label_13->setGeometry(QRect(10, 40, 81, 20));
        label_20 = new QLabel(groupBoxFolders_4);
        label_20->setObjectName(QStringLiteral("label_20"));
        label_20->setGeometry(QRect(10, 80, 81, 20));
        classificationBadgeThumbDiamLbl = new QLabel(groupBoxFolders_4);
        classificationBadgeThumbDiamLbl->setObjectName(QStringLiteral("classificationBadgeThumbDiamLbl"));
        classificationBadgeThumbDiamLbl->setGeometry(QRect(360, 70, 55, 31));
        classificationBadgeThumbDiamSlider = new QSlider(groupBoxFolders_4);
        classificationBadgeThumbDiamSlider->setObjectName(QStringLiteral("classificationBadgeThumbDiamSlider"));
        classificationBadgeThumbDiamSlider->setGeometry(QRect(100, 80, 251, 22));
        classificationBadgeThumbDiamSlider->setMinimum(0);
        classificationBadgeThumbDiamSlider->setMaximum(50);
        classificationBadgeThumbDiamSlider->setPageStep(5);
        classificationBadgeThumbDiamSlider->setValue(12);
        classificationBadgeThumbDiamSlider->setOrientation(Qt::Horizontal);
        classificationBadgeThumbDiamSlider->setTickPosition(QSlider::TicksAbove);
        classificationBadgeThumbDiamSlider->setTickInterval(5);
        stackedWidget->addWidget(page_4);
        pageCache = new QWidget();
        pageCache->setObjectName(QStringLiteral("pageCache"));
        cacheSizeSpinbox = new QSpinBox(pageCache);
        cacheSizeSpinbox->setObjectName(QStringLiteral("cacheSizeSpinbox"));
        cacheSizeSpinbox->setGeometry(QRect(140, 23, 51, 18));
        cacheSizeSpinbox->setMinimum(1);
        cacheSizeSpinbox->setMaximum(64);
        label_10 = new QLabel(pageCache);
        label_10->setObjectName(QStringLiteral("label_10"));
        label_10->setGeometry(QRect(185, 291, 171, 18));
        label_10->setFont(font);
        label_10->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        cacheStatusWidthSpin = new QSpinBox(pageCache);
        cacheStatusWidthSpin->setObjectName(QStringLiteral("cacheStatusWidthSpin"));
        cacheStatusWidthSpin->setGeometry(QRect(360, 290, 71, 18));
        cacheStatusWidthSpin->setFont(font);
        cacheStatusWidthSpin->setMinimum(100);
        cacheStatusWidthSpin->setMaximum(1000);
        cacheStatusWidthSpin->setSingleStep(25);
        cacheStatusWidthSpin->setValue(200);
        label_8 = new QLabel(pageCache);
        label_8->setObjectName(QStringLiteral("label_8"));
        label_8->setGeometry(QRect(30, 24, 91, 16));
        label_8->setFont(font);
        label_8->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        label = new QLabel(pageCache);
        label->setObjectName(QStringLiteral("label"));
        label->setGeometry(QRect(30, 60, 111, 16));
        label->setFont(font);
        label->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);
        showCacheStatusChk = new QCheckBox(pageCache);
        showCacheStatusChk->setObjectName(QStringLiteral("showCacheStatusChk"));
        showCacheStatusChk->setGeometry(QRect(20, 289, 151, 21));
        showCacheStatusChk->setFont(font);
        cachePreviewsChk = new QCheckBox(pageCache);
        cachePreviewsChk->setObjectName(QStringLiteral("cachePreviewsChk"));
        cachePreviewsChk->setGeometry(QRect(20, 240, 341, 20));
        cachePreviewsChk->setFont(font);
        showCacheThreadActivityChk = new QCheckBox(pageCache);
        showCacheThreadActivityChk->setObjectName(QStringLiteral("showCacheThreadActivityChk"));
        showCacheThreadActivityChk->setGeometry(QRect(20, 320, 351, 17));
        showCacheThreadActivityChk->setFont(font);
        label_9 = new QLabel(pageCache);
        label_9->setObjectName(QStringLiteral("label_9"));
        label_9->setGeometry(QRect(260, 24, 91, 16));
        label_9->setFont(font);
        label_9->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        cacheDelaySpinbox = new QSpinBox(pageCache);
        cacheDelaySpinbox->setObjectName(QStringLiteral("cacheDelaySpinbox"));
        cacheDelaySpinbox->setGeometry(QRect(360, 23, 71, 18));
        cacheDelaySpinbox->setMinimum(0);
        cacheDelaySpinbox->setMaximum(500);
        cacheDelaySpinbox->setSingleStep(25);
        cacheDelaySpinbox->setValue(250);
        frame = new QFrame(pageCache);
        frame->setObjectName(QStringLiteral("frame"));
        frame->setGeometry(QRect(132, 50, 151, 161));
        verticalLayout = new QVBoxLayout(frame);
        verticalLayout->setObjectName(QStringLiteral("verticalLayout"));
        cache50AheadRadio = new QRadioButton(frame);
        cache50AheadRadio->setObjectName(QStringLiteral("cache50AheadRadio"));
        cache50AheadRadio->setFont(font);

        verticalLayout->addWidget(cache50AheadRadio);

        cache60AheadRadio = new QRadioButton(frame);
        cache60AheadRadio->setObjectName(QStringLiteral("cache60AheadRadio"));
        cache60AheadRadio->setFont(font);

        verticalLayout->addWidget(cache60AheadRadio);

        cache70AheadRadio = new QRadioButton(frame);
        cache70AheadRadio->setObjectName(QStringLiteral("cache70AheadRadio"));
        cache70AheadRadio->setFont(font);

        verticalLayout->addWidget(cache70AheadRadio);

        cache80AheadRadio = new QRadioButton(frame);
        cache80AheadRadio->setObjectName(QStringLiteral("cache80AheadRadio"));
        cache80AheadRadio->setFont(font);

        verticalLayout->addWidget(cache80AheadRadio);

        cache90AheadRadio = new QRadioButton(frame);
        cache90AheadRadio->setObjectName(QStringLiteral("cache90AheadRadio"));
        cache90AheadRadio->setFont(font);

        verticalLayout->addWidget(cache90AheadRadio);

        cache100AheadRadio = new QRadioButton(frame);
        cache100AheadRadio->setObjectName(QStringLiteral("cache100AheadRadio"));
        cache100AheadRadio->setFont(font);

        verticalLayout->addWidget(cache100AheadRadio);

        stackedWidget->addWidget(pageCache);
        pageSlideshow = new QWidget();
        pageSlideshow->setObjectName(QStringLiteral("pageSlideshow"));
        slideshowRandomChk = new QCheckBox(pageSlideshow);
        slideshowRandomChk->setObjectName(QStringLiteral("slideshowRandomChk"));
        slideshowRandomChk->setGeometry(QRect(200, 80, 19, 18));
        slideshowRandomChk->setFont(font);
        label_12 = new QLabel(pageSlideshow);
        label_12->setObjectName(QStringLiteral("label_12"));
        label_12->setGeometry(QRect(20, 80, 161, 16));
        label_12->setFont(font);
        slideshowDelaySpinbox = new QSpinBox(pageSlideshow);
        slideshowDelaySpinbox->setObjectName(QStringLiteral("slideshowDelaySpinbox"));
        slideshowDelaySpinbox->setGeometry(QRect(200, 40, 48, 18));
        slideshowDelaySpinbox->setFont(font);
        label_11 = new QLabel(pageSlideshow);
        label_11->setObjectName(QStringLiteral("label_11"));
        label_11->setGeometry(QRect(20, 40, 111, 18));
        label_11->setFont(font);
        stackedWidget->addWidget(pageSlideshow);
        pageThumbsDock = new QWidget();
        pageThumbsDock->setObjectName(QStringLiteral("pageThumbsDock"));
        iconWidthLbl = new QLabel(pageThumbsDock);
        iconWidthLbl->setObjectName(QStringLiteral("iconWidthLbl"));
        iconWidthLbl->setGeometry(QRect(380, 30, 30, 16));
        iconWidthLbl->setFont(font);
        iconWidthLbl->setFrameShape(QFrame::NoFrame);
        iconWidthLbl->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        fontSizeSlider = new QSlider(pageThumbsDock);
        fontSizeSlider->setObjectName(QStringLiteral("fontSizeSlider"));
        fontSizeSlider->setGeometry(QRect(200, 200, 160, 22));
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
        iconPaddingLbl = new QLabel(pageThumbsDock);
        iconPaddingLbl->setObjectName(QStringLiteral("iconPaddingLbl"));
        iconPaddingLbl->setGeometry(QRect(380, 135, 30, 16));
        iconPaddingLbl->setFont(font);
        iconPaddingLbl->setFrameShape(QFrame::NoFrame);
        iconPaddingLbl->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        thumbSpacingLbl = new QLabel(pageThumbsDock);
        thumbSpacingLbl->setObjectName(QStringLiteral("thumbSpacingLbl"));
        thumbSpacingLbl->setGeometry(QRect(380, 115, 30, 16));
        thumbSpacingLbl->setFont(font);
        thumbSpacingLbl->setFrameShape(QFrame::NoFrame);
        thumbSpacingLbl->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label_4 = new QLabel(pageThumbsDock);
        label_4->setObjectName(QStringLiteral("label_4"));
        label_4->setGeometry(QRect(20, 115, 151, 16));
        label_4->setFont(font);
        label_5 = new QLabel(pageThumbsDock);
        label_5->setObjectName(QStringLiteral("label_5"));
        label_5->setGeometry(QRect(20, 135, 141, 16));
        label_5->setFont(font);
        iconWidthSlider = new QSlider(pageThumbsDock);
        iconWidthSlider->setObjectName(QStringLiteral("iconWidthSlider"));
        iconWidthSlider->setGeometry(QRect(200, 30, 160, 22));
        iconWidthSlider->setFont(font);
        iconWidthSlider->setMinimum(40);
        iconWidthSlider->setMaximum(160);
        iconWidthSlider->setSingleStep(1);
        iconWidthSlider->setPageStep(5);
        iconWidthSlider->setValue(41);
        iconWidthSlider->setSliderPosition(41);
        iconWidthSlider->setOrientation(Qt::Horizontal);
        iconWidthSlider->setInvertedAppearance(false);
        iconWidthSlider->setTickPosition(QSlider::NoTicks);
        iconWidthSlider->setTickInterval(5);
        showThumbLabelChk = new QCheckBox(pageThumbsDock);
        showThumbLabelChk->setObjectName(QStringLiteral("showThumbLabelChk"));
        showThumbLabelChk->setGeometry(QRect(20, 180, 221, 17));
        showThumbLabelChk->setFont(font);
        fontSizeLbl = new QLabel(pageThumbsDock);
        fontSizeLbl->setObjectName(QStringLiteral("fontSizeLbl"));
        fontSizeLbl->setGeometry(QRect(380, 196, 30, 16));
        fontSizeLbl->setFont(font);
        fontSizeLbl->setFrameShape(QFrame::NoFrame);
        fontSizeLbl->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        iconPaddingSlider = new QSlider(pageThumbsDock);
        iconPaddingSlider->setObjectName(QStringLiteral("iconPaddingSlider"));
        iconPaddingSlider->setGeometry(QRect(200, 135, 160, 22));
        iconPaddingSlider->setFont(font);
        iconPaddingSlider->setMinimum(0);
        iconPaddingSlider->setMaximum(10);
        iconPaddingSlider->setSingleStep(1);
        iconPaddingSlider->setPageStep(5);
        iconPaddingSlider->setValue(0);
        iconPaddingSlider->setSliderPosition(0);
        iconPaddingSlider->setOrientation(Qt::Horizontal);
        iconPaddingSlider->setInvertedAppearance(false);
        iconPaddingSlider->setTickPosition(QSlider::NoTicks);
        iconPaddingSlider->setTickInterval(5);
        thumbSpacingSlider = new QSlider(pageThumbsDock);
        thumbSpacingSlider->setObjectName(QStringLiteral("thumbSpacingSlider"));
        thumbSpacingSlider->setGeometry(QRect(200, 115, 160, 22));
        thumbSpacingSlider->setFont(font);
        thumbSpacingSlider->setMinimum(0);
        thumbSpacingSlider->setMaximum(30);
        thumbSpacingSlider->setSingleStep(1);
        thumbSpacingSlider->setPageStep(5);
        thumbSpacingSlider->setValue(24);
        thumbSpacingSlider->setSliderPosition(24);
        thumbSpacingSlider->setOrientation(Qt::Horizontal);
        thumbSpacingSlider->setInvertedAppearance(false);
        thumbSpacingSlider->setTickPosition(QSlider::NoTicks);
        thumbSpacingSlider->setTickInterval(5);
        label_2 = new QLabel(pageThumbsDock);
        label_2->setObjectName(QStringLiteral("label_2"));
        label_2->setGeometry(QRect(20, 30, 121, 16));
        label_2->setFont(font);
        iconHeightSlider = new QSlider(pageThumbsDock);
        iconHeightSlider->setObjectName(QStringLiteral("iconHeightSlider"));
        iconHeightSlider->setGeometry(QRect(200, 50, 160, 22));
        iconHeightSlider->setFont(font);
        iconHeightSlider->setMinimum(40);
        iconHeightSlider->setMaximum(160);
        iconHeightSlider->setSingleStep(1);
        iconHeightSlider->setPageStep(5);
        iconHeightSlider->setValue(41);
        iconHeightSlider->setSliderPosition(41);
        iconHeightSlider->setOrientation(Qt::Horizontal);
        iconHeightSlider->setInvertedAppearance(false);
        iconHeightSlider->setTickPosition(QSlider::NoTicks);
        iconHeightSlider->setTickInterval(5);
        lockDimChk = new QCheckBox(pageThumbsDock);
        lockDimChk->setObjectName(QStringLiteral("lockDimChk"));
        lockDimChk->setGeometry(QRect(20, 70, 181, 20));
        lockDimChk->setFont(font);
        label_3 = new QLabel(pageThumbsDock);
        label_3->setObjectName(QStringLiteral("label_3"));
        label_3->setGeometry(QRect(20, 50, 131, 16));
        label_3->setFont(font);
        iconHeightLbl = new QLabel(pageThumbsDock);
        iconHeightLbl->setObjectName(QStringLiteral("iconHeightLbl"));
        iconHeightLbl->setGeometry(QRect(380, 50, 30, 16));
        iconHeightLbl->setFont(font);
        iconHeightLbl->setFrameShape(QFrame::NoFrame);
        iconHeightLbl->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label_7 = new QLabel(pageThumbsDock);
        label_7->setObjectName(QStringLiteral("label_7"));
        label_7->setGeometry(QRect(20, 200, 121, 16));
        label_7->setFont(font);
        wrapChk = new QCheckBox(pageThumbsDock);
        wrapChk->setObjectName(QStringLiteral("wrapChk"));
        wrapChk->setGeometry(QRect(20, 250, 431, 20));
        stackedWidget->addWidget(pageThumbsDock);
        pageThumbsGrid = new QWidget();
        pageThumbsGrid->setObjectName(QStringLiteral("pageThumbsGrid"));
        iconWidthLbl_2 = new QLabel(pageThumbsGrid);
        iconWidthLbl_2->setObjectName(QStringLiteral("iconWidthLbl_2"));
        iconWidthLbl_2->setGeometry(QRect(380, 30, 30, 16));
        iconWidthLbl_2->setFont(font);
        iconWidthLbl_2->setFrameShape(QFrame::NoFrame);
        iconWidthLbl_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        fontSizeSlider_2 = new QSlider(pageThumbsGrid);
        fontSizeSlider_2->setObjectName(QStringLiteral("fontSizeSlider_2"));
        fontSizeSlider_2->setGeometry(QRect(200, 200, 160, 22));
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
        iconPaddingLbl_2 = new QLabel(pageThumbsGrid);
        iconPaddingLbl_2->setObjectName(QStringLiteral("iconPaddingLbl_2"));
        iconPaddingLbl_2->setGeometry(QRect(380, 135, 30, 16));
        iconPaddingLbl_2->setFont(font);
        iconPaddingLbl_2->setFrameShape(QFrame::NoFrame);
        iconPaddingLbl_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        thumbSpacingLbl_2 = new QLabel(pageThumbsGrid);
        thumbSpacingLbl_2->setObjectName(QStringLiteral("thumbSpacingLbl_2"));
        thumbSpacingLbl_2->setGeometry(QRect(380, 115, 30, 16));
        thumbSpacingLbl_2->setFont(font);
        thumbSpacingLbl_2->setFrameShape(QFrame::NoFrame);
        thumbSpacingLbl_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label_14 = new QLabel(pageThumbsGrid);
        label_14->setObjectName(QStringLiteral("label_14"));
        label_14->setGeometry(QRect(20, 115, 151, 16));
        label_14->setFont(font);
        label_15 = new QLabel(pageThumbsGrid);
        label_15->setObjectName(QStringLiteral("label_15"));
        label_15->setGeometry(QRect(20, 135, 141, 16));
        label_15->setFont(font);
        iconWidthSlider_2 = new QSlider(pageThumbsGrid);
        iconWidthSlider_2->setObjectName(QStringLiteral("iconWidthSlider_2"));
        iconWidthSlider_2->setGeometry(QRect(200, 30, 160, 22));
        iconWidthSlider_2->setFont(font);
        iconWidthSlider_2->setMinimum(40);
        iconWidthSlider_2->setMaximum(160);
        iconWidthSlider_2->setSingleStep(1);
        iconWidthSlider_2->setPageStep(5);
        iconWidthSlider_2->setValue(41);
        iconWidthSlider_2->setSliderPosition(41);
        iconWidthSlider_2->setOrientation(Qt::Horizontal);
        iconWidthSlider_2->setInvertedAppearance(false);
        iconWidthSlider_2->setTickPosition(QSlider::NoTicks);
        iconWidthSlider_2->setTickInterval(5);
        showThumbLabelChk_2 = new QCheckBox(pageThumbsGrid);
        showThumbLabelChk_2->setObjectName(QStringLiteral("showThumbLabelChk_2"));
        showThumbLabelChk_2->setGeometry(QRect(20, 180, 201, 17));
        showThumbLabelChk_2->setFont(font);
        fontSizeLbl_2 = new QLabel(pageThumbsGrid);
        fontSizeLbl_2->setObjectName(QStringLiteral("fontSizeLbl_2"));
        fontSizeLbl_2->setGeometry(QRect(380, 196, 30, 16));
        fontSizeLbl_2->setFont(font);
        fontSizeLbl_2->setFrameShape(QFrame::NoFrame);
        fontSizeLbl_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        iconPaddingSlider_2 = new QSlider(pageThumbsGrid);
        iconPaddingSlider_2->setObjectName(QStringLiteral("iconPaddingSlider_2"));
        iconPaddingSlider_2->setGeometry(QRect(200, 135, 160, 22));
        iconPaddingSlider_2->setFont(font);
        iconPaddingSlider_2->setMinimum(0);
        iconPaddingSlider_2->setMaximum(10);
        iconPaddingSlider_2->setSingleStep(1);
        iconPaddingSlider_2->setPageStep(5);
        iconPaddingSlider_2->setValue(0);
        iconPaddingSlider_2->setSliderPosition(0);
        iconPaddingSlider_2->setOrientation(Qt::Horizontal);
        iconPaddingSlider_2->setInvertedAppearance(false);
        iconPaddingSlider_2->setTickPosition(QSlider::NoTicks);
        iconPaddingSlider_2->setTickInterval(5);
        thumbSpacingSlider_2 = new QSlider(pageThumbsGrid);
        thumbSpacingSlider_2->setObjectName(QStringLiteral("thumbSpacingSlider_2"));
        thumbSpacingSlider_2->setGeometry(QRect(200, 115, 160, 22));
        thumbSpacingSlider_2->setFont(font);
        thumbSpacingSlider_2->setMinimum(0);
        thumbSpacingSlider_2->setMaximum(30);
        thumbSpacingSlider_2->setSingleStep(1);
        thumbSpacingSlider_2->setPageStep(5);
        thumbSpacingSlider_2->setValue(24);
        thumbSpacingSlider_2->setSliderPosition(24);
        thumbSpacingSlider_2->setOrientation(Qt::Horizontal);
        thumbSpacingSlider_2->setInvertedAppearance(false);
        thumbSpacingSlider_2->setTickPosition(QSlider::NoTicks);
        thumbSpacingSlider_2->setTickInterval(5);
        label_17 = new QLabel(pageThumbsGrid);
        label_17->setObjectName(QStringLiteral("label_17"));
        label_17->setGeometry(QRect(20, 30, 131, 16));
        label_17->setFont(font);
        iconHeightSlider_2 = new QSlider(pageThumbsGrid);
        iconHeightSlider_2->setObjectName(QStringLiteral("iconHeightSlider_2"));
        iconHeightSlider_2->setGeometry(QRect(200, 50, 160, 22));
        iconHeightSlider_2->setFont(font);
        iconHeightSlider_2->setMinimum(40);
        iconHeightSlider_2->setMaximum(160);
        iconHeightSlider_2->setSingleStep(1);
        iconHeightSlider_2->setPageStep(5);
        iconHeightSlider_2->setValue(41);
        iconHeightSlider_2->setSliderPosition(41);
        iconHeightSlider_2->setOrientation(Qt::Horizontal);
        iconHeightSlider_2->setInvertedAppearance(false);
        iconHeightSlider_2->setTickPosition(QSlider::NoTicks);
        iconHeightSlider_2->setTickInterval(5);
        lockDimChk_2 = new QCheckBox(pageThumbsGrid);
        lockDimChk_2->setObjectName(QStringLiteral("lockDimChk_2"));
        lockDimChk_2->setGeometry(QRect(20, 70, 181, 20));
        lockDimChk_2->setFont(font);
        label_18 = new QLabel(pageThumbsGrid);
        label_18->setObjectName(QStringLiteral("label_18"));
        label_18->setGeometry(QRect(20, 50, 131, 16));
        label_18->setFont(font);
        iconHeightLbl_2 = new QLabel(pageThumbsGrid);
        iconHeightLbl_2->setObjectName(QStringLiteral("iconHeightLbl_2"));
        iconHeightLbl_2->setGeometry(QRect(380, 50, 30, 16));
        iconHeightLbl_2->setFont(font);
        iconHeightLbl_2->setFrameShape(QFrame::NoFrame);
        iconHeightLbl_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label_19 = new QLabel(pageThumbsGrid);
        label_19->setObjectName(QStringLiteral("label_19"));
        label_19->setGeometry(QRect(20, 200, 121, 16));
        label_19->setFont(font);
        stackedWidget->addWidget(pageThumbsGrid);
        pageFullScreen = new QWidget();
        pageFullScreen->setObjectName(QStringLiteral("pageFullScreen"));
        fullScreenExplanationLbl = new QLabel(pageFullScreen);
        fullScreenExplanationLbl->setObjectName(QStringLiteral("fullScreenExplanationLbl"));
        fullScreenExplanationLbl->setGeometry(QRect(20, 30, 421, 61));
        fullScreenExplanationLbl->setFont(font);
        fullScreenExplanationLbl->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        fullScreenExplanationLbl->setWordWrap(true);
        layoutWidget1 = new QWidget(pageFullScreen);
        layoutWidget1->setObjectName(QStringLiteral("layoutWidget1"));
        layoutWidget1->setGeometry(QRect(20, 110, 221, 205));
        layoutWidget1->setFont(font);
        verticalLayout_2 = new QVBoxLayout(layoutWidget1);
        verticalLayout_2->setObjectName(QStringLiteral("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        foldersChk = new QCheckBox(layoutWidget1);
        foldersChk->setObjectName(QStringLiteral("foldersChk"));
        foldersChk->setFont(font);

        verticalLayout_2->addWidget(foldersChk);

        favsChk = new QCheckBox(layoutWidget1);
        favsChk->setObjectName(QStringLiteral("favsChk"));
        favsChk->setFont(font);

        verticalLayout_2->addWidget(favsChk);

        filtersChk = new QCheckBox(layoutWidget1);
        filtersChk->setObjectName(QStringLiteral("filtersChk"));
        filtersChk->setFont(font);

        verticalLayout_2->addWidget(filtersChk);

        metadataChk = new QCheckBox(layoutWidget1);
        metadataChk->setObjectName(QStringLiteral("metadataChk"));
        metadataChk->setFont(font);

        verticalLayout_2->addWidget(metadataChk);

        thumbsChk = new QCheckBox(layoutWidget1);
        thumbsChk->setObjectName(QStringLiteral("thumbsChk"));
        thumbsChk->setFont(font);

        verticalLayout_2->addWidget(thumbsChk);

        statusBarChk = new QCheckBox(layoutWidget1);
        statusBarChk->setObjectName(QStringLiteral("statusBarChk"));
        statusBarChk->setFont(font);

        verticalLayout_2->addWidget(statusBarChk);

        stackedWidget->addWidget(pageFullScreen);
        page_3 = new QWidget();
        page_3->setObjectName(QStringLiteral("page_3"));
        frame_2 = new QFrame(page_3);
        frame_2->setObjectName(QStringLiteral("frame_2"));
        frame_2->setGeometry(QRect(10, 10, 460, 481));
        frame_2->setFrameShape(QFrame::NoFrame);
        frame_2->setFrameShadow(QFrame::Raised);
        frame1 = new QFrame(frame_2);
        frame1->setObjectName(QStringLiteral("frame1"));
        frame1->setGeometry(QRect(20, 0, 181, 471));
        frame1->setFrameShape(QFrame::NoFrame);
        frame1->setFrameShadow(QFrame::Raised);
        layoutWidget2 = new QWidget(frame1);
        layoutWidget2->setObjectName(QStringLiteral("layoutWidget2"));
        layoutWidget2->setGeometry(QRect(0, 0, 198, 471));
        verticalLayout_4 = new QVBoxLayout(layoutWidget2);
        verticalLayout_4->setObjectName(QStringLiteral("verticalLayout_4"));
        verticalLayout_4->setContentsMargins(0, 0, 0, 0);
        label_6 = new QLabel(layoutWidget2);
        label_6->setObjectName(QStringLiteral("label_6"));

        verticalLayout_4->addWidget(label_6);

        infoFieldsTreeView = new QTreeView(layoutWidget2);
        infoFieldsTreeView->setObjectName(QStringLiteral("infoFieldsTreeView"));
        infoFieldsTreeView->setMinimumSize(QSize(0, 0));
        infoFieldsTreeView->setMaximumSize(QSize(16777215, 16777215));
        infoFieldsTreeView->setStyleSheet(QStringLiteral(""));
        infoFieldsTreeView->setFrameShape(QFrame::StyledPanel);
        infoFieldsTreeView->setFrameShadow(QFrame::Plain);
        infoFieldsTreeView->setLineWidth(0);
        infoFieldsTreeView->setIconSize(QSize(1, 0));
        infoFieldsTreeView->setHeaderHidden(true);

        verticalLayout_4->addWidget(infoFieldsTreeView);

        frame_3 = new QFrame(frame_2);
        frame_3->setObjectName(QStringLiteral("frame_3"));
        frame_3->setGeometry(QRect(240, 0, 191, 471));
        frame_3->setFrameShape(QFrame::NoFrame);
        frame_3->setFrameShadow(QFrame::Raised);
        layoutWidget3 = new QWidget(frame_3);
        layoutWidget3->setObjectName(QStringLiteral("layoutWidget3"));
        layoutWidget3->setGeometry(QRect(10, 0, 188, 471));
        verticalLayout_5 = new QVBoxLayout(layoutWidget3);
        verticalLayout_5->setObjectName(QStringLiteral("verticalLayout_5"));
        verticalLayout_5->setContentsMargins(0, 0, 0, 0);
        label_16 = new QLabel(layoutWidget3);
        label_16->setObjectName(QStringLiteral("label_16"));

        verticalLayout_5->addWidget(label_16);

        tableFieldsTable = new QTableView(layoutWidget3);
        tableFieldsTable->setObjectName(QStringLiteral("tableFieldsTable"));
        tableFieldsTable->setMaximumSize(QSize(200, 16777215));
        tableFieldsTable->setIconSize(QSize(0, 0));
        tableFieldsTable->verticalHeader()->setDefaultSectionSize(23);

        verticalLayout_5->addWidget(tableFieldsTable);

        stackedWidget->addWidget(page_3);
        listContainer = new QStackedWidget(Prefdlg);
        listContainer->setObjectName(QStringLiteral("listContainer"));
        listContainer->setGeometry(QRect(10, 20, 191, 491));
        listContainer->setFrameShape(QFrame::Box);
        page = new QWidget();
        page->setObjectName(QStringLiteral("page"));
        listWidget = new QListWidget(page);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        new QListWidgetItem(listWidget);
        listWidget->setObjectName(QStringLiteral("listWidget"));
        listWidget->setGeometry(QRect(0, 9, 191, 481));
        listWidget->setStyleSheet(QStringLiteral("border: none;"));
        listWidget->setFrameShape(QFrame::NoFrame);
        listWidget->setFrameShadow(QFrame::Plain);
        listWidget->setSpacing(4);
        listContainer->addWidget(page);
        page_2 = new QWidget();
        page_2->setObjectName(QStringLiteral("page_2"));
        listContainer->addWidget(page_2);

        retranslateUi(Prefdlg);
        QObject::connect(iconWidthSlider, SIGNAL(valueChanged(int)), iconWidthLbl, SLOT(setNum(int)));
        QObject::connect(iconHeightSlider, SIGNAL(valueChanged(int)), iconHeightLbl, SLOT(setNum(int)));
        QObject::connect(thumbSpacingSlider, SIGNAL(valueChanged(int)), thumbSpacingLbl, SLOT(setNum(int)));
        QObject::connect(iconPaddingSlider, SIGNAL(valueChanged(int)), iconPaddingLbl, SLOT(setNum(int)));
        QObject::connect(closeBtn, SIGNAL(clicked()), Prefdlg, SLOT(reject()));
        QObject::connect(fontSizeSlider, SIGNAL(valueChanged(int)), fontSizeLbl, SLOT(setNum(int)));
        QObject::connect(iconWidthSlider_2, SIGNAL(valueChanged(int)), iconWidthLbl_2, SLOT(setNum(int)));
        QObject::connect(iconHeightSlider_2, SIGNAL(valueChanged(int)), iconHeightLbl_2, SLOT(setNum(int)));
        QObject::connect(iconPaddingSlider_2, SIGNAL(valueChanged(int)), iconPaddingLbl_2, SLOT(setNum(int)));
        QObject::connect(thumbSpacingSlider_2, SIGNAL(valueChanged(int)), thumbSpacingLbl_2, SLOT(setNum(int)));
        QObject::connect(fontSizeSlider_2, SIGNAL(valueChanged(int)), fontSizeLbl_2, SLOT(setNum(int)));
        QObject::connect(classificationBadgeImageDiamSlider, SIGNAL(valueChanged(int)), classificationBadgeImageDiamLbl, SLOT(setNum(int)));
        QObject::connect(classificationBadgeThumbDiamSlider, SIGNAL(valueChanged(int)), classificationBadgeThumbDiamLbl, SLOT(setNum(int)));
        QObject::connect(globalFontSizeSlider, SIGNAL(valueChanged(int)), globalFontSizeLbl, SLOT(setNum(int)));

        stackedWidget->setCurrentIndex(0);
        listContainer->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(Prefdlg);
    } // setupUi

    void retranslateUi(QDialog *Prefdlg)
    {
        Prefdlg->setWindowTitle(QApplication::translate("Prefdlg", "Winnow Preferences", nullptr));
#ifndef QT_NO_TOOLTIP
        closeBtn->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>When closing the preferences dialog all changes will by retained.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        closeBtn->setText(QApplication::translate("Prefdlg", "Okay", nullptr));
        groupBoxTrackpadAction->setTitle(QApplication::translate("Prefdlg", "Trackpad Swipe Action", nullptr));
        trackpadIterateRadio->setText(QApplication::translate("Prefdlg", "Next / previous image", nullptr));
        trackpadScrollRadio->setText(QApplication::translate("Prefdlg", "Scroll current image when zoomed", nullptr));
        groupBoxFolders->setTitle(QApplication::translate("Prefdlg", "When open program", nullptr));
        rememberFolderChk->setText(QApplication::translate("Prefdlg", "Remember previous folder", nullptr));
        updateAppChk->setText(QApplication::translate("Prefdlg", "Check for program updates", nullptr));
        groupBoxFolders_2->setTitle(QApplication::translate("Prefdlg", "Mouse click selection", nullptr));
        mouseClickScrollChk->setText(QApplication::translate("Prefdlg", "Scroll to position at center when mouse click", nullptr));
        groupBoxFolders_3->setTitle(QApplication::translate("Prefdlg", "Application font size", nullptr));
#ifndef QT_NO_TOOLTIP
        globalFontSizeSlider->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Changes the global font. The font size in the thumbnails for the file name is controlled independently in &quot;Thumbs in Dock&quot; and &quot;Thumbs in Grid View&quot;.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        globalFontSizeLbl->setText(QString());
        groupBoxFolders_4->setTitle(QApplication::translate("Prefdlg", "Classification badge diameter", nullptr));
#ifndef QT_NO_TOOLTIP
        classificationBadgeImageDiamSlider->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Changes the global font. The font size in the thumbnails for the file name is controlled independently in &quot;Thumbs in Dock&quot; and &quot;Thumbs in Grid View&quot;.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        classificationBadgeImageDiamLbl->setText(QString());
        label_13->setText(QApplication::translate("Prefdlg", "Loupe", nullptr));
        label_20->setText(QApplication::translate("Prefdlg", "Thumb", nullptr));
        classificationBadgeThumbDiamLbl->setText(QString());
#ifndef QT_NO_TOOLTIP
        classificationBadgeThumbDiamSlider->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Changes the global font. The font size in the thumbnails for the file name is controlled independently in &quot;Thumbs in Dock&quot; and &quot;Thumbs in Grid View&quot;.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        cacheSizeSpinbox->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        label_10->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>This sets the width of the cache status shown on the application status bar.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        label_10->setText(QApplication::translate("Prefdlg", "Cache status width (px)", nullptr));
#ifndef QT_NO_TOOLTIP
        cacheStatusWidthSpin->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>This sets the width of the cache status shown on the application status bar.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        label_8->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        label_8->setText(QApplication::translate("Prefdlg", "Cache size (GB)", nullptr));
#ifndef QT_NO_TOOLTIP
        label->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        label->setText(QApplication::translate("Prefdlg", "Cache strategy", nullptr));
#ifndef QT_NO_TOOLTIP
        showCacheStatusChk->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>This will show the cache status in the status bar at the bottom of the application.  If you do not see the status bar you can enable it in the Windows menu or by pressing F10.  If you hover over the cache status another tooltip will explain the colour codes for the current file, target cache, current cache and total images.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        showCacheStatusChk->setText(QApplication::translate("Prefdlg", "Show cache status", nullptr));
#ifndef QT_NO_TOOLTIP
        cachePreviewsChk->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>The largest preview embedded in the image file will be cached.  However, for large files this can take time.  This option tells the application to first create a smaller preview that will fit your monitor while the larger one is being built.  Try checking this option if you are experiencing some latency in loupe mode.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        cachePreviewsChk->setText(QApplication::translate("Prefdlg", "Cache previews scaled to monitor resolution", nullptr));
#ifndef QT_NO_TOOLTIP
        showCacheThreadActivityChk->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>This will display two small activity lights on the status bar at the bottom of the application window. The activity lights turn green when there is caching activity for building metadata/thumbnails and full scale images.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        showCacheThreadActivityChk->setText(QApplication::translate("Prefdlg", "Show cache thread activity", nullptr));
#ifndef QT_NO_TOOLTIP
        label_9->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Caching images and rendering the selected image simultaneously can degrade performance when rapidly moving through images.  The delay allows the next image selected to be rendered without any caching activity.  Try experimenting with the delay value if you are experiencing hesitation rendering the selected image.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        label_9->setText(QApplication::translate("Prefdlg", "Delay (ms)", nullptr));
#ifndef QT_NO_TOOLTIP
        cacheDelaySpinbox->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Caching images and rendering the selected image simultaneously can degrade performance when rapidly moving through images.  The delay allows the next image selected to be rendered without any caching activity.  Try experimenting with the delay value if you are experiencing hesitation rendering the selected image.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        cache50AheadRadio->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        cache50AheadRadio->setText(QApplication::translate("Prefdlg", "50% ahead", nullptr));
#ifndef QT_NO_TOOLTIP
        cache60AheadRadio->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        cache60AheadRadio->setText(QApplication::translate("Prefdlg", "60% ahead", nullptr));
#ifndef QT_NO_TOOLTIP
        cache70AheadRadio->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        cache70AheadRadio->setText(QApplication::translate("Prefdlg", "70% ahead", nullptr));
#ifndef QT_NO_TOOLTIP
        cache80AheadRadio->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        cache80AheadRadio->setText(QApplication::translate("Prefdlg", "80% ahead", nullptr));
#ifndef QT_NO_TOOLTIP
        cache90AheadRadio->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        cache90AheadRadio->setText(QApplication::translate("Prefdlg", "90% ahead", nullptr));
#ifndef QT_NO_TOOLTIP
        cache100AheadRadio->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Experiment with different cache sizes.  2-8GB appear to work best.  Cache performance does not always improve with size.</p><p>If you tend to move back and forth between images then a 50% cache ahead strategy is best.  If you generally just move ahead through the images then weighting a higher percentage ahead makes sense.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        cache100AheadRadio->setText(QApplication::translate("Prefdlg", "100% ahead", nullptr));
        slideshowRandomChk->setText(QString());
        label_12->setText(QApplication::translate("Prefdlg", "Show random order", nullptr));
#ifndef QT_NO_TOOLTIP
        slideshowDelaySpinbox->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>This sets the amount of time each image is displayed before the next image appears.  If set to zero then each image will be displayed until you manually move forward or backwards.  Note that you can also change this delay while the slideshow is active by pressing the zero to nine (0 - 9) keys.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
#ifndef QT_NO_TOOLTIP
        label_11->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>This sets the amount of time each image is displayed before the next image appears.  If set to zero then each image will be displayed until you manually move forward or backwards.  Note that you can also change this delay while the slideshow is active by pressing the zero to nine (0 - 9) keys.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        label_11->setText(QApplication::translate("Prefdlg", "Delay (sec)", nullptr));
        iconWidthLbl->setText(QString());
        iconPaddingLbl->setText(QString());
        thumbSpacingLbl->setText(QString());
        label_4->setText(QApplication::translate("Prefdlg", "Thumb spacing (px)", nullptr));
        label_5->setText(QApplication::translate("Prefdlg", "Icon padding (px)", nullptr));
        showThumbLabelChk->setText(QApplication::translate("Prefdlg", "Show thumbnail filename", nullptr));
        fontSizeLbl->setText(QString());
        label_2->setText(QApplication::translate("Prefdlg", "Icon width (px)", nullptr));
        lockDimChk->setText(QApplication::translate("Prefdlg", "Lock width = height", nullptr));
        label_3->setText(QApplication::translate("Prefdlg", "Icon height (px)", nullptr));
        iconHeightLbl->setText(QString());
        label_7->setText(QApplication::translate("Prefdlg", "Font size", nullptr));
        wrapChk->setText(QApplication::translate("Prefdlg", "Wrap thumbnails when docked on sides or when floating", nullptr));
        iconWidthLbl_2->setText(QString());
        iconPaddingLbl_2->setText(QApplication::translate("Prefdlg", "0", nullptr));
        thumbSpacingLbl_2->setText(QString());
        label_14->setText(QApplication::translate("Prefdlg", "Thumb spacing (px)", nullptr));
        label_15->setText(QApplication::translate("Prefdlg", "Icon padding (px)", nullptr));
        showThumbLabelChk_2->setText(QApplication::translate("Prefdlg", "Show thumbnail filename", nullptr));
        fontSizeLbl_2->setText(QString());
        label_17->setText(QApplication::translate("Prefdlg", "Icon width (px)", nullptr));
        lockDimChk_2->setText(QApplication::translate("Prefdlg", "Lock width = height", nullptr));
        label_18->setText(QApplication::translate("Prefdlg", "Icon height (px)", nullptr));
        iconHeightLbl_2->setText(QString());
        label_19->setText(QApplication::translate("Prefdlg", "Font size", nullptr));
        fullScreenExplanationLbl->setText(QApplication::translate("Prefdlg", "<html><head/><body><p>When full screen is chosen, select which panels should be visible. Hiding all the docks will result in only the loupe view or grid of all the thumbnails being visible.  You can still show/hide panels in the usual way, so this is just a customization to shortcut to your usual full screen preference.</p><p>When you leave full screen view the application will return to its prior state.</p></body></html>", nullptr));
        foldersChk->setText(QApplication::translate("Prefdlg", "Folders", nullptr));
        favsChk->setText(QApplication::translate("Prefdlg", "Favourites", nullptr));
        filtersChk->setText(QApplication::translate("Prefdlg", "Filters", nullptr));
        metadataChk->setText(QApplication::translate("Prefdlg", "Metadata", nullptr));
        thumbsChk->setText(QApplication::translate("Prefdlg", "Thumbnails", nullptr));
        statusBarChk->setText(QApplication::translate("Prefdlg", "Status Bar", nullptr));
#ifndef QT_NO_TOOLTIP
        frame1->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Select which items to show or hide in the metadata panel.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        label_6->setText(QApplication::translate("Prefdlg", "Metadata Panel Items", nullptr));
#ifndef QT_NO_TOOLTIP
        frame_3->setToolTip(QApplication::translate("Prefdlg", "<html><head/><body><p>Select which items to show or hide in the table mode view.</p></body></html>", nullptr));
#endif // QT_NO_TOOLTIP
        label_16->setText(QApplication::translate("Prefdlg", "Table Mode Columns", nullptr));

        const bool __sortingEnabled = listWidget->isSortingEnabled();
        listWidget->setSortingEnabled(false);
        QListWidgetItem *___qlistwidgetitem = listWidget->item(0);
        ___qlistwidgetitem->setText(QApplication::translate("Prefdlg", "General", nullptr));
        QListWidgetItem *___qlistwidgetitem1 = listWidget->item(1);
        ___qlistwidgetitem1->setText(QApplication::translate("Prefdlg", "Appearance", nullptr));
        QListWidgetItem *___qlistwidgetitem2 = listWidget->item(2);
        ___qlistwidgetitem2->setText(QApplication::translate("Prefdlg", "Cache", nullptr));
        QListWidgetItem *___qlistwidgetitem3 = listWidget->item(3);
        ___qlistwidgetitem3->setText(QApplication::translate("Prefdlg", "Slideshow", nullptr));
        QListWidgetItem *___qlistwidgetitem4 = listWidget->item(4);
        ___qlistwidgetitem4->setText(QApplication::translate("Prefdlg", "Thumbs in Dock", nullptr));
        QListWidgetItem *___qlistwidgetitem5 = listWidget->item(5);
        ___qlistwidgetitem5->setText(QApplication::translate("Prefdlg", "Thumbs in Grid View", nullptr));
        QListWidgetItem *___qlistwidgetitem6 = listWidget->item(6);
        ___qlistwidgetitem6->setText(QApplication::translate("Prefdlg", "Full screen default docks", nullptr));
        QListWidgetItem *___qlistwidgetitem7 = listWidget->item(7);
        ___qlistwidgetitem7->setText(QApplication::translate("Prefdlg", "Show or hide fields", nullptr));
        listWidget->setSortingEnabled(__sortingEnabled);

    } // retranslateUi

};

namespace Ui {
    class Prefdlg: public Ui_Prefdlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_PREFDLG_H
