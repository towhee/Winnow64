#include "preferences.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include <QDebug>

// this works because propertyeditor and preferences are friend classes of MW
MW *mw;

Preferences::Preferences(QWidget *parent): PropertyEditor(parent)
{
    mw = qobject_cast<MW*>(parent);
    connect(this->propertyDelegate, &PropertyDelegate::itemChanged,
            this, &Preferences::itemChange);
    captionColumnWidth = 225;
    valueColumnWidth = 200;
    addItems();
//    expandAll();
}

void Preferences::itemChange(QModelIndex idx)
{
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
    QString dataType = idx.data(UR_Type).toString();
    int delegateType = idx.data(UR_DelegateType).toInt();
    QModelIndex index = idx.data(UR_QModelIndex).toModelIndex();
    qDebug() << __FUNCTION__ << idx
             << "value =" << v
             << "source =" << source
             << "dataType =" << dataType
             << "delegateType =" << delegateType;

    if (source == "gridViewIconSize") {
        if (v == 1) {
            if (mw->gridView->isWrapping())
                mw->gridView->justify(IconView::JustifyAction::Enlarge);
            else mw->gridView->thumbsEnlarge();
        }
        else {
            if (mw->gridView->isWrapping())
                mw->gridView->justify(IconView::JustifyAction::Shrink);
            else mw->gridView->thumbsShrink();
        }
    }

    if (source == "gridViewShowLabel") {
        mw->gridView->showIconLabels = v.toBool();
        mw->gridView->setThumbParameters();
    }

    if (source == "gridViewLabelSize") {
        mw->gridView->labelFontSize = v.toInt();
        mw->gridView->setThumbParameters();
    }

    if (source == "thumbViewIconSize") {
        if (v == 1) {
            if (mw->thumbView->isWrapping())
                mw->thumbView->justify(IconView::JustifyAction::Enlarge);
            else mw->thumbView->thumbsEnlarge();
        }
        else {
            if (mw->thumbView->isWrapping())
                mw->thumbView->justify(IconView::JustifyAction::Shrink);
            else mw->thumbView->thumbsShrink();
        }
    }

    if (source == "thumbViewShowLabel") {
        mw->thumbView->showIconLabels = v.toBool();
        mw->thumbView->setThumbParameters();
    }

    if (source == "thumbViewLabelSize") {
        mw->thumbView->labelFontSize = v.toInt();
        mw->thumbView->setThumbParameters();
    }

    if (source == "classificationBadgeInImageDiameter") {
        int value = v.toInt();
        mw->setClassificationBadgeImageDiam(value);
    }

    if (source == "showThreadActivity") {
        mw->isShowCacheThreadActivity = v.toBool();
        mw->setCacheParameters();
    }

    if (source == "metadataChunkSize") {
        mw->metadataCacheThread->metadataChunkSize = v.toInt();
    }

    if (source == "maximumIconSize") {
        G::maxIconSize = v.toInt();
    }

    if (source == "metadataCacheStrategy") {
        if (v.toString() == "All") mw->metadataCacheThread->cacheAllMetadata = true;
        else mw->metadataCacheThread->cacheAllMetadata = false;
    }

    if (source == "thumbnailCacheStrategy") {
        if (v.toString() == "All") mw->metadataCacheThread->cacheAllIcons = true;
        else mw->metadataCacheThread->cacheAllIcons = false;
    }

    if (source == "cacheSizeMB") {
        qDebug() << __FUNCTION__ << v << source;
        mw->cacheSizeMB = v.toInt() /** 1024*/;
        mw->setCacheParameters();
    }

    if (source == "cacheWtAhead") {
        if (v.toString() == "50% ahead") mw->cacheWtAhead = 5;
        if (v.toString() == "60% ahead") mw->cacheWtAhead = 6;
        if (v.toString() == "70% ahead") mw->cacheWtAhead = 7;
        if (v.toString() == "80% ahead") mw->cacheWtAhead = 8;
        if (v.toString() == "90% ahead") mw->cacheWtAhead = 9;
        if (v.toString() == "100% ahead") mw->cacheWtAhead = 10;
         mw->setCacheParameters();
    }

    if (source == "progressWidthSlider") {
        mw->progressWidth = v.toInt();
        mw->setCacheParameters();
    }

    if (source == "showCacheStatus") {
        mw->isShowCacheStatus = v.toInt();
        mw->setCacheParameters();
    }

    if (source == "slideShowDelay") {
        qDebug() << __FUNCTION__ << v << source;
        mw->slideShowDelay = v.toInt();
    }

    if (source == "isSlideShowRandom") {
        mw->isSlideShowRandom = v.toBool();
        mw->slideShowResetSequence();
    }

    if (source == "isSlideShowWrap") {
        mw->isSlideShowWrap = v.toBool();
    }

    if (source == "classificationBadgeInThumbDiameter") {
        int value = v.toInt();
        mw->setClassificationBadgeThumbDiam(value);
    }

    if (source == "rememberLastDir") {
        mw->rememberLastDir = v.toBool();
    }

    if (source == "useWheelToScroll") {
        if (v.toString() == "Next/previous image") mw->imageView->useWheelToScroll = false;
        else mw->imageView->useWheelToScroll = true;
    }

    if (source == "globalFontSize") {
        mw->setFontSize(v.toInt());
    }

    if (source == "infoOverlayFontSize") {
        qDebug() << __FUNCTION__ << v;
        mw->imageView->infoOverlayFontSize = v.toInt();
        mw->setInfoFontSize();
    }

    if (source == "tableView->ok") {
        mw->tableView->ok->setData(index, v.toBool());
    }

    if (source == "infoView->ok") {
        mw->infoView->ok->setData(index, v.toBool());
    }
}

