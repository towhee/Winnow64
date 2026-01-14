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

    stringToFitCaptions = "======String to fit in captions column needs to be wide======";
    stringToFitValues   = "=String to fit in values column=";
    resizeColumns();

    addItems();
}

void Preferences::rory()
{
    QModelIndex capIdx;

    capIdx = findCaptionIndex("showCacheProgressBar");
    if (G::isRory) setRowHidden(capIdx.row(), capIdx.parent(), false);
    else setRowHidden(capIdx.row(), capIdx.parent(), true);

    capIdx = findCaptionIndex("progressWidthSlider");
    if (G::isRory) setRowHidden(capIdx.row(), capIdx.parent(), false);
    else setRowHidden(capIdx.row(), capIdx.parent(), true);

    // capIdx = findCaptionIndex("iconChunkSize");
    // if (G::isRory) setRowHidden(capIdx.row(), capIdx.parent(), false);
    // else setRowHidden(capIdx.row(), capIdx.parent(), true);

    capIdx = findCaptionIndex("cacheMethod");
    if (G::isRory) setRowHidden(capIdx.row(), capIdx.parent(), false);
    else setRowHidden(capIdx.row(), capIdx.parent(), true);
}

void Preferences::itemChange(QModelIndex idx)
{
/*
    When the user changes a value in the editor (the control in the value column of the
    tree) this slot is triggered to update the associated value in the application. The
    model index of the value is used to recall:

        v      - the value of the property
        source - the name of the value, which is used to figure out which associated
                 app value to update with v.
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

    if (source == "labelChoice") {
        mw->gridView->labelChoice = v.toString();
        mw->gridView->iconViewDelegate->setThumbDimensions(
                    mw->gridView->iconWidth,
                    mw->gridView->iconHeight,
                    mw->gridView->labelFontSize,
                    mw->gridView->showIconLabels,
                    mw->gridView->labelChoice,
                    mw->gridView->badgeSize,
                    mw->gridView->iconNumberSize);
        mw->gridView->repaint();
        mw->thumbView->labelChoice = v.toString();
        mw->thumbView->iconViewDelegate->setThumbDimensions(
                    mw->thumbView->iconWidth,
                    mw->thumbView->iconHeight,
                    mw->thumbView->labelFontSize,
                    mw->thumbView->showIconLabels,
                    mw->thumbView->labelChoice,
                    mw->thumbView->badgeSize,
                    mw->thumbView->iconNumberSize);
        mw->thumbView->repaint();
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
        mw->thumbView->thumbsFitTopOrBottom("thumbViewShowLabel");
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

    if (source == "showThumbNailSymbolHelp") {
        bool value = v.toBool();
        mw->dm->setShowThumbNailSymbolHelp(value);
        mw->settings->setValue("showThumbNailSymbolHelp", value);

    }

    if (source == "playPickAudio") {
        bool value = v.toBool();
        mw->settings->setValue("playPickAudio", value);
    }

    if (source == "pickAudioVolume") {
        int value = v.toInt();
        mw->pickClick->setVolume(value * 1.0 / 100);
        mw->settings->setValue("pickAudioVolume", value);
        qDebug() << "Preferences::itemChange pickClick->volume() =" << mw->pickClick->volume();
    }

    if (source == "loadOnlyVisibleIcons") {
        G::loadOnlyVisibleIcons = v.toBool();
    }

    if (source == "showCacheProgressBar") {
        // signal ImageCache
        bool isShow = v.toBool();
        emit mw->setShowCacheStatus(isShow);
        mw->isShowCacheProgressBar = isShow;
        mw->metaRead->showProgressInStatusbar = isShow;
        mw->progressLabel->setVisible(isShow);
        // hide/show progressWidthSlider in preferences
        QModelIndex capIdx = findCaptionIndex("progressWidthSlider");
        if (v.toBool()) setRowHidden(capIdx.row(), capIdx.parent(), false);
        else setRowHidden(capIdx.row(), capIdx.parent(), true);
    }

    if (source == "progressWidthSlider") {
        mw->cacheBarProgressWidth = v.toInt();
        mw->updateProgressBarWidth();
    }

    if (source == "maxIconSize") {
        G::maxIconSize = v.toInt();
    }

    if (source == "iconChunkSize") {
        mw->updateDefaultIconChunkSize(v.toInt());
        mw->settings->setValue("iconChunkSize", v.toInt());
    }

    if (source == "imageCacheSize") {
        QString size = v.toString();

        ImageCache::AutoStrategy as = ImageCache::AutoStrategy::Ignore;

        // auto mode?
        bool isAuto = size.startsWith("Auto");
        mw->settings->setValue("autoMaxMB", isAuto);

        if (isAuto) {
            if (size == "Auto Frugal") {
                as = ImageCache::AutoStrategy::Frugal;
                setItemValue("iconChunkSize", 100);
                mw->updateDefaultIconChunkSize(100);
                mw->settings->setValue("iconChunkSize", 100);
            }
            else if (size == "Auto Moderate") {
                as = ImageCache::AutoStrategy::Moderate;
                setItemValue("iconChunkSize", 25000);
                mw->updateDefaultIconChunkSize(25000);
                mw->settings->setValue("iconChunkSize", 25000);
            }
            else if (size == "Auto Greedy") {
                as = ImageCache::AutoStrategy::Greedy;
                setItemValue("iconChunkSize", 25000);
                mw->updateDefaultIconChunkSize(25000);
                mw->settings->setValue("iconChunkSize", 25000);
            }
            mw->imageCache->setAutoMaxMB(true, as);
            // emit mw->setAutoMaxMB(true, as);
            QString strategy = mw->imageCache->autoStrategyStr.at(as);
            mw->settings->setValue("autoMaxMBStrategy", strategy);
        }

        // manual set maxMB size
        else {
            quint64 mb = 4000;
            if (size == "0.5 GB") mb = 500;
            else if (size == "1 GB") mb = 1000;
            else if (size == "2 GB") mb = 2000;
            else if (size == "4 GB") mb = 4000;
            else if (size == "6 GB") mb = 6000;
            else if (size == "8 GB") mb = 8000;
            else if (size == "12 GB") mb = 12000;
            else if (size == "16 GB") mb = 16000;
            else if (size == "24 GB") mb = 24000;
            else if (size == "32 GB") mb = 32000;
            else if (size == "48 GB") mb = 48000;
            else if (size == "64 GB") mb = 64000;
            mw->imageCache->setAutoMaxMB(false, as);
            qDebug() << "mw->imageCache->setMaxMB(mb)";
            // mw->imageCache->setMaxMB(mb);
            // emit mw->setAutoMaxMB(false, as);
            emit mw->setMaxMB(mb);
            mw->settings->setValue("cacheMaxMB", mb);
        }
        mw->refreshAfterImageCacheSizeChange();
        // get available memory
        #ifdef Q_OS_WIN
        Win::availableMemory();     // sets G::availableMemoryMB
        #endif
        #ifdef Q_OS_MAC
        Mac::availableMemory();     // sets G::availableMemoryMB
        #endif

        quint64 memAvail = mw->icd->sizeMB() + G::availableMemoryMB;
        quint64 maxMB = mw->imageCache->getMaxMB();
        QString availMBMsg = QString::number(maxMB) + " of " +
                QString::number(memAvail) + " available MB";
        // static_cast<LabelEditor*>(availMBMsgWidget)->setValue(availMBMsg);
    }

    if (source == "slideShowDelay") {
        mw->slideShowDelay = v.toInt();
    }

    if (source == "isSlideShowRandom") {
        mw->isSlideShowRandom = v.toBool();
        mw->slideShowResetSequence();
    }

    if (source == "isSlideShowWrap") {
        mw->isSlideShowWrap = v.toBool();
    }

    if (source == "classificationBadgeSizeFactor") {
        int value = v.toInt();
        mw->setClassificationBadgeSizeFactor(value);
    }

    if (source == "iconNumberSize") {
        int value = v.toInt();
        mw->setIconNumberSize(value);
    }

    if (source == "autoAdvance") {
        G::autoAdvance = v.toBool();
    }

    if (source == "turnOffEmbellish") {
        mw->turnOffEmbellish = v.toBool();
    }

    if (source == "deleteWarning") {
        mw->deleteWarning = v.toBool();
    }

    if (source == "modifySourceFiles") {
        G::modifySourceFiles = v.toBool();
        // mw->embedThumbnailsAction->setEnabled(G::modifySourceFiles);
        // toggle status bar modify images button
        mw->toggleModifyImages();
        setItemEnabled("backupBeforeModify", G::modifySourceFiles);
        setItemEnabled("autoAddMissingThumbnails", G::modifySourceFiles);
        setItemEnabled("ignoreAddThumbnailsDlg", G::modifySourceFiles);
    }

    if (source == "backupBeforeModify") {
        G::backupBeforeModifying = v.toBool();
    }

    if (source == "autoAddMissingThumbnails") {
        G::autoAddMissingThumbnails = v.toBool();
        setItemEnabled("ignoreAddThumbnailsDlg", !v.toBool());
        setItemValue("ignoreAddThumbnailsDlg", !v.toBool());
    }

    if (source == "ignoreAddThumbnailsDlg") {
        mw->ignoreAddThumbnailsDlg = !v.toBool();

    }

    if (source == "useSidecar") {
        G::useSidecar = v.toBool();
    }    

    if (source == "activityLog") {
        G::isLogger = v.toBool();
        G::isFileLogger = v.toBool();
        mw->settings->setValue("isFileLogger", v.toBool());
    }

    if (source == "errorLog") {
        G::isLogger = v.toBool();
        G::isErrorLogger = v.toBool();
        G::isIssueLogger = v.toBool();
        mw->settings->setValue("isErrorLogger", v.toBool());
    }

    if (source == "renderVideoThumb") {
        G::renderVideoThumb = v.toBool();
    }

    if (source == "isLogger") {
        G::isLogger = v.toBool();
        // if (G::isLogger) mw->startLog();
    }

    if (source == "isErrorLogger") {
        G::isErrorLogger = v.toBool();
        G::showIssueInConsole = v.toBool();
        // if (G::isErrorLogger) mw->startLog();
    }

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

    if (source == "wheelSensitivity") {
        G::wheelSensitivity = v.toInt();
    }

    if (source == "globalFontSize") {
        mw->setFontSize(v.toInt());
        G::fontSize = v.toInt();
        setStyleSheet(mw->css);
    }

    if (source == "globalBackgroundShade") {
        mw->setBackgroundShade(v.toInt());
        setStyleSheet(mw->css);
        resizeColumns();
        #ifdef Q_OS_WIN
            Win::setTitleBarColor(mw->preferencesDlg->winId(), G::backgroundColor);
        #endif
    }

    if (source == "infoOverlayFontSize") {
        mw->imageView->infoOverlayFontSize = v.toInt();
        mw->setInfoFontSize();
    }

    if (source == "tableView->ok") {
        mw->tableView->ok->setData(index, v.toBool());
    }

    if (source == "infoView->ok") {
        if (G::useInfoView)
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

    if (source == "focusStackMethod") {
        mw->fsMethod = v.toString();
        mw->writeSetting("focusStackMethod", v.toString());  // also saved in MW::writeSettings
    }

}

void Preferences::addItems()
{
    // ItemInfo i;
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
    addGeneral();
    addModify();
    addUserInterface();
    addCache();
    addSlideShow();
    addFullScreen();
    addMetadataPanel();
    addTableView();
    addUtilities();
}

void Preferences::addGeneral()
{
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
    // addItem(i);

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
    i.tooltip = "Limit loupe fit zoom to 100%.  This will prevent the image from being\n"
                "zoomed beyond 100% to fit in the loupe view."
                ;
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
    i.captionText = "Auto advance after pick, rating or label.";
    i.tooltip = "Advance to next image after pick, rating or label change.\n"
                "CAPS lock toggles state."
                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::autoAdvance;
    i.key = "autoAdvance";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Do not Embellish when folder change
    i.name = "turnOffEmbellish";
    i.parentName = "GeneralHeader";
    i.captionText = "Turn off embellish when change folder";
    i.tooltip = "Turn off embellish when change folder. This helps\n"
                "to avoid confusion.  ;^)";
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
    i.captionText = "Warn before delete image files";
    i.tooltip = "Turn this off to prevent a warning dialog every time\n"
                "you delete a file or group of files.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->deleteWarning;
    i.key = "deleteWarning";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Render video thumbnails
    i.name = "renderVideoThumb";
    i.parentName = "GeneralHeader";
    i.captionText = "Render video thumbnails (slow).";
    i.tooltip = "If true, the first frame of the video with be shown as the\n"
                "thumbnail.  If false, then ""Video"" will be shown at the\n"
                "bottom of the video.  Rendering the first frame cam be quite\n"
                "slow, so if performance is important then do not render."
                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::renderVideoThumb;
    i.key = "renderVideoThumb";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Logger
    i.name = "isLogger";
    i.parentName = "GeneralHeader";
    i.captionText = "Turn log on";
    i.tooltip = "Turn this on to write errors to a log file.\n"
                "Warning: this will impact performance.  Use\n"
                "to help resolve bugs and crashes.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::isLogger;
    i.key = "isLogger";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
//    addItem(i);

    // Error Logger
    i.name = "isErrorLogger";
    i.parentName = "GeneralHeader";
    i.captionText = "Turn error logging on";
    i.tooltip = "Turn this on to write errors to a log file.\n"
                "Warning: this will impact performance.  Use\n"
                "to help resolve bugs and crashes.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::isErrorLogger;
    i.key = "isErrorLogger";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
}

void Preferences::addModify()
{
    // Modify Files header  > File modification header
    i.name = "ModifyHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Modify";
    i.tooltip = "Modify image files";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    // Allow source files to be changed
    i.name = "modifySourceFiles";
    i.parentName = "ModifyHeader";
    i.captionText = "Permit image file modification";
    i.tooltip = "Permit file modification to change image orientation\n"
                "or embed thumbnails automatically without notification."
                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::modifySourceFiles;
    i.key = "modifySourceFiles";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);       // cancel this for version 1.32 release

    // Backup before modifying
    i.name = "backupBeforeModify";
    i.parentName = "modifySourceFiles";
    i.captionText = "   Backup before modify files";
    i.tooltip = "All image files about to be modified will be copied to a\n"
                "subfolder called 'backup'"
                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::backupBeforeModifying;
    i.key = "backupBeforeModify";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
    setItemEnabled("backupBeforeModify", G::modifySourceFiles);

    // Automatically and silently add missing thumbnails to TIFF and JPG files
    i.name = "autoAddMissingThumbnails";
    i.parentName = "modifySourceFiles";
    i.captionText = "   Silently embed missing thumbnails";
    i.tooltip = "This silently embeds thumbnail if missing from TIFF and\n"
                "JPG image files.\n\n"
                "Enabling this will dramatically improve future thumbnail\n"
                "loading times.  Permit image file modification must also\n"
                "be enabled.\n\n"
                "WARNING: this will modify your TIFF/JPG files.  Please make\n"
                "sure you have backups until you are sure this does not corrupt\n"
                "your images.  FYI, no corruption has been reported by users."
                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::autoAddMissingThumbnails;
    i.key = "autoAddMissingThumbnails";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    if (G::useMissingThumbs) addItem(i);
    // setItemEnabled("autoAddMissingThumbnails", G::modifySourceFiles);

    // Show missing thumbnails dialog
    i.name = "ignoreAddThumbnailsDlg";
    i.parentName = "modifySourceFiles";
    i.captionText = "   Show missing thumbnails dialog";
    i.tooltip = "If enabled, the missing thumbnails dialog will be shown\n"
                "if there are any missing thumbnails for TIFF or JPG files\n"
                "in the folder and Modify source files is enabled."
            ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = !mw->ignoreAddThumbnailsDlg;
    i.key = "ignoreAddThumbnailsDlg";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    if (G::useMissingThumbs) addItem(i);
    setItemEnabled("ignoreAddThumbnailsDlg", G::modifySourceFiles);

    // Write metadata edits to sidecar XMP file
    i.name = "useSidecar";
    i.parentName = "ModifyHeader";
    i.captionText = "Use xmp sidecars";
    i.tooltip = "This will NOT MODIFY your source image file.\n\n"
                "If you edit metadata (rating, color class, title, creator,\n"
                "copyright, email, url) the change will be written to\n"
                "a XMP sidecar file.  This data can be read by Winnow and\n"
                "other programs like Lightroom.\n\n"
                "Note this could slightly impact performance, as it might\n"
                "take longer to initially read all the metadata when a\n"
                "folder is being loaded."
                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::useSidecar;
    i.key = "useSidecar";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
}

    /*
    // Loggers: on/off
    i.name = "loggers";
    i.parentName = "GeneralHeader";
    i.captionText = "Loggers";
    i.tooltip = "Record activity or errors to a log file."                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::modifySourceFiles;
    i.key = "loggers";
    addItem(i);

    // Activity log
    i.name = "activityLog";
    i.parentName = "loggers";
    i.captionText = "   Log activity";
    i.tooltip = "All activity is saved in the log file and can be reviewed in help."                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::isFileLogger;
    i.key = "activityLog";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Error log
    i.name = "errorLog";
    i.parentName = "loggers";
    i.captionText = "   Log errors";
    i.tooltip = "Errors are saved in the error file and can be reviewed in help."                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::isErrorLogger;
    i.key = "errorLog";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
    */

void Preferences::addUserInterface()
{
    // UserInterfaceHeader Header (Root) ---------------------------------------------------------------
    i.name = "UserInterfaceHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "User Interface";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    // Application background luminousity
    i.name = "globalBackgroundShade";
    i.parentName = "UserInterfaceHeader";
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

    // Mouse wheel sensitivity
    i.name = "wheelSensitivity";
    i.parentName = "UserInterfaceHeader";
    i.captionText = "Next/prev image scroll sensitivity";
    i.tooltip = "Fine tune the sensitivity of the mouse wheel or trackpad\n"
                "to trigger the next or previous image in loupe view.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 75;
    i.value = G::wheelSensitivity;
    i.key = "wheelSensitivity";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 10;
    i.max = 210;
    i.fixedWidth = 50;
    addItem(i);

    // UserInterfaceHeader > Font size header
    i.name = "FontSizeHeader";
    i.parentName = "UserInterfaceHeader";
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
    i.value = G::strFontSize;
    i.key = "globalFontSize";
    i.delegateType = DT_Spinbox;
//    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 6;
    i.max = 24;
    i.fixedWidth = 50;
    addItem(i);

    // Info overlay font size
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
    i.parentName = "UserInterfaceHeader";
    i.captionText = "Classification badge / Icon number size";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Loupe classification badge
    i.name = "classificationBadgeInImageDiameter";
    i.parentName = "BadgeSizeHeader";
    i.tooltip = "The image badge is a circle showing the colour classification, rating and pick\n"
                "status.  It is located in the lower right corner of the image.  This property \n"
                "allows you to adjust its size.";
    i.captionText = "Loupe  classification badge size";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 36;
    i.value =mw->classificationBadgeInImageDiameter;
    i.key = "classificationBadgeInImageDiameter";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);

    // Thumbnail classification badge
    i.name = "classificationBadgeSizeFactor";
    i.parentName = "BadgeSizeHeader";
    i.tooltip = "The icon badge is a small bar showing the colour classification and rating\n"
                "status.  It is located in the top right corner of the thumbnail in the thumb \n"
                "and grid views.  This property adjusts the size of the badge.\n\n"
                "Note that the badge size is also proportional to the font size."
                ;
    i.captionText = "Icon classification badge size";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 19;
    i.value =mw->classificationBadgeSizeFactor;
    i.key = "classificationBadgeSizeFactor";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 3;
    i.max = 20;
    i.fixedWidth = 50;
    // addItem(i);

    // Thumbnail count number size
    i.name = "iconNumberSize";
    i.parentName = "BadgeSizeHeader";
    i.tooltip = "The image number is located in the top left corner.  This property\n"
                "adjusts the size of the number.";
    i.captionText = "Icon count size";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 19;
    i.value =mw->iconNumberSize;
    i.key = "iconNumberSize";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 10;
    i.max = 60;
    i.fixedWidth = 50;
    addItem(i);

    // Label Header
    i.name = "LabelHeader";
    i.parentName = "UserInterfaceHeader";
    i.captionText = "Thumbnail labels (file name / title)";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Label what to show
    i.name = "labelChoice";
    i.parentName = "LabelHeader";
    i.captionText = "What to show";
    i.tooltip = "Select what to show: file name or title.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->gridView->labelChoice;
    i.key = "labelChoice";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "File name"
               << "Title";
    addItem(i);

    // Filmstrip label font size
    i.name = "thumbViewLabelSize";
    i.parentName = "LabelHeader";
    i.captionText = "Filmstrip label size";
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
    i.parentName = "LabelHeader";
    i.captionText = "Show filmstrip labels";
    i.tooltip = "Show or hide the label with the file name / title at the bottom of each thumbnail.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 8;
    i.value = mw->thumbView->showIconLabels;
    i.key = "thumbViewShowLabel";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Grid label font size
    i.name = "gridViewLabelSize";
    i.parentName = "LabelHeader";
    i.captionText = "Grid view label size";
    i.tooltip = "Change the display size of the file name / title shown at the bottom of each thumbnail.";
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
    i.parentName = "LabelHeader";
    i.captionText = "Show grid view labels";
    i.tooltip = "Show or hide the label with the file name / title at the bottom of each thumbnail.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->gridView->showIconLabels;
    i.key = "gridViewShowLabel";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Show IconView tooltip legend help
    i.name = "showThumbNailSymbolHelp";
    i.parentName = "UserInterfaceHeader";
    i.captionText = "Show thumbnail tooltip legend";
    i.tooltip = "Show or hide the legend in the thumbnail tooltip.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->dm->showThumbNailSymbolHelp;
    i.key = "showThumbNailSymbolHelp";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Ingest audio Header
    i.name = "PickAudioHeader";
    i.parentName = "UserInterfaceHeader";
    i.captionText = "Ingest audio feedback";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    addItem(i);

    // Play audio feedback when pick, unpick or reject
    i.name = "playPickAudio";
    i.parentName = "PickAudioHeader";
    i.captionText = "Make a click sound when pick, unpick or reject";
    i.tooltip = "Make a click sound when pick, unpick or reject.";
    i.hasValue = true;
    i.captionIsEditable = false;
    if (mw->settings->contains("playPickAudio"))
        i.value = mw->settings->value("playPickAudio").toBool();
    else
        i.value = false;
    i.key = "playPickAudio";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Pick audio volume
    i.name = "pickAudioVolume";
    i.parentName = "PickAudioHeader";
    i.tooltip = "The image number is located in the top left corner.  This property\n"
                "adjusts the size of the number.";
    i.captionText = "Pick audio volume";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 25;
    if (mw->settings->contains("pickAudioVolume"))
        i.value = mw->settings->value("pickAudioVolume").toInt();
    else
        i.value = 25;
    i.key = "pickAudioVolume";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    addItem(i);
}

