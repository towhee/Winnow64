#include "preferences.h"
#include "Main/mainwindow.h"
#include "Main/global.h"
#include <QDebug>

// this works because propertyeditor and preferences are friend classes of MW
extern MW *mw;
MW *mw;

Preferences::Preferences(QWidget *parent): PropertyEditor(parent)
{
    mw = qobject_cast<MW*>(parent);

    // necessary as the font size is changed in preferences so the slider action would not
    // work if being resized at the same time due to changes in font size
    ignoreFontSizeChangeSignals = true;

    stringToFitCaptions = "======String to fit in captions column======";
    stringToFitValues   = "=String to fit in values column=";
    resizeColumns();

    addItems();
}

void Preferences::itemChange(QModelIndex idx)
{
/*
When the user changes a value in the editor (the control in the value column of the tree)
this slot is triggered to update the associated value in the application.  The model index
of the value is used to recall:

    v      - the value of the property
    source - the name of the value, which is used to figure out which associated app value
             to update with v.
    index  - is used where the associated value is in another model

The connection is defined in the parent PropertyEditor to fire the virtual function
itemChange, which is subclassed here.
*/
    QVariant v = idx.data(Qt::EditRole);
    QString source = idx.data(UR_Source).toString();
//    QString dataType = idx.data(UR_Type).toString();
    QModelIndex index = idx.data(UR_QModelIndex).toModelIndex();

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

    if (source == "showZoomFrame") {
        mw->thumbView->showZoomFrame = v.toBool();
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
        G::showCacheStatus = v.toBool();
        mw->setCacheParameters();
    }

    if (source == "metadataChunkSize") {
        mw->metadataCacheThread->metadataChunkSize = v.toInt();
    }

    if (source == "maxIconSize") {
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

    if (source == "imageCacheSizeMethod") {
        QString s = v.toString();
        int mb = 0;
        if (s == "Thrifty") mb = static_cast<int>(G::availableMemoryMB * 0.10);
        if (s == "Moderate") mb = static_cast<int>(G::availableMemoryMB * 0.50);
        if (s == "Greedy") mb = static_cast<int>(G::availableMemoryMB * 0.90);
        if (mb > 0 && mb < 1000) mb = 1000;
        /*
        if (s == "Percent of available") {
            static_cast<SpinBoxEditor*>(cacheSizePercentOfAvailable)->setEnabled(true);
            int pct = static_cast<SpinBoxEditor*>(cacheSizePercentOfAvailable)->value();
            mb = static_cast<int>((G::availableMemoryMB * static_cast<uint>(pct)) / 100);
        }
        else {
            static_cast<SpinBoxEditor*>(cacheSizePercentOfAvailable)->setEnabled(false);
        }
        if (s == "MB") {
            static_cast<SpinBoxEditor*>(cacheSizeMB)->setEnabled(true);
            mb = static_cast<SpinBoxEditor*>(cacheSizeMB)->value();
        }
        else {
            static_cast<SpinBoxEditor*>(cacheSizeMB)->setEnabled(false);
            static_cast<SpinBoxEditor*>(cacheSizeMB)->setValue(mb);
        }
        */
        mw->cacheSizeMethod = s;
        mw->cacheSizeMB = mb;
        mw->setCacheParameters();
        QString availMBMsg = QString::number(mw->cacheSizeMB) + " / " + QString::number(G::availableMemoryMB) + " MB";
        static_cast<LabelEditor*>(availMBMsgWidget)->setValue(availMBMsg);
    }

    if (source == "cacheSizeMB") {
        qDebug() << __FUNCTION__ << v << source;
        mw->cacheSizeMB = v.toInt();
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
        mw->updateProgressBarWidth();
        mw->progressWidthBeforeResizeWindow = mw->progressWidth;
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

    if (source == "autoAdvance") {
        mw->autoAdvance = v.toBool();
    }

    if (source == "turnOffEmbellish") {
        mw->turnOffEmbellish = v.toBool();
    }

    if (source == "deleteWarning") {
        mw->deleteWarning = v.toBool();
    }

    if (source == "isLogger") {
        G::isLogger = v.toBool();
        if (G::isLogger) mw->openLog();
    }

//    if (source == "colorManage") {
//        G::colorManage = v.toBool();
//    }

    if (source == "rememberLastDir") {
        mw->rememberLastDir = v.toBool();
    }

    if (source == "checkIfUpdate") {
        mw->checkIfUpdate = v.toBool();
    }

    if (source == "limitFit100Pct") {
        mw->imageView->limitFit100Pct = v.toBool();
        mw->imageView->refresh();
    }

    if (source == "globalFontSize") {
        mw->setFontSize(v.toInt());
        setStyleSheet(mw->css);
//        resizeColumns();
    }

    if (source == "globalBackgroundShade") {
        mw->setBackgroundShade(v.toInt());
        setStyleSheet(mw->css);
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

    if (source == "fullScreenShowFolders") {
        mw->fullScreenDocks.isFolders = v.toBool();
    }

    if (source == "fullScreenShowBookmarks") {
        mw->fullScreenDocks.isFavs = v.toBool();
    }

    if (source == "fullScreenShowFilters") {
        mw->fullScreenDocks.isFilters = v.toBool();
    }

    if (source == "fullScreenShowMetadata") {
        mw->fullScreenDocks.isMetadata = v.toBool();
    }

    if (source == "fullScreenShowThumbs") {
        mw->fullScreenDocks.isThumbs = v.toBool();
    }

    if (source == "fullScreenShowStatusBar") {
        mw->fullScreenDocks.isStatusBar = v.toBool();
    }
}

void Preferences::addItems()
{
    ItemInfo i;
    /* template of ItemInfo
    i.name = "";
    i.parentName = "";
    i.captionText = "";
    i.tooltip = "";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.valueName = "";
    i.delegateType = DT_None;
    i.min = 0;
    i.max = 0;
    i.fixedWidth = 50;
    i.color = "#AABBCC";
    i.index = QModelIndex();
    i.dropList = {"1", "2"};
    */
    clearItemInfo(i);

    // General header (Root)
    i.name = "GeneralHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "General";
    i.tooltip = "General items";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    {
//    // Manage color
//    i.name = "colorManage";
//    i.parentName = "GeneralHeader";
//    i.captionText = "Color manage";
//    i.tooltip = "Turning on color management will ensure consistent color, especially for\n"
//                "files that have been converted to a color profile other than sRGB.  Color \n"
//                "management has a small impact on image caching performance.";
//    i.hasValue = true;
//    i.captionIsEditable = false;
//    i.value = G::colorManage;
//    i.key = "colorManage";
//    i.delegateType = DT_Checkbox;
//    i.type = "bool";
//    addItem(i);

    // Remember last folder
    i.name = "rememberLastDir";
    i.parentName = "GeneralHeader";
    i.captionText = "Remember folder";
    i.tooltip = "Remember the last folder used in Winnow from the previous session.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->rememberLastDir;
    i.key = "rememberLastDir";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Check for app update at startup
    i.name = "checkIfUpdate";
    i.parentName = "GeneralHeader";
    i.captionText = "Check for program update";
    i.tooltip = "At startup check if there is an update to Winnow.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->checkIfUpdate;
    i.key = "checkIfUpdate";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Limit loupe fit zoom to 100%.
    i.name = "limitFit100Pct";
    i.parentName = "GeneralHeader";
    i.captionText = "Limit loupe fit zoom to 100%.";
    i.tooltip = "Limit loupe fit zoom to 100%.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->imageView->limitFit100Pct;
    i.key = "limitFit100Pct";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Auto advance
    i.name = "autoAdvance";
    i.parentName = "GeneralHeader";
    i.captionText = "Auto advance";
    i.tooltip = "Advance to next image after pick.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->autoAdvance;
    i.key = "autoAdvance";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Do not Embellish when folder change
    i.name = "turnOffEmbellish";
    i.parentName = "GeneralHeader";
    i.captionText = "Turn off embellish when change folder";
    i.tooltip = "Turn off embellishwhen change folder.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->turnOffEmbellish;
    i.key = "turnOffEmbellish";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Delete warning
    i.name = "deleteWarning";
    i.parentName = "GeneralHeader";
    i.captionText = "Warn before delete";
    i.tooltip = "Turn this off to prevent a warning dialog every time"
                "you delete a file or group of files.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->deleteWarning;
    i.key = "deleteWarning";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Logger
    i.name = "isLogger";
    i.parentName = "GeneralHeader";
    i.captionText = "Turn log on";
    i.tooltip = "Turn this on to write errors to a log file."
                "Warning: this will impact performance.  Use"
                "to help resolve bugs and crashes.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::isLogger;
    i.key = "isLogger";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Thumbnails Header (Root) ---------------------------------------------------------------
    i.name = "AppearanceHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Appearance";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    // Application background luminousity
    i.name = "globalBackgroundShade";
    i.parentName = "AppearanceHeader";
    i.captionText = "Application background luminousity";
    i.tooltip = "Change the background shade throughout the application.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 50;
    i.value = G::backgroundShade;
    i.key = "globalBackgroundShade";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 10;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // Font size header
    i.name = "FontSizeHeader";
    i.parentName = "AppearanceHeader";
    i.captionText = "Font size";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    // Application font size
    i.name = "globalFontSize";
    i.parentName = "FontSizeHeader";
    i.tooltip = "Change the font size throughout the application.";
    i.captionText = "Global";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 10;
    i.value = G::fontSize;
    i.key = "globalFontSize";
    i.delegateType = DT_Spinbox;
//    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 6;
    i.max = 20;
    i.fixedWidth = 50;
    addItem(i);

    // Application font size
    i.name = "infoOverlayFontSize";
    i.parentName = "FontSizeHeader";
    i.tooltip = "Change the font size for the info overlay (usually showing the shooting"
                "info in the top left of the image).";
    i.captionText = "Info overlay";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 16;
    i.value = mw->imageView->infoOverlayFontSize;
    i.key = "infoOverlayFontSize";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 6;
    i.max = 30;
    i.fixedWidth = 50;
    addItem(i);

    // General category::Badge size subcategory
    i.name = "BadgeSizeHeader";
    i.parentName = "AppearanceHeader";
    i.captionText = "Classification badge size";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Classification badge size
    i.name = "classificationBadgeInImageDiameter";
    i.parentName = "BadgeSizeHeader";
    i.tooltip = "The image badge is a circle showing the colour classification, rating and pick\n"
                "status.  It is located in the lower right corner of the image.  This property \n"
                "allows you to adjust its size.";
    i.captionText = "Loupe";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 36;
    i.value =mw->classificationBadgeInImageDiameter;
    i.key = "classificationBadgeInImageDiameter";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 10;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // Thumbnail badge size
    i.name = "thumbnailBadgeInImageDiameter";
    i.parentName = "BadgeSizeHeader";
    i.tooltip = "The image badge is a circle showing the colour classification, rating and pick\n"
                "status.  It is located in the top right corner of the thumbnail in the thumb \n"
                "and grid views.  This property allows you to adjust its size.";
    i.captionText = "Thumbnail";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 19;
    i.value =mw->classificationBadgeInThumbDiameter;
    i.key = "classificationBadgeInThumbDiameter";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 10;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // Filmstrip Header
    i.name = "FilmstripHeader";
    i.parentName = "AppearanceHeader";
    i.captionText = "Film strip";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Thumbnail badge size
    i.name = "thumbViewIconSize";
    i.parentName = "FilmstripHeader";
    i.tooltip = "Change the display size of the thumbnails in the film strip.";
    i.captionText = "Size";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0; // n/a
    i.key = "thumbViewIconSize";
    i.delegateType = DT_PlusMinus;
    i.type = "int";
    addItem(i);

    // Thumbnail label font size
    i.name = "thumbViewLabelSize";
    i.parentName = "FilmstripHeader";
    i.captionText = "Label size";
    i.tooltip = "Change the display size of the file name shown at the bottom of each thumbnail.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 8;
    i.value = mw->thumbView->labelFontSize;
    i.key = "thumbViewLabelSize";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 6;
    i.max = 16;
    i.fixedWidth = 50;
    addItem(i);

    // Show thumbnail label
    i.name = "thumbViewShowLabel";
    i.parentName = "FilmstripHeader";
    i.captionText = "Show Label";
    i.tooltip = "Show or hide the label with the file name at the bottom of each thumbnail.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 8;
    i.value = mw->thumbView->showIconLabels;
    i.key = "thumbViewShowLabel";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Grid Header
    i.name = "GridHeader";
    i.parentName = "AppearanceHeader";
    i.captionText = "Grid";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Grid size
    i.name = "gridIconSize";
    i.parentName = "GridHeader";
    i.tooltip = "Change the display size of the thumbnails in the grid view.";
    i.captionText = "Size";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = 0; // n/a
    i.key = "gridViewIconSize";
    i.delegateType = DT_PlusMinus;
    i.type = "int";
    addItem(i);

    // Thumbnail label font size
    i.name = "gridViewLabelSize";
    i.parentName = "GridHeader";
    i.captionText = "Label size";
    i.tooltip = "Change the display size of the file name shown at the bottom of each thumbnail.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->gridView->labelFontSize;
    i.key = "gridViewLabelSize";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 6;
    i.max = 16;
    i.fixedWidth = 50;
    addItem(i);

    // Show grid thumbnail label
    i.name = "gridViewShowLabel";
    i.parentName = "GridHeader";
    i.captionText = "Show Label";
    i.tooltip = "Show or hide the label with the file name at the bottom of each thumbnail.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->gridView->showIconLabels;
    i.key = "gridViewShowLabel";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
    }

    // Cache Header (Root)
    i.name = "CacheHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Cache";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    {
    // Show thumbnail label
    i.name = "showThreadActivity";
    i.parentName = "CacheHeader";
    i.captionText = "Show caching activity";
    i.tooltip = "Two small indicators on the extreme right side of the status bar turn red\n"
                "when there is caching activity.  The left indicator is for matadata caching\n"
                "activity, while the right indicator shows image caching activity.  This\n"
                "preference shows or hides the indicators.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->isShowCacheThreadActivity;
    i.key = "showThreadActivity";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Set the width of the cache status progress bar
    i.name = "progressWidthSlider";
    i.parentName = "CacheHeader";
    i.captionText = "Cache status bar width";
    i.tooltip = "Change the width of the cache status in the status bar.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 120;
    i.value = mw->progressWidth;
    i.key = "progressWidthSlider";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 100;
    i.max = 1000;
    i.fixedWidth = 50;
    addItem(i);

    // Cache Thumbnail Header
    i.name = "CacheThumbnailHeader";
    i.parentName = "CacheHeader";
    i.captionText = "Thumbnails";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Metadata chunk size (number of thumbnails)
    i.name = "metadataChunkSize";
    i.parentName = "CacheThumbnailHeader";
    i.captionText = "Incremental amount to load";
    i.tooltip = "Enter the number of minimum thumbnails and metadata you want to cache.\n"
                "If the grid is displaying a larger number then the larger number will \n"
                "be used to make sure they are all shown.  You can experiment to see \n"
                "what works best.  250 is the default amount.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->metadataCacheThread->metadataChunkSize;
    i.key = "metadataChunkSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 3000;
    i.fixedWidth = 50;
    addItem(i);

    // Maximum icon size
    i.name = "maxIconSize";
    i.parentName = "CacheThumbnailHeader";
    i.captionText = "Maximum Icon Size";
    i.tooltip = "Enter the maximum size in pixel width for thumbnails.  Icons will be \n"
                "created at this size.  \n\n"
                "WARNING: The memory requirements increase at the square of the size, \n"
                "and folders can contain thousands of images.  Larger thumbnail sizes \n"
                "can consume huge amounts of memory.  The default size is 256 pixels \n"
                "to a side.\n";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::maxIconSize;
    i.key = "maxIconSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 40;
    i.max = 640;
    i.fixedWidth = 50;
    addItem(i);

    // Cache Full Image Header
    i.name = "CacheImagesHeader";
    i.parentName = "CacheHeader";
    i.captionText = "Full size images";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Available memory for caching
    i.name = "availableMBToCache";
    i.parentName = "CacheImagesHeader";
    i.captionText = "Image cache / Available memory";
    i.tooltip = "The total amount of available memory in MB.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = QString::number(mw->cacheSizeMB) + " / " + QString::number(G::availableMemoryMB) + " MB";
    i.key = "availableMBToCache";
    i.delegateType = DT_Label;
    i.type = "QString";
    i.color = "#1b8a83";
    availMBMsgWidget = addItem(i);

    // Image cache size strategy
    i.name = "imageCacheSizeMethod";
    i.parentName = "CacheImagesHeader";
    i.captionText = "Choose cache size method";
    i.tooltip = "Select method of determining the size of the image cache\n"
                "Thrifty  = larger of 10% of available memory\n"
                "Moderate = 50% of available memory\n"
                "Greedy   = 90% of available memory";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->cacheSizeMethod;
    i.key = "imageCacheSizeMethod";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList << "Thrifty" << "Moderate" << "Greedy";
    addItem(i);

    // Slideshow Header (Root)
    i.name = "SlideshowHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Slideshow";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    // Slideshow delay
    i.name = "slideshowDelay";
    i.parentName = "SlideshowHeader";
    i.captionText = "Slideshow delay (sec)";
    i.tooltip = "Enter the slideshow delay in seconds.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->slideShowDelay;
    i.key = "slideshowDelay";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 300;
    i.fixedWidth = 50;
    addItem(i);

    // Random slideshow toggle
    i.name = "isSlideShowRandom";
    i.parentName = "SlideshowHeader";
    i.captionText = "Random slide selection";
    i.tooltip = "Selects random slides if checked.  Otherwise the slide selection is \n"
                "sequential, based on the sort order.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->isSlideShowRandom;
    i.key = "isSlideShowRandom";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Wrap slideshow
    i.name = "isSlideShowWrap";
    i.parentName = "SlideshowHeader";
    i.captionText = "Wrap slide selection";
    i.tooltip = "Wrap mode goes back to the beginning when the last image has been shown.n";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->isSlideShowWrap;
    i.key = "isSlideShowWrap";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
    }

    // Full Screen Header (Root)
    i.name = "FullScreenHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Full screen defaults";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);//return;

    {
    // Full screen - show folders
    i.name = "fullScreenShowFolders";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show folders";
    i.tooltip = "When you switch to full screen show the folders dock.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isFolders;
    i.key = "fullScreenShowFolders";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show bookmarks
    i.name = "fullScreenShowBookmarks";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show bookmarks";
    i.tooltip = "When you switch to full screen show the bookmarks dock.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isFavs;
    i.key = "fullScreenShowBookmarks";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show filters
    i.name = "fullScreenShowFilters";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show filters";
    i.tooltip = "When you switch to full screen show the filters dock.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isFilters;
    i.key = "fullScreenShowFilters";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show metadata
    i.name = "fullScreenShowMetadata";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show metadata";
    i.tooltip = "When you switch to full screen show the metadata dock.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isMetadata;
    i.key = "fullScreenShowMetadata";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show filmstrip
    i.name = "fullScreenShowThumbs";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show filmstrip";
    i.tooltip = "When you switch to full screen show the filmstrip.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isThumbs;
    i.key = "fullScreenShowThumbs";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show status bar
    i.name = "fullScreenShowStatusBar";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show status bar";
    i.tooltip = "When you switch to full screen show the status bar.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isStatusBar;
    i.key = "fullScreenShowStatusBar";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
    }

    // Metadata InfoView Header (Root)
    i.name = "MetadataPanelHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Metadata panel items";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    { // Metadata InfoView items

    // InfoView fields to show
    QStandardItemModel *okInfo = mw->infoView->ok;
    // iterate through infoView data, adding it to the property editor
    for(int row = 0; row < okInfo->rowCount(); row++) {
        QModelIndex parentIdx = okInfo->index(row, 0);
        QString caption = okInfo->index(row, 0).data().toString();
        i.parentName = "MetadataPanelHeader";
        i.name = caption;
        i.captionText = "Show " + caption;
        i.tooltip = "Show or hide the category " + caption + " in the metadata panel";
        i.hasValue = true;
        i.delegateType = DT_Checkbox;
        i.type = "bool";
        i.captionIsEditable = false;
        i.key = "infoView->ok";
        i.value = okInfo->index(row, 2).data().toBool();
        i.index = okInfo->index(row, 2);
        addItem(i);
        for (int childRow = 0; childRow < okInfo->rowCount(parentIdx); childRow++) {
            QString childCaption = okInfo->index(childRow, 0, parentIdx).data().toString();
            i.parentName = caption;
            i.captionText = "Show " + childCaption;
            i.tooltip = "Show or hide the category " + childCaption + " in the metadata panel";
            i.hasValue = true;
            i.delegateType = DT_Checkbox;
            i.type = "bool";
            i.captionIsEditable = false;
            i.key = "infoView->ok";
            i.value = okInfo->index(childRow, 2, parentIdx).data().toBool();
            i.index = okInfo->index(childRow, 2, parentIdx);
            addItem(i);
        }
    }
    // end Metadata InfoView items
    }

    // TableView show/hide fields Header (Root)
    i.name = "TableViewColumnsHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "TableView columns";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    { // TableView show/hide field items

    // TableView conventional fields to show
    QStandardItemModel *tv = mw->tableView->ok;
    for (int row = 0; row < tv->rowCount(); row++) {
        // do not show if is a geek column
        if (tv->index(row, 2).data().toBool()) continue;
        QString caption = tv->index(row, 0).data().toString();
        i.name = caption;
        i.captionText = "Show " + caption;
        i.tooltip = "Show or hide the category " + caption + " in the table view";
        i.hasValue = true;
        i.delegateType = DT_Checkbox;
        i.type = "bool";
        i.captionIsEditable = false;
        i.key = "tableView->ok";
        i.parentName = "TableViewColumnsHeader";
        i.value = tv->index(row, 1).data().toBool();
        i.index = tv->index(row, 1);
        addItem(i);
    }

    // TableView geek columns header
    i.name = "TableViewGeekColumnsHeader";
    i.parentName = "TableViewColumnsHeader";
    i.captionText = "Geek columns";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // TableView geek fields to show
    for(int row = 0; row < tv->rowCount(); row++) {
        // do not show if is a not a geek column
        if (!tv->index(row, 2).data().toBool()) continue;
        QString caption = tv->index(row, 0).data().toString();
        i.parentName = "TableViewGeekColumnsHeader";
        i.captionText = "Show " + caption;
        i.tooltip = "Show or hide the category " + caption + " in the table view";
        i.hasValue = true;
        i.delegateType = DT_Checkbox;
        i.type = "bool";
        i.captionIsEditable = false;
        i.key = "tableView->ok";
        i.value = tv->index(row, 1).data().toBool();
        i.index = tv->index(row, 1);
        addItem(i);
    }
    // end TableView show/hide field items
    }

    return;
}