void Preferences::addItems()
{
    int firstGenerationCount = -1;        // top items
    int secondGenerationCount;            // child items
    int thirdGenerationCount;             // child child items
    QString tooltip;
    QString caption;
    QString s;                            // general purpose
    int n;                                // general purpose
    bool isShow;
    QModelIndex catIdx;
    QModelIndex valIdx;
    QStandardItem *captionItem;
    QStandardItem *valueItem;

    firstGenerationCount++;
    // HEADER
    // General category
    QStandardItem *generalItem = new QStandardItem;
    generalItem->setText("General");
    generalItem->setData(DT_None, UR_DelegateType);
    generalItem->setData("GENERAL", UR_Source);
    generalItem->setEditable(false);
    model->appendRow(generalItem);
    model->insertColumns(1, 1);
    catIdx = generalItem->index();
    setColumnWidth(0, captionColumnWidth);
    setColumnWidth(1, valueColumnWidth);
    secondGenerationCount = -1;

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = rememberFolder
        tooltip = "Remember the last folder used in Winnow from the previous session.";
        QStandardItem *rememberFolderCaption = new QStandardItem;
        rememberFolderCaption->setToolTip(tooltip);
        rememberFolderCaption->setText("Remember folder");
        rememberFolderCaption->setEditable(false);
        QStandardItem *rememberFolderValue = new QStandardItem;
        rememberFolderValue->setToolTip(tooltip);
        rememberFolderValue->setData(mw->rememberLastDir, Qt::EditRole);
        rememberFolderValue->setData(DT_Checkbox, UR_DelegateType);
        rememberFolderValue->setData("rememberLastDir", UR_Source);
        rememberFolderValue->setData("bool", UR_Type);
        generalItem->setChild(secondGenerationCount, 0, rememberFolderCaption);
        generalItem->setChild(secondGenerationCount, 1, rememberFolderValue);
        valIdx = rememberFolderValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        secondGenerationCount++;
        // Type = COMBOBOX
        // name = useWheelToScroll
        tooltip = "Use the trackpad or mouse wheel to either scroll when zoomed into an image\n"
                  "or to progress forward and backward through the images.";
        QStandardItem *useWheelToScrollCaption = new QStandardItem;
        useWheelToScrollCaption->setToolTip(tooltip);
        useWheelToScrollCaption->setText("Wheel/trackpad");
        useWheelToScrollCaption->setEditable(false);
        QStandardItem *useWheelToScrollValue = new QStandardItem;
        useWheelToScrollValue->setToolTip(tooltip);
        if (mw->imageView->useWheelToScroll) s = "Scroll current image when zoomed";
        if (!mw->imageView->useWheelToScroll) s = "Next/previous image";
        useWheelToScrollValue->setData(s, Qt::EditRole);
        useWheelToScrollValue->setData(DT_Combo, UR_DelegateType);
        useWheelToScrollValue->setData("useWheelToScroll", UR_Source);
        useWheelToScrollValue->setData("QString", UR_Type);
        QStringList scrollList = {"Next/previous image", "Scroll current image when zoomed"};
        useWheelToScrollValue->setData(scrollList, UR_StringList);
        generalItem->setChild(secondGenerationCount, 0, useWheelToScrollCaption);
        generalItem->setChild(secondGenerationCount, 1, useWheelToScrollValue);
        valIdx = useWheelToScrollValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        // HEADER
        // General category::Font size subcategory
        secondGenerationCount++;
        QStandardItem *fontSizeItem = new QStandardItem;
        fontSizeItem->setText("Font size");
        fontSizeItem->setEditable(false);
        fontSizeItem->setData(DT_None, UR_DelegateType);
        generalItem->setChild(secondGenerationCount, 0, fontSizeItem);
        thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = SLIDER
            // name = globalFontSize (for search and replace)
            // parent = fontSizeItem
            tooltip = "Change the font size throughout the application.";
            QStandardItem *globalFontSizeCaption = new QStandardItem;
            globalFontSizeCaption->setToolTip(tooltip);
            globalFontSizeCaption->setText("Global");
            globalFontSizeCaption->setEditable(false);
            QStandardItem *globalFontSizeValue = new QStandardItem;
            globalFontSizeValue->setToolTip(tooltip);
            globalFontSizeValue->setData(G::fontSize, Qt::EditRole);
            globalFontSizeValue->setData(DT_Slider, UR_DelegateType);
            globalFontSizeValue->setData("globalFontSize", UR_Source);
            globalFontSizeValue->setData("int", UR_Type);
            globalFontSizeValue->setData(6, UR_Min);
            globalFontSizeValue->setData(16, UR_Max);
            globalFontSizeValue->setData(50, UR_LabelFixedWidth);
            fontSizeItem->setChild(thirdGenerationCount, 0, globalFontSizeCaption);
            fontSizeItem->setChild(thirdGenerationCount, 1, globalFontSizeValue);
            valIdx = globalFontSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            thirdGenerationCount++;
            // Type = SLIDER
            // name = infoOverlayFontSize (for search and replace)
            // parent = fontSizeItem
            tooltip = "Change the font size for the info overlay (usually showing the shooting info in the top left of the image).";
            QStandardItem *infoOverlayFontSizeCaption = new QStandardItem;
            infoOverlayFontSizeCaption->setToolTip(tooltip);
            infoOverlayFontSizeCaption->setText("Info overlay");
            infoOverlayFontSizeCaption->setEditable(false);
            QStandardItem *infoOverlayFontSizeValue = new QStandardItem;
            infoOverlayFontSizeValue->setToolTip(tooltip);
            infoOverlayFontSizeValue->setData(mw->imageView->infoOverlayFontSize, Qt::EditRole);
            infoOverlayFontSizeValue->setData(DT_Slider, UR_DelegateType);
            infoOverlayFontSizeValue->setData("infoOverlayFontSize", UR_Source);
            infoOverlayFontSizeValue->setData("int", UR_Type);
            infoOverlayFontSizeValue->setData(6, UR_Min);
            infoOverlayFontSizeValue->setData(30, UR_Max);
            infoOverlayFontSizeValue->setData(50, UR_LabelFixedWidth);
            fontSizeItem->setChild(thirdGenerationCount, 0, infoOverlayFontSizeCaption);
            fontSizeItem->setChild(thirdGenerationCount, 1, infoOverlayFontSizeValue);
            valIdx = infoOverlayFontSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

    // HEADER
    // General category::Badge size subcategory
    secondGenerationCount++;
    QStandardItem *badgeSizeItem = new QStandardItem;
    badgeSizeItem->setText("Classification badge size");
    badgeSizeItem->setEditable(false);
    badgeSizeItem->setData(DT_None, UR_DelegateType);
    generalItem->setChild(secondGenerationCount, 0, badgeSizeItem);
    thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = SLIDER
            // name = thumbBadgeSize (for search and replace)
            // parent = badgeSizeItem
            tooltip = "The image badge is a circle showing the colour classification, rating and pick\n"
                      "status.  It is located in the lower right corner of the image.  This property \n"
                      "allows you to adjust its size.";
            QStandardItem *imBadgeSizeCaption = new QStandardItem;
            imBadgeSizeCaption->setToolTip(tooltip);
            imBadgeSizeCaption->setText("Loupe");
            imBadgeSizeCaption->setEditable(false);
            QStandardItem *imageBadgeSizeValue = new QStandardItem;
            imageBadgeSizeValue->setToolTip(tooltip);
            imageBadgeSizeValue->setData(mw->classificationBadgeInImageDiameter, Qt::EditRole);
            imageBadgeSizeValue->setData(DT_Slider, UR_DelegateType);
            imageBadgeSizeValue->setData("classificationBadgeInImageDiameter", UR_Source);
            imageBadgeSizeValue->setData("int", UR_Type);
            imageBadgeSizeValue->setData(10, UR_Min);
            imageBadgeSizeValue->setData(100, UR_Max);
            imageBadgeSizeValue->setData(50, UR_LabelFixedWidth);
            badgeSizeItem->setChild(thirdGenerationCount, 0, imBadgeSizeCaption);
            badgeSizeItem->setChild(thirdGenerationCount, 1, imageBadgeSizeValue);
            valIdx = imageBadgeSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            thirdGenerationCount++;
            // Type = SLIDER
            // name = thumbBadgeSize (for search and replace)
            // parent = badgeSizeItem
            tooltip = "The image badge is a circle showing the colour classification, rating and pick\n"
                      "status.  It is located in the top right corner of the thumbnail in the thumb \n"
                      "and grid views.  This property allows you to adjust its size.";
            QStandardItem *thumbBadgeSizeCaption = new QStandardItem;
            thumbBadgeSizeCaption->setToolTip(tooltip);
            thumbBadgeSizeCaption->setText("Thumbnail");
            thumbBadgeSizeCaption->setEditable(false);
            QStandardItem *thumbBadgeSizeValue = new QStandardItem;
            thumbBadgeSizeValue->setToolTip(tooltip);
            thumbBadgeSizeValue->setData(mw->classificationBadgeInThumbDiameter, Qt::EditRole);
            thumbBadgeSizeValue->setData(DT_Slider, UR_DelegateType);
            thumbBadgeSizeValue->setData("classificationBadgeInThumbDiameter", UR_Source);
            thumbBadgeSizeValue->setData("int", UR_Type);
            thumbBadgeSizeValue->setData(10, UR_Min);
            thumbBadgeSizeValue->setData(100, UR_Max);
            thumbBadgeSizeValue->setData(50, UR_LabelFixedWidth);
            badgeSizeItem->setChild(thirdGenerationCount, 0, thumbBadgeSizeCaption);
            badgeSizeItem->setChild(thirdGenerationCount, 1, thumbBadgeSizeValue);
            valIdx = thumbBadgeSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

    firstGenerationCount++;
    // HEADER
    // Thumbnail category
    QStandardItem *thumbnailCatItem = new QStandardItem;
    thumbnailCatItem->setText("Thumbnails");
    thumbnailCatItem->setEditable(false);
    thumbnailCatItem->setData(DT_None, UR_DelegateType);
    model->appendRow(thumbnailCatItem);
    setColumnWidth(0, captionColumnWidth);
    setColumnWidth(1, valueColumnWidth);
    secondGenerationCount = -1;

        // HEADER
        // Thumbnails:: film strip subcategory
        // Parent = thumbnailCatItem
        secondGenerationCount++;
        QStandardItem *thumbnailFilmStripCatItem = new QStandardItem;
        QStandardItem *thumbnailFilmStripCatItem1 = new QStandardItem;  //req'd to paint alt background on all siblings with children
        thumbnailFilmStripCatItem->setText("Film strip");
        thumbnailFilmStripCatItem->setEditable(false);
        thumbnailFilmStripCatItem->setData(DT_None, UR_DelegateType);
        thumbnailCatItem->setChild(secondGenerationCount, 0, thumbnailFilmStripCatItem);
        thumbnailCatItem->setChild(secondGenerationCount, 1, thumbnailFilmStripCatItem1);
        thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = PLUSMINUS
            // name = thumbViewIconSize (for search and replace)
            // parent = thumbnailFilmStripCatItem
            tooltip = "Change the display size of the thumbnails in the film strip.";
            QStandardItem *thumbViewIconSizeCaption = new QStandardItem;
            thumbViewIconSizeCaption->setToolTip(tooltip);
            thumbViewIconSizeCaption->setText("Size");
            thumbViewIconSizeCaption->setEditable(false);
            QStandardItem *thumbViewIconSizeValue = new QStandardItem;
            thumbViewIconSizeValue->setToolTip(tooltip);
            thumbViewIconSizeValue->setData(120, Qt::EditRole);  // figure this out
            thumbViewIconSizeValue->setData(DT_PlusMinus, UR_DelegateType);
            thumbViewIconSizeValue->setData("thumbViewIconSize", UR_Source);
            thumbViewIconSizeValue->setData("int", UR_Type);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 0, thumbViewIconSizeCaption);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 1, thumbViewIconSizeValue);
            valIdx = thumbViewIconSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            thirdGenerationCount++;
            // Type = SLIDER
            // name = thumbViewLabelSize (for search and replace)
            // parent = thumbnailFilmStripCatItem
            tooltip = "Change the display size of the thumbnails in the film strip.";
            QStandardItem *thumbViewLabelSizeCaption = new QStandardItem;
            thumbViewLabelSizeCaption->setToolTip(tooltip);
            thumbViewLabelSizeCaption->setText("Label size");
            thumbViewLabelSizeCaption->setEditable(false);
            QStandardItem *thumbViewLabelSizeValue = new QStandardItem;
            thumbViewLabelSizeValue->setToolTip(tooltip);
            thumbViewLabelSizeValue->setData(mw->thumbView->labelFontSize, Qt::EditRole);  // figure this out
            thumbViewLabelSizeValue->setData(DT_Slider, UR_DelegateType);
            thumbViewLabelSizeValue->setData("thumbViewLabelSize", UR_Source);
            thumbViewLabelSizeValue->setData("int", UR_Type);
            thumbViewLabelSizeValue->setData(6, UR_Min);
            thumbViewLabelSizeValue->setData(16, UR_Max);
            thumbViewLabelSizeValue->setData(50, UR_LabelFixedWidth);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 0, thumbViewLabelSizeCaption);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 1, thumbViewLabelSizeValue);
            valIdx = thumbViewLabelSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            thirdGenerationCount++;
            // Type = CHECKBOX
            // name = thumbViewShowLabel
            tooltip = "Remember the last folder used in Winnow from the previous session.";
            QStandardItem *thumbViewShowLabelCaption = new QStandardItem;
            thumbViewShowLabelCaption->setToolTip(tooltip);
            thumbViewShowLabelCaption->setText("Show label");
            thumbViewShowLabelCaption->setEditable(false);
            QStandardItem *thumbViewShowLabelValue = new QStandardItem;
            thumbViewShowLabelValue->setToolTip(tooltip);
            thumbViewShowLabelValue->setData(mw->thumbView->showIconLabels, Qt::EditRole);
            thumbViewShowLabelValue->setData(DT_Checkbox, UR_DelegateType);
            thumbViewShowLabelValue->setData("thumbViewShowLabel", UR_Source);
            thumbViewShowLabelValue->setData("bool", UR_Type);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 0, thumbViewShowLabelCaption);
            thumbnailFilmStripCatItem->setChild(thirdGenerationCount, 1, thumbViewShowLabelValue);
            valIdx = thumbViewShowLabelValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            // HEADER
            // Thumbnails:: grid subcategory
            secondGenerationCount++;
            QStandardItem *thumbnailGridCatItem = new QStandardItem;
            thumbnailGridCatItem->setText("Grid");
            thumbnailGridCatItem->setEditable(false);
            thumbnailGridCatItem->setData(DT_None, UR_DelegateType);
            thumbnailCatItem->setChild(secondGenerationCount, 0, thumbnailGridCatItem);
            thirdGenerationCount = -1;

                thirdGenerationCount++;
                // Type = PLUSMINUS
                // name = gridViewIconSize (for search and replace)
                // parent = thumbnailGridCatItem
                tooltip = "Change the display size of the thumbnails in the film strip.";
                QStandardItem *gridViewIconSizeCaption = new QStandardItem;
                gridViewIconSizeCaption->setToolTip(tooltip);
                gridViewIconSizeCaption->setText("Size");
                gridViewIconSizeCaption->setEditable(false);
                QStandardItem *gridViewIconSizeValue = new QStandardItem;
                gridViewIconSizeValue->setToolTip(tooltip);
                gridViewIconSizeValue->setData(120, Qt::EditRole);  // figure this out
                gridViewIconSizeValue->setData(DT_PlusMinus, UR_DelegateType);
                gridViewIconSizeValue->setData("gridViewIconSize", UR_Source);
                gridViewIconSizeValue->setData("int", UR_Type);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 0, gridViewIconSizeCaption);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 1, gridViewIconSizeValue);
                valIdx = gridViewIconSizeValue->index();
                propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

                thirdGenerationCount++;
                // Type = SLIDER
                // name = gridViewLabelSize (for search and replace)
                // parent = thumbnailGridCatItem
                tooltip = "Change the display size of the thumbnails in the film strip.";
                QStandardItem *gridViewLabelSizeCaption = new QStandardItem;
                gridViewLabelSizeCaption->setToolTip(tooltip);
                gridViewLabelSizeCaption->setText("Label size");
                gridViewLabelSizeCaption->setEditable(false);
                QStandardItem *gridViewLabelSizeValue = new QStandardItem;
                gridViewLabelSizeValue->setToolTip(tooltip);
                gridViewLabelSizeValue->setData(mw->gridView->labelFontSize, Qt::EditRole);  // figure this out
                gridViewLabelSizeValue->setData(DT_Slider, UR_DelegateType);
                gridViewLabelSizeValue->setData("gridViewLabelSize", UR_Source);
                gridViewLabelSizeValue->setData("int", UR_Type);
                gridViewLabelSizeValue->setData(6, UR_Min);
                gridViewLabelSizeValue->setData(16, UR_Max);
                gridViewLabelSizeValue->setData(50, UR_LabelFixedWidth);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 0, gridViewLabelSizeCaption);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 1, gridViewLabelSizeValue);
                valIdx = gridViewLabelSizeValue->index();
                propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

                thirdGenerationCount++;
                // Type = CHECKBOX
                // name = gridViewShowLabel
                tooltip = "Remember the last folder used in Winnow from the previous session.";
                QStandardItem *gridViewShowLabelCaption = new QStandardItem;
                gridViewShowLabelCaption->setToolTip(tooltip);
                gridViewShowLabelCaption->setText("Show label");
                gridViewShowLabelCaption->setEditable(false);
                QStandardItem *gridViewShowLabelValue = new QStandardItem;
                gridViewShowLabelValue->setToolTip(tooltip);
                gridViewShowLabelValue->setData(mw->gridView->showIconLabels, Qt::EditRole);
                gridViewShowLabelValue->setData(DT_Checkbox, UR_DelegateType);
                gridViewShowLabelValue->setData("gridViewShowLabel", UR_Source);
                gridViewShowLabelValue->setData("bool", UR_Type);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 0, gridViewShowLabelCaption);
                thumbnailGridCatItem->setChild(thirdGenerationCount, 1, gridViewShowLabelValue);
                valIdx = gridViewShowLabelValue->index();
                propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

    firstGenerationCount++;
    // HEADER
    // Cache category
    QStandardItem *cacheCatItem = new QStandardItem;
    cacheCatItem->setText("Cache");
    cacheCatItem->setEditable(false);
    cacheCatItem->setData(DT_None, UR_DelegateType);
    model->appendRow(cacheCatItem);
    //    model->insertColumns(1, 1);
    setColumnWidth(0, captionColumnWidth);
    setColumnWidth(1, valueColumnWidth);
    secondGenerationCount = -1;

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = showThreadActivity
        // parent = cacheCatItem
        tooltip = "Two small indicators on the extreme right side of the status bar turn red\n"
                  "when there is caching activity.  The left indicator is for matadata caching\n"
                  "activity, while the right indicator shows image caching activity.  This\n"
                  "preference shows or hides the indicators.";
        QStandardItem *showThreadActivityCaption = new QStandardItem;
        showThreadActivityCaption->setToolTip(tooltip);
        showThreadActivityCaption->setText("Show caching activity");
        showThreadActivityCaption->setEditable(false);
        QStandardItem *showThreadActivityValue = new QStandardItem;
        showThreadActivityValue->setToolTip(tooltip);
        showThreadActivityValue->setData(mw->isShowCacheThreadActivity, Qt::EditRole);
        showThreadActivityValue->setData(DT_Checkbox, UR_DelegateType);
        showThreadActivityValue->setData("showThreadActivity", UR_Source);
        showThreadActivityValue->setData("bool", UR_Type);
        cacheCatItem->setChild(secondGenerationCount, 0, showThreadActivityCaption);
        cacheCatItem->setChild(secondGenerationCount, 1, showThreadActivityValue);
        valIdx = showThreadActivityValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        // HEADER
        // Cache:: Metadata subcategory
        secondGenerationCount++;
        QStandardItem *metadataCatItem = new QStandardItem;
        QStandardItem *metadataCatItem1 = new QStandardItem;
        metadataCatItem->setText("Metadata");
        metadataCatItem->setEditable(false);
        metadataCatItem->setData(DT_None, UR_DelegateType);
        cacheCatItem->setChild(secondGenerationCount, 0, metadataCatItem);
        cacheCatItem->setChild(secondGenerationCount, 1, metadataCatItem1);
        thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = COMBO
            // name = metadataCacheStrategy
            tooltip = "If you cache the metadata for all the images in the folder(s) it will take\n"
                      "longer to initially to get started but performance might be better.  Alternatively\n"
                      "you can incrementally load the metadata as needed, and for larger folders with\n"
                      "thousands of images this might be quicker.";
            QStandardItem *metadataCacheStrategyCaption = new QStandardItem;
            metadataCacheStrategyCaption->setToolTip(tooltip);
            metadataCacheStrategyCaption->setText("Strategy");
            metadataCacheStrategyCaption->setEditable(false);
            QStandardItem *metadataCacheStrategyValue = new QStandardItem;
            metadataCacheStrategyValue->setToolTip(tooltip);
            s = "Incremental";
            if (mw->metadataCacheThread->cacheAllMetadata) s = "All";
            metadataCacheStrategyValue->setData(s, Qt::EditRole);
            metadataCacheStrategyValue->setData(DT_Combo, UR_DelegateType);
            metadataCacheStrategyValue->setData("metadataCacheStrategy", UR_Source);
            metadataCacheStrategyValue->setData("QString", UR_Type);
            QStringList list1 = {"All", "Incremental"};
            metadataCacheStrategyValue->setData(list1, UR_StringList);
            metadataCatItem->setChild(thirdGenerationCount, 0, metadataCacheStrategyCaption);
            metadataCatItem->setChild(thirdGenerationCount, 1, metadataCacheStrategyValue);
            valIdx = metadataCacheStrategyValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        // HEADER
        // Cache:: CacheThumbnail subcategory
        secondGenerationCount++;
        QStandardItem *cacheThumbnailCatItem = new QStandardItem;
        cacheThumbnailCatItem->setText("Thumbnails");
        cacheThumbnailCatItem->setEditable(false);
        cacheThumbnailCatItem->setData(DT_None, UR_DelegateType);
        cacheCatItem->setChild(secondGenerationCount, 0, cacheThumbnailCatItem);
        thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = COMBO
            // name = thumbnailCacheStrategy
            tooltip = "If you cache the thumbnails for all the images in the folder(s) it will take\n"
                      "longer to initially to get started and can consume huge amounts of memory \n"
                      "but performance might be better while scrolling.  Alternatively you can\n"
                      "incrementally load the thumbnails as needed, and for larger folders with\n"
                      "thousands of images this is usually the better choice.";
            QStandardItem *thumbnailCacheStrategyCaption = new QStandardItem;
            thumbnailCacheStrategyCaption->setToolTip(tooltip);
            thumbnailCacheStrategyCaption->setText("Strategy");
            thumbnailCacheStrategyCaption->setEditable(false);
            QStandardItem *thumbnailCacheStrategyValue = new QStandardItem;
            thumbnailCacheStrategyValue->setToolTip(tooltip);
            s = "Incremental";
            if (mw->metadataCacheThread->cacheAllIcons) s = "All";
            thumbnailCacheStrategyValue->setData(s, Qt::EditRole);
            thumbnailCacheStrategyValue->setData(DT_Combo, UR_DelegateType);
            thumbnailCacheStrategyValue->setData("thumbnailCacheStrategy", UR_Source);
            thumbnailCacheStrategyValue->setData("QString", UR_Type);
            QStringList list2 = {"All", "Incremental"};
            thumbnailCacheStrategyValue->setData(list1, UR_StringList);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 0, thumbnailCacheStrategyCaption);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 1, thumbnailCacheStrategyValue);
            valIdx = thumbnailCacheStrategyValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            thirdGenerationCount++;
            // Type = SPINBOX
            // name = metadataChunkSize
            // parent = cacheThumbnailCatItem
            tooltip = "Enter the number of thumbnails and metadata you want to cache.  If the\n"
                      "grid is displaying a larger number then the larger number will be used\n"
                      "to make sure they are all shown.  You can experiment to see what works\n"
                      "best.  250 is the default amount.";
            QStandardItem *metadataChunkSizeCaption = new QStandardItem;
            metadataChunkSizeCaption->setToolTip(tooltip);
            metadataChunkSizeCaption->setText("Incremental amount to load");
            metadataChunkSizeCaption->setEditable(false);
            QStandardItem *metadataChunkSizeValue = new QStandardItem;
            metadataChunkSizeValue->setToolTip(tooltip);
            metadataChunkSizeValue->setData(mw->metadataCacheThread->metadataChunkSize, Qt::EditRole);
            metadataChunkSizeValue->setData(DT_Spinbox, UR_DelegateType);
            metadataChunkSizeValue->setData("metadataChunkSize", UR_Source);
            metadataChunkSizeValue->setData("int", UR_Type);
            metadataChunkSizeValue->setData(1, UR_Min);
            metadataChunkSizeValue->setData(3000, UR_Max);
            metadataChunkSizeValue->setData(50, UR_LineEditFixedWidth);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 0, metadataChunkSizeCaption);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 1, metadataChunkSizeValue);
            valIdx = metadataChunkSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            thirdGenerationCount++;
            // Type = SPINBOX
            // name = maximumIconSize
            // parent = cacheThumbnailCatItem
            tooltip = "Enter the maximum size in pixel width for thumbnails.  Icons will be \nt"
                      "created a this size.  The memory requirements increase at the square of \n"
                      "the size, and folders can contain thousands of images.\n\n"
                      "WARNING: Larger thumbnail sizes can consume huge amounts of memory.  The\n"
                      "default size is 256 pixels to a side.";
            QStandardItem *maximumIconSizeCaption = new QStandardItem;
            maximumIconSizeCaption->setToolTip(tooltip);
            maximumIconSizeCaption->setText("Maximum icon size");
            maximumIconSizeCaption->setEditable(false);
            QStandardItem *maximumIconSizeValue = new QStandardItem;
            maximumIconSizeValue->setToolTip(tooltip);
            maximumIconSizeValue->setData(G::maxIconSize, Qt::EditRole);
            maximumIconSizeValue->setData(DT_Spinbox, UR_DelegateType);
            maximumIconSizeValue->setData("maxIconSize", UR_Source);
            maximumIconSizeValue->setData("int", UR_Type);
            maximumIconSizeValue->setData(40, UR_Min);
            maximumIconSizeValue->setData(640, UR_Max);
            maximumIconSizeValue->setData(50, UR_LineEditFixedWidth);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 0, maximumIconSizeCaption);
            cacheThumbnailCatItem->setChild(thirdGenerationCount, 1, maximumIconSizeValue);
            valIdx = maximumIconSizeValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        // HEADER
        // Cache:: Full image subcategory
        secondGenerationCount++;
        QStandardItem *imageCatItem = new QStandardItem;
        QStandardItem *imageCatItem1 = new QStandardItem;
        imageCatItem->setText("Full size images");
        imageCatItem->setEditable(false);
        imageCatItem->setData(DT_None, UR_DelegateType);
        cacheCatItem->setChild(secondGenerationCount, 0, imageCatItem);
        cacheCatItem->setChild(secondGenerationCount, 1, imageCatItem1);
        thirdGenerationCount = -1;

            thirdGenerationCount++;
            // Type = SPINBOX
            // name = cacheSizeMB
            // parent = imageCatItem
            tooltip = "<html><head/><body><p>Experiment with different cache sizes.  2-8GB "
                      "appear to work best.  Cache performance does not always improve with "
                      "size.</p><p>If you tend to move back and forth between images then a "
                      "50% cache ahead strategy is best.  If you generally just move ahead "
                      "through the images then weighting a higher percentage ahead makes "
                      "sense.</p></body></html>";
            QStandardItem *cacheSizeMBCaption = new QStandardItem;
            cacheSizeMBCaption->setToolTip(tooltip);
            cacheSizeMBCaption->setText("Image cache size (MB)");
            cacheSizeMBCaption->setEditable(false);
            QStandardItem *cacheSizeMBValue = new QStandardItem;
            cacheSizeMBValue->setToolTip(tooltip);
            cacheSizeMBValue->setData(mw->cacheSizeMB /*/ 1024*/, Qt::EditRole);
            cacheSizeMBValue->setData(DT_Spinbox, UR_DelegateType);
            cacheSizeMBValue->setData("cacheSizeMB", UR_Source);
            cacheSizeMBValue->setData("int", UR_Type);
            cacheSizeMBValue->setData(1, UR_Min);
            cacheSizeMBValue->setData(32000, UR_Max);
            cacheSizeMBValue->setData(50, UR_LineEditFixedWidth);
            imageCatItem->setChild(thirdGenerationCount, 0, cacheSizeMBCaption);
            imageCatItem->setChild(thirdGenerationCount, 1, cacheSizeMBValue);
            valIdx = cacheSizeMBValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            thirdGenerationCount++;
            // Type = COMBOBOX
            // name = cacheWtAhead
            // parent = imageCatItem
            tooltip = "<html><head/><body><p>Experiment with different cache sizes.  2-8GB "
                      "appear to work best.  Cache performance does not always improve with "
                      "size.</p><p>If you tend to move back and forth between images then a "
                      "50% cache ahead strategy is best.  If you generally just move ahead "
                      "through the images then weighting a higher percentage ahead makes "
                      "sense.</p></body></html>";
            QStandardItem *cacheWtAheadCaption = new QStandardItem;
            cacheWtAheadCaption->setToolTip(tooltip);
            cacheWtAheadCaption->setText("Image cache strategy");
            cacheWtAheadCaption->setEditable(false);
            QStandardItem *cacheWtAheadValue = new QStandardItem;
            cacheWtAheadValue->setToolTip(tooltip);
            if (mw->cacheWtAhead == 5) s = "50% ahead";
            if (mw->cacheWtAhead == 6) s = "60% ahead";
            if (mw->cacheWtAhead == 7) s = "70% ahead";
            if (mw->cacheWtAhead == 8) s = "80% ahead";
            if (mw->cacheWtAhead == 9) s = "90% ahead";
            if (mw->cacheWtAhead == 10) s = "100% ahead";
            cacheWtAheadValue->setData(s, Qt::EditRole);
            cacheWtAheadValue->setData(DT_Combo, UR_DelegateType);
            cacheWtAheadValue->setData("cacheWtAhead", UR_Source);
            cacheWtAheadValue->setData("QString", UR_Type);
            QStringList cacheWtList = {"50% ahead", "60% ahead", "70% ahead", "80% ahead", "90% ahead", "100% ahead"};
            cacheWtAheadValue->setData(cacheWtList, UR_StringList);
            imageCatItem->setChild(thirdGenerationCount, 0, cacheWtAheadCaption);
            imageCatItem->setChild(thirdGenerationCount, 1, cacheWtAheadValue);
            valIdx = cacheWtAheadValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            thirdGenerationCount++;
            // Type = CHECKBOX
            // name = showCacheStatus
            // parent = imageCatItem
            tooltip = "The image cache status shows the current state of the images targeted to\n"
                      "cache and images already cached.  This can be helpful in understanding\n"
                      "what Winnow is doing and making decisions on the image cache size.";
            QStandardItem *showCacheStatusCaption = new QStandardItem;
            showCacheStatusCaption->setToolTip(tooltip);
            showCacheStatusCaption->setText("Show image cache status");
            showCacheStatusCaption->setEditable(false);
            QStandardItem *showCacheStatusValue = new QStandardItem;
            showCacheStatusValue->setToolTip(tooltip);
            showCacheStatusValue->setData(mw->isShowCacheStatus, Qt::EditRole);
            showCacheStatusValue->setData(DT_Checkbox, UR_DelegateType);
            showCacheStatusValue->setData("showCacheStatus", UR_Source);
            showCacheStatusValue->setData("bool", UR_Type);
            imageCatItem->setChild(thirdGenerationCount, 0, showCacheStatusCaption);
            imageCatItem->setChild(thirdGenerationCount, 1, showCacheStatusValue);
            valIdx = showCacheStatusValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

            thirdGenerationCount++;
            // Type = SLIDER
            // name = progressWidthSlider
            // parent = imageCatItem
            tooltip = "Change the width of the cache status in the status bar.";
            QStandardItem *progressWidthSliderCaption = new QStandardItem;
            progressWidthSliderCaption->setToolTip(tooltip);
            progressWidthSliderCaption->setText("Cache status bar width");
            progressWidthSliderCaption->setEditable(false);
            QStandardItem *progressWidthSliderValue = new QStandardItem;
            progressWidthSliderValue->setToolTip(tooltip);
            progressWidthSliderValue->setData(mw->progressWidth, Qt::EditRole);
            progressWidthSliderValue->setData(DT_Slider, UR_DelegateType);
            progressWidthSliderValue->setData("progressWidthSlider", UR_Source);
            progressWidthSliderValue->setData("int", UR_Type);
            progressWidthSliderValue->setData(100, UR_Min);
            progressWidthSliderValue->setData(800, UR_Max);
            progressWidthSliderValue->setData(50, UR_LabelFixedWidth);
            imageCatItem->setChild(thirdGenerationCount, 0, progressWidthSliderCaption);
            imageCatItem->setChild(thirdGenerationCount, 1, progressWidthSliderValue);
            valIdx = progressWidthSliderValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

    firstGenerationCount++;
    // HEADER
    // Slideshow category
    QStandardItem *slideshowCatItem = new QStandardItem;
    slideshowCatItem->setText("Slideshow");
    slideshowCatItem->setEditable(false);
    slideshowCatItem->setData(DT_None, UR_DelegateType);
    model->appendRow(slideshowCatItem);
    setColumnWidth(0, captionColumnWidth);
    setColumnWidth(1, valueColumnWidth);
    secondGenerationCount = -1;

        secondGenerationCount++;
        // Type = SPINBOX
        // name = slideshowDelay
        tooltip = "Enter the slideshow delay in seconds.";
        QStandardItem *slideshowDelayCaption = new QStandardItem;
        slideshowDelayCaption->setToolTip(tooltip);
        slideshowDelayCaption->setText("Slideshow delay (sec)");
        slideshowDelayCaption->setEditable(false);
        QStandardItem *slideshowDelayValue = new QStandardItem;
        slideshowDelayValue->setToolTip(tooltip);
        slideshowDelayValue->setData(mw->slideShowDelay, Qt::EditRole);
        slideshowDelayValue->setData(DT_Spinbox, UR_DelegateType);
        slideshowDelayValue->setData("slideShowDelay", UR_Source);
        slideshowDelayValue->setData("int", UR_Type);
        slideshowDelayValue->setData(1, UR_Min);
        slideshowDelayValue->setData(300, UR_Max);
        slideshowDelayValue->setData(50, UR_LineEditFixedWidth);
        slideshowCatItem->setChild(secondGenerationCount, 0, slideshowDelayCaption);
        slideshowCatItem->setChild(secondGenerationCount, 1, slideshowDelayValue);
        valIdx = slideshowDelayValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = randomSlideshow
        tooltip = "Selects random slides if checked.  Otherwise the slide selection is \n"
                  "sequential, based on the sort order.";
        QStandardItem *randomSlideshowCaption = new QStandardItem;
        randomSlideshowCaption->setToolTip(tooltip);
        randomSlideshowCaption->setText("Random slide selection");
        randomSlideshowCaption->setEditable(false);
        QStandardItem *randomSlideshowValue = new QStandardItem;
        randomSlideshowValue->setToolTip(tooltip);
        randomSlideshowValue->setData(mw->isSlideShowRandom, Qt::EditRole);
        randomSlideshowValue->setData(DT_Checkbox, UR_DelegateType);
        randomSlideshowValue->setData("isSlideShowRandom", UR_Source);
        randomSlideshowValue->setData("bool", UR_Type);
        slideshowCatItem->setChild(secondGenerationCount, 0, randomSlideshowCaption);
        slideshowCatItem->setChild(secondGenerationCount, 1, randomSlideshowValue);
        valIdx = randomSlideshowValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = wrapSlideshow
        tooltip = "If not in wrap mode when the slideshow it wraps back to the start.";
        QStandardItem *wrapSlideshowCaption = new QStandardItem;
        wrapSlideshowCaption->setToolTip(tooltip);
        wrapSlideshowCaption->setText("Wrap slide selection");
        wrapSlideshowCaption->setEditable(false);
        QStandardItem *wrapSlideshowValue = new QStandardItem;
        wrapSlideshowValue->setToolTip(tooltip);
        wrapSlideshowValue->setData(mw->isSlideShowWrap, Qt::EditRole);
        wrapSlideshowValue->setData(DT_Checkbox, UR_DelegateType);
        wrapSlideshowValue->setData("isSlideShowWrap", UR_Source);
        wrapSlideshowValue->setData("bool", UR_Type);
        slideshowCatItem->setChild(secondGenerationCount, 0, wrapSlideshowCaption);
        slideshowCatItem->setChild(secondGenerationCount, 1, wrapSlideshowValue);
        valIdx = wrapSlideshowValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

    firstGenerationCount++;
    // HEADER
    // Full screen category
    QStandardItem *fullScreenCatItem = new QStandardItem;
    fullScreenCatItem->setText("Full screen defaults");
    fullScreenCatItem->setEditable(false);
    fullScreenCatItem->setData(DT_None, UR_DelegateType);
    model->appendRow(fullScreenCatItem);
    setColumnWidth(0, captionColumnWidth);
    setColumnWidth(1, valueColumnWidth);
    secondGenerationCount = -1;

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = fullScreenShowFolders
        tooltip = "When you switch to full screen show the folders dock.";
        QStandardItem *fullScreenShowFoldersCaption = new QStandardItem;
        fullScreenShowFoldersCaption->setToolTip(tooltip);
        fullScreenShowFoldersCaption->setText("Show folders");
        fullScreenShowFoldersCaption->setEditable(false);
        QStandardItem *fullScreenShowFoldersValue = new QStandardItem;
        fullScreenShowFoldersValue->setToolTip(tooltip);
        fullScreenShowFoldersValue->setData(mw->fullScreenDocks.isFolders, Qt::EditRole);
        fullScreenShowFoldersValue->setData(DT_Checkbox, UR_DelegateType);
        fullScreenShowFoldersValue->setData("fullScreenShowFolders", UR_Source);
        fullScreenShowFoldersValue->setData("bool", UR_Type);
        fullScreenCatItem->setChild(secondGenerationCount, 0, fullScreenShowFoldersCaption);
        fullScreenCatItem->setChild(secondGenerationCount, 1, fullScreenShowFoldersValue);
        valIdx = fullScreenShowFoldersValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = fullScreenShowBookmarks
        tooltip = "When you switch to full screen show the Bookmarks dock.";
        QStandardItem *fullScreenShowBookmarksCaption = new QStandardItem;
        fullScreenShowBookmarksCaption->setToolTip(tooltip);
        fullScreenShowBookmarksCaption->setText("Show bookmarks");
        fullScreenShowBookmarksCaption->setEditable(false);
        QStandardItem *fullScreenShowBookmarksValue = new QStandardItem;
        fullScreenShowBookmarksValue->setToolTip(tooltip);
        fullScreenShowBookmarksValue->setData(mw->fullScreenDocks.isFavs, Qt::EditRole);
        fullScreenShowBookmarksValue->setData(DT_Checkbox, UR_DelegateType);
        fullScreenShowBookmarksValue->setData("fullScreenShowBookmarks", UR_Source);
        fullScreenShowBookmarksValue->setData("bool", UR_Type);
        fullScreenCatItem->setChild(secondGenerationCount, 0, fullScreenShowBookmarksCaption);
        fullScreenCatItem->setChild(secondGenerationCount, 1, fullScreenShowBookmarksValue);
        valIdx = fullScreenShowBookmarksValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = fullScreenShowFilters
        tooltip = "When you switch to full screen show the Filters dock.";
        QStandardItem *fullScreenShowFiltersCaption = new QStandardItem;
        fullScreenShowFiltersCaption->setToolTip(tooltip);
        fullScreenShowFiltersCaption->setText("Show filters");
        fullScreenShowFiltersCaption->setEditable(false);
        QStandardItem *fullScreenShowFiltersValue = new QStandardItem;
        fullScreenShowFiltersValue->setToolTip(tooltip);
        fullScreenShowFiltersValue->setData(mw->fullScreenDocks.isFilters, Qt::EditRole);
        fullScreenShowFiltersValue->setData(DT_Checkbox, UR_DelegateType);
        fullScreenShowFiltersValue->setData("fullScreenShowFilters", UR_Source);
        fullScreenShowFiltersValue->setData("bool", UR_Type);
        fullScreenCatItem->setChild(secondGenerationCount, 0, fullScreenShowFiltersCaption);
        fullScreenCatItem->setChild(secondGenerationCount, 1, fullScreenShowFiltersValue);
        valIdx = fullScreenShowFiltersValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = fullScreenShowMetadata
        tooltip = "When you switch to full screen show the Metadata dock.";
        QStandardItem *fullScreenShowMetadataCaption = new QStandardItem;
        fullScreenShowMetadataCaption->setToolTip(tooltip);
        fullScreenShowMetadataCaption->setText("Show metadata");
        fullScreenShowMetadataCaption->setEditable(false);
        QStandardItem *fullScreenShowMetadataValue = new QStandardItem;
        fullScreenShowMetadataValue->setToolTip(tooltip);
        fullScreenShowMetadataValue->setData(mw->fullScreenDocks.isMetadata, Qt::EditRole);
        fullScreenShowMetadataValue->setData(DT_Checkbox, UR_DelegateType);
        fullScreenShowMetadataValue->setData("fullScreenShowMetadata", UR_Source);
        fullScreenShowMetadataValue->setData("bool", UR_Type);
        fullScreenCatItem->setChild(secondGenerationCount, 0, fullScreenShowMetadataCaption);
        fullScreenCatItem->setChild(secondGenerationCount, 1, fullScreenShowMetadataValue);
        valIdx = fullScreenShowMetadataValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = fullScreenShowThumbs
        tooltip = "When you switch to full screen show the Thumbs dock.";
        QStandardItem *fullScreenShowThumbsCaption = new QStandardItem;
        fullScreenShowThumbsCaption->setToolTip(tooltip);
        fullScreenShowThumbsCaption->setText("Show film strip");
        fullScreenShowThumbsCaption->setEditable(false);
        QStandardItem *fullScreenShowThumbsValue = new QStandardItem;
        fullScreenShowThumbsValue->setToolTip(tooltip);
        fullScreenShowThumbsValue->setData(mw->fullScreenDocks.isThumbs, Qt::EditRole);
        fullScreenShowThumbsValue->setData(DT_Checkbox, UR_DelegateType);
        fullScreenShowThumbsValue->setData("fullScreenShowThumbs", UR_Source);
        fullScreenShowThumbsValue->setData("bool", UR_Type);
        fullScreenCatItem->setChild(secondGenerationCount, 0, fullScreenShowThumbsCaption);
        fullScreenCatItem->setChild(secondGenerationCount, 1, fullScreenShowThumbsValue);
        valIdx = fullScreenShowThumbsValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

        secondGenerationCount++;
        // Type = CHECKBOX
        // name = fullScreenShowStatusBar
        tooltip = "When you switch to full screen show the StatusBar dock.";
        QStandardItem *fullScreenShowStatusBarCaption = new QStandardItem;
        fullScreenShowStatusBarCaption->setToolTip(tooltip);
        fullScreenShowStatusBarCaption->setText("Show status bar");
        fullScreenShowStatusBarCaption->setEditable(false);
        QStandardItem *fullScreenShowStatusBarValue = new QStandardItem;
        fullScreenShowStatusBarValue->setToolTip(tooltip);
        fullScreenShowStatusBarValue->setData(mw->fullScreenDocks.isStatusBar, Qt::EditRole);
        fullScreenShowStatusBarValue->setData(DT_Checkbox, UR_DelegateType);
        fullScreenShowStatusBarValue->setData("fullScreenShowStatusBar", UR_Source);
        fullScreenShowStatusBarValue->setData("bool", UR_Type);
        fullScreenCatItem->setChild(secondGenerationCount, 0, fullScreenShowStatusBarCaption);
        fullScreenCatItem->setChild(secondGenerationCount, 1, fullScreenShowStatusBarValue);
        valIdx = fullScreenShowStatusBarValue->index();
        propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);

    firstGenerationCount++;
    // HEADER
    // Metadata panel category
    QStandardItem *metadataPanelCatItem = new QStandardItem;
    metadataPanelCatItem->setText("Metadata panel items");
    metadataPanelCatItem->setEditable(false);
    metadataPanelCatItem->setData(DT_None, UR_DelegateType);
    model->appendRow(metadataPanelCatItem);
    setColumnWidth(0, captionColumnWidth);
    setColumnWidth(1, valueColumnWidth);
    secondGenerationCount = -1;

        // InfoView fields to show
        QStandardItemModel *okInfo = mw->infoView->ok;
        QStandardItem *okCaption;
        QStandardItem *okValue;
        QStandardItem *okChildCaption;
        QStandardItem *okChildValue;
        // iterate through infoView data, adding it to the property editor
        for(int row = 0; row < okInfo->rowCount(); row++) {
            QModelIndex parentIdx = okInfo->index(row, 0);
            caption = okInfo->index(row, 0).data().toString();
            QModelIndex isShowInfoIdx = okInfo->index(row, 2);
            isShow = isShowInfoIdx.data().toBool();
            tooltip = "Show or hide the category " + caption + " in the metadata panel";
            // Add okInfo category to the property editor
            secondGenerationCount++;
            okCaption = new QStandardItem;
            okCaption->setToolTip(tooltip);
            okCaption->setText("Show " + caption);
            okCaption->setEditable(false);
            okValue = new QStandardItem;
            okValue->setToolTip(tooltip);
            okValue->setData(isShow, Qt::EditRole);
            okValue->setData(isShowInfoIdx, UR_QModelIndex);
            okValue->setData(DT_Checkbox, UR_DelegateType);
            okValue->setData("infoView->ok", UR_Source);
            okValue->setData("bool", UR_Type);
            metadataPanelCatItem->setChild(secondGenerationCount, 0, okCaption);
            metadataPanelCatItem->setChild(secondGenerationCount, 1, okValue);
            valIdx = okValue->index();
            propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);
            thirdGenerationCount = -1;
            for (int childRow = 0; childRow < okInfo->rowCount(parentIdx); childRow++) {
                caption = okInfo->index(childRow, 0, parentIdx).data().toString();
                QModelIndex isShowInfoIdx = okInfo->index(childRow, 2, parentIdx);
                isShow = isShowInfoIdx.data().toBool();
                tooltip = "Show or hide the field " + caption + " in the metadata panel";
                // Add okInfo child to the property editor
                thirdGenerationCount++;
                okChildCaption = new QStandardItem;
                okChildCaption->setToolTip(tooltip);
                okChildCaption->setText("Show " + caption);
                okChildCaption->setEditable(false);
                okChildValue = new QStandardItem;
                okChildValue->setToolTip(tooltip);
                okChildValue->setData(isShow, Qt::EditRole);
                okChildValue->setData(isShowInfoIdx, UR_QModelIndex);
                okChildValue->setData(DT_Checkbox, UR_DelegateType);
                okChildValue->setData("infoView->ok", UR_Source);
                okChildValue->setData("bool", UR_Type);
                okCaption->setChild(thirdGenerationCount, 0, okChildCaption);
                okCaption->setChild(thirdGenerationCount, 1, okChildValue);
                valIdx = okChildValue->index();
                propertyDelegate->createEditor(this, *styleOptionViewItem, valIdx);
            }
        }

    firstGenerationCount++;
    // HEADER
    // Tableview show/hide fields category
    QStandardItem *tableViewCatItem = new QStandardItem;
    tableViewCatItem->setText("TableView columns");
    tableViewCatItem->setEditable(false);
    tableViewCatItem->setData(DT_None, UR_DelegateType);
    model->appendRow(tableViewCatItem);
    setColumnWidth(0, captionColumnWidth);
    setColumnWidth(1, valueColumnWidth);
    secondGenerationCount = -1;

        // TableView conventional fields to show
        QStandardItemModel *tv = mw->tableView->ok;
        int geekStartRow;
        for(int row = 0; row < tv->rowCount(); row++) {
            // do not show if is a geek column
            if (tv->index(row, 2).data().toBool()) continue;
            caption = tv->index(row, 0).data().toString();
            valIdx = tv->index(row, 1);
            isShow = valIdx.data().toBool();
            tooltip = "Show or hide the column " + caption + " in the table view.";
            // Add tableview column to the property editor
            secondGenerationCount++;
            captionItem = new QStandardItem;
            captionItem = new QStandardItem;
            captionItem->setToolTip(tooltip);
            captionItem->setText("Show " + caption);
            captionItem->setEditable(false);
            valueItem = new QStandardItem;
            valueItem->setToolTip(tooltip);
            valueItem->setData(isShow, Qt::EditRole);
            valueItem->setData(valIdx, UR_QModelIndex);
            valueItem->setData(DT_Checkbox, UR_DelegateType);
            valueItem->setData("tableView->ok", UR_Source);
            valueItem->setData("bool", UR_Type);
            tableViewCatItem->setChild(secondGenerationCount, 0, captionItem);
            tableViewCatItem->setChild(secondGenerationCount, 1, valueItem);
            propertyDelegate->createEditor(this, *styleOptionViewItem, valueItem->index());
        }

        // HEADER
        // TableView:: Geek Columns subcategory
        secondGenerationCount++;
        QStandardItem *tableViewGeekCatItem = new QStandardItem;
        tableViewGeekCatItem->setText("Geek columns");
        tableViewGeekCatItem->setEditable(false);
        tableViewGeekCatItem->setData(DT_None, UR_DelegateType);
        tableViewCatItem->setChild(secondGenerationCount, 0, tableViewGeekCatItem);
        thirdGenerationCount = -1;

            // TableView geek fields to show