void Preferences::addCache()
{
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

    // Show caching activity
    i.name = "showCacheProgressBar";
    i.parentName = "CacheHeader";
    i.captionText = "Show caching progress";
    i.tooltip = "Show or hide the cache progress bar in the status area on the lower right.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->isShowCacheProgressBar;
    i.key = "showCacheProgressBar";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Set the width of the cache status progress bar
    i.name = "progressWidthSlider";
    i.parentName = "CacheHeader";
    i.captionText = "Cache progress bar width";
    i.tooltip = "Change the width of the cache progress bar in the status bar.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 120;
    i.value = mw->cacheBarProgressWidth;
    i.key = "progressWidthSlider";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 100;
    i.max = 2000;
    i.fixedWidth = 50;
    addItem(i);
    // hide/show progressWidthSlider in preferences
    // QModelIndex idx = findCaptionIndex("showCacheProgressBar");
    // if (model->index(idx.row(), 1, idx.parent()).data().toBool())
    //     setRowHidden(capIdx.row(), capIdx.parent(), false);
    // else
    //     setRowHidden(capIdx.row(), capIdx.parent(), true);

    // Metadata chunk size (number of thumbnails)
    i.name = "iconChunkSize";
    i.parentName = "CacheHeader";
    i.captionText = "Maximum thumbnails to cache";
    i.tooltip = "Enter the minimum number of thumbnails you want to cache.\n"
                "For the best performance this should be greater than the\n"
                "number of images in the folder.\n"
                "•  1000 thumbnails consumes approximately 18 MB of RAM\n"
                "•  3000 is the default amount.\n";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->dm->defaultIconChunkSize;
    i.key = "iconChunkSize";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 10;
    i.max = G::maxIconChunk;
    i.fixedWidth = 50;
    addItem(i);

    // Image cache maximum size excluding metadata cache
    i.name = "imageCacheSize";
    i.parentName = "CacheHeader";
    i.captionText = "Cache size";
    i.tooltip = "The maximum cache size in GB.  Auto allows Winnow to optimise the\n"
                "cache size dynamically.\n\n"
                "Note the cache size will be reduced if the operating system does\n"
                "not have enough memory available.\n\n"
                "Maximise performance:\n"
                "  • turn color management off\n"
                "  • set number of thumbnails to cache to greater than folder count\n"
                "  • hide caching progess\n"
        ;
    i.hasValue = true;
    i.captionIsEditable = false;
    QHash<int, QString> mbToSize = {
        {0,     "Auto Frugal"},
        {0,     "Auto Moderate"},
        {0,     "Auto Greedy"},
        {500,   "0.5 GB"},
        {1000,  "1 GB"},
        {2000,  "2 GB"},
        {4000,  "4 GB"},
        {6000,  "6 GB"},
        {8000,  "8 GB"},
        {12000, "12 GB"},
        {16000, "16 GB"},
        {24000, "24 GB"},
        {32000, "32 GB"},
        {48000, "48 GB"},
        {64000, "64 GB"}
    };
    qDebug() << "pref maxMB =" << mw->imageCache->getMaxMB();
    if (mw->imageCache->getAutoMaxMB())
        i.value = "Auto " + mw->imageCache->getAutoStrategy();
    else
        i.value = mbToSize.value(mw->imageCache->getMaxMB());
    i.key = "imageCacheSize";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "Auto Frugal"
               << "Auto Moderate"
               << "Auto Greedy"
               << "0.5 GB"
               << "1 GB"
               << "2 GB"
               << "4 GB"
               << "6 GB"
               << "8 GB"
               << "12 GB"
               << "16 GB"
               << "24 GB"
               << "32 GB"
               << "48 GB"
               << "64 GB"
                ;
    addItem(i);

    // // Available memory for caching
    // i.name = "availableMBToCache";
    // i.parentName = "CacheHeader";
    // i.captionText = "Image cache / Available memory";
    // i.tooltip = "The total amount of available memory in MB.  On some\n"
    //             "systems, including Mac M chips, you can allocate more\n"
    //             "than available, due to excellent memory swapping\n"
    //             "performance."
    //             ;
    // i.hasValue = true;
    // i.captionIsEditable = false;
    // // get available memory
    // #ifdef Q_OS_WIN
    // Win::availableMemory();     // sets G::availableMemoryMB
    // #endif
    // #ifdef Q_OS_MAC
    // Mac::availableMemory();     // sets G::availableMemoryMB
    // #endif
    // int memAvail = mw->icd->sizeMB() + G::availableMemoryMB;
    // i.value = QString::number(mw->cacheMaxMB) + " of " +
    //         QString::number(memAvail) + " available MB";
    // i.key = "availableMBToCache";
    // i.delegateType = DT_Label;
    // i.type = "QString";
    // i.color = G::disabledColor.name();
    // availMBMsgWidget = addItem(i);
}

void Preferences::addSlideShow()
{
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
    i.name = "slideShowDelay";
    i.parentName = "SlideshowHeader";
    i.captionText = "Slideshow delay (sec)";
    i.tooltip = "Enter the slideshow delay in seconds.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->slideShowDelay;
    i.key = "slideShowDelay";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 86400;
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
    i.tooltip = "Wrap mode goes back to the beginning when the last image has been shown.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->isSlideShowWrap;
    i.key = "isSlideShowWrap";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);
}

void Preferences::addFullScreen()
{
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

void Preferences::addMetadataPanel()
{
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

    // Metadata InfoView items

    // InfoView fields to show
    if (G::useInfoView) {
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
        } // end Metadata InfoView items
    } // end if (mw->G::useInfoView)
}

void Preferences::addTableView()
{
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
    for (int row = 0; row < tv->rowCount(); row++) {
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

void Preferences::addUtilities()
{
    // Utilities Header (Root)
    i.name = "UtilitiesHeader";
    i.parentName = "???";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Utilities";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

        // Utilities Header (Root)
        i.name = "FocusStackHeader";
        i.parentName = "UtilitiesHeader";
        i.isHeader = true;
        i.isDecoration = true;
        i.decorateGradient = true;
        i.captionText = "Focus Stack Utility";
        i.tooltip = "";
        i.hasValue = false;
        i.captionIsEditable = false;
        i.delegateType = DT_None;
        addItem(i);

            // Focus stack mdethod
            i.name = "focusStackMethod";
            i.parentName = "FocusStackHeader";
            i.captionText = "Focus stack method";
            i.tooltip =
                "PMax:               Align and fuse using multiscale wavelets\n"
                "StreamPMax:         Same as PMax but saves memory\n"
                "TennengradVersions: Depths for various radius/thresholds\n"
                ;
            i.hasValue = true;
            i.captionIsEditable = false;
            i.value = mw->fsMethod;
            i.key = "focusStackMethod";
            i.delegateType = DT_Combo;
            i.type = "QString";
            i.dropList = FS::MethodsString;
            addItem(i);

            // Cleanup working files after focus stacking
            i.name = "deleteInputStackImages";
            i.parentName = "FocusStackHeader";
            i.captionText = "Remove all temporary folders";
            i.tooltip =
                "If Focus Stack was initiated remotely (ie from Lightroom)\n"
                "then a subfolder called 'FocusStack' will have been created\n"
                "with the temporary input images.  Also, working subfolders\n"
                "will exist, and they will also be removed.";
            i.hasValue = true;
            i.captionIsEditable = false;
            i.value = mw->checkIfUpdate;
            i.key = "deleteInputStackImages";
            i.delegateType = DT_Checkbox;
            i.type = "bool";
            addItem(i);

            // 8-bit pipeline
            i.name = "8bitFSPipeline";
            i.parentName = "FocusStackHeader";
            i.captionText = "Limit to 8-bit pipeline";
            i.tooltip =
                "If the input images are 16-bit they will be converted to\n"
                 "8-bit and the output fused image will be 8-bit.\n";
            i.hasValue = true;
            i.captionIsEditable = false;
            i.value = mw->checkIfUpdate;
            i.key = "8bitFSPipeline";
            i.delegateType = DT_Checkbox;
            i.type = "bool";
            addItem(i);

}

    // enable/disable rory items
    // rory();