//            for(int row = geekStartRow; row < tv->rowCount(); row++) {
            for(int row = 0; row < tv->rowCount(); row++) {
                // do not show if is a not a geek column
                if (!tv->index(row, 2).data().toBool()) continue;
                caption = tv->index(row, 0).data().toString();
                valIdx = tv->index(row, 1);
                isShow = valIdx.data().toBool();
                tooltip = "Show or hide the column " + caption + " in the table view.";
                // Add tableview column to the property editor
                thirdGenerationCount++;
                captionItem = new QStandardItem;
                captionItem = new QStandardItem;
                captionItem->setToolTip(tooltip);
                captionItem->setText("Show " + caption);
                captionItem->setEditable(false);
                valueItem = new QStandardItem;
                valueItem->setToolTip(tooltip);
                valueItem->setData(isShow, Qt::EditRole);
                valueItem->setData(valIdx, UR_QModelIndex);
                valueItem->setData(DT_Checkbox, UR_DelegateType);
                valueItem->setData("tableView->ok", UR_Source);
                valueItem->setData("bool", UR_Type);
                tableViewGeekCatItem->setChild(thirdGenerationCount, 0, captionItem);
                tableViewGeekCatItem->setChild(thirdGenerationCount, 1, valueItem);
                propertyDelegate->createEditor(this, *styleOptionViewItem, valueItem->index());

//                bool isGeek = tv->index(row, 2).data().toBool();
//                qDebug() << __FUNCTION__ << caption << "isGeek =" << isGeek;
            }
}











