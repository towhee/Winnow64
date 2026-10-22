#include "preferences.h"
#include "Main/mainwindow.h"
#ifdef Q_OS_LINUX
#include "Utilities/linux.h"
#endif
#include "Main/global.h"
#include <QDebug>
#include "Cache/devpreviewcache.h"
#include "Develop/rawrender.h"
#ifdef Q_OS_MAC
#include "ImageFormats/Raw/applerawdecode.h"
#endif
#include "Cache/thumbcache.h"

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
    stringToFitValues   = "===String to fit in values column===";
    resizeColumns();

    addItems();
}

void Preferences::rory()
{
    QModelIndex capIdx;

    capIdx = findCaptionIndex("hideCachingProgressBars");
    if (G::isRory) setRowHidden(capIdx.row(), capIdx.parent(), false);
    else setRowHidden(capIdx.row(), capIdx.parent(), true);

    capIdx = findCaptionIndex("progressWidthSlider");
    if (G::isRory) setRowHidden(capIdx.row(), capIdx.parent(), false);
    else setRowHidden(capIdx.row(), capIdx.parent(), true);

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

    if (source == "disableColorCorrection") {
        // checkbox is "disable", so colour management is the inverse
        bool disable = v.toBool();
        // only toggle (which rebuilds the image cache) if the state actually changes
        if (G::colorManage == disable)
            mw->toggleColorManage(disable ? MW::off : MW::on);
        mw->settings->setValue("colorManage", G::colorManage);
    }

    if (source == "hideCachingProgressBars") {
        /* Single gate for ImageCache + MetaRead progress. Checkbox is "hide", so
           G::showCacheProgress is the inverse. ImageCache and MetaRead read
           G::showCacheProgress directly, so no per-class setters are needed. */
        bool hide = v.toBool();
        G::showCacheProgress = !hide;
        /* Gate the cache rows; Progress shows/hides its own container from row
           content, so we don't toggle the whole widget here (that would also hide
           a concurrent FocusStack / RawDenoise row). */
        mw->setCacheProgressEnabled(G::showCacheProgress);
        mw->settings->setValue("showCacheProgress", G::showCacheProgress);
    }

    if (source == "cacheThumbsJIT") {
        G::useJitIconCache = v.toBool();
        mw->settings->setValue("useJitIconCache", G::useJitIconCache);
        /*  Applied on the NEXT folder load, like the thumbnail ceiling beside it and for
            the same reason: the window is resolved once per load
            (DataModel::resolveIconChunkSize), and re-resolving it under a folder the user
            is looking at would drop thumbnails currently on screen. */
    }

    if (source == "progressWidthSlider") {
        mw->cacheBarProgressWidth = v.toInt();
        mw->updateProgressBarWidth();
    }

    if (source == "maxIconSize") {
        G::maxIconSize = v.toInt();
    }

    /* Developed previews. Only NEW previews are written at a changed size or quality --
       the ones already cached stay valid, because the staleness test is the recipe hash,
       not the dimensions or the encoding. Lowering either setting therefore reclaims disk
       gradually; "Clear developed preview cache" on the Develop menu is the immediate
       route. */
    if (source == "devPreviewSize") {
        G::devPreviewMaxEdge = devPreviewSizeValue(v.toString());
        mw->settings->setValue("devPreviewMaxEdge", G::devPreviewMaxEdge);
    }

    if (source == "devPreviewQuality") {
        G::devPreviewQuality = v.toInt();
        mw->settings->setValue("devPreviewQuality", G::devPreviewQuality);
    }

    if (source == "devPreviewCacheSize") {
        G::devPreviewCacheMaxBytes = devPreviewCacheValue(v.toString());
        mw->settings->setValue("devPreviewCacheMaxBytes", G::devPreviewCacheMaxBytes);
        /* Applied now, not next launch: lowering the limit should reclaim the disk while
           the user is looking at the setting they just changed. */
        DevPreviewCache::instance().setMaxBytes(G::devPreviewCacheMaxBytes);
    }

    if (source == "maxSearchResults") {
        G::maxSearchResults = maxIconChunkValue(v.toString());
        mw->settings->setValue("maxSearchResults", G::maxSearchResults);
        /*  Applied to the NEXT search, which is what the user will do next
            anyway -- re-running the current one would replace what they are
            looking at without being asked. */
    }

    if (source == "maxIconChunk") {
        G::maxIconChunk = maxIconChunkValue(v.toString());
        mw->settings->setValue("maxIconChunk", G::maxIconChunk);
        /*  Applied on the NEXT folder load, not now: the icon window is resolved
            once per load (DataModel::resolveIconChunkSize) and re-resolving it
            underneath a folder the user is looking at would drop thumbnails they
            can currently see. */
    }

    if (source == "cacheThumbnails") {
        G::cacheThumbnails = v.toBool();
        mw->settings->setValue("cacheThumbnails", G::cacheThumbnails);
    }

    if (source == "useScrollInVerify") {
        G::useScrollInVerify = v.toBool();
        mw->settings->setValue("useScrollInVerify", G::useScrollInVerify);
    }
    if (source == "autoScanCatalog") {
        G::autoScanCatalog = v.toBool();
        mw->settings->setValue("autoScanCatalog", G::autoScanCatalog);
    }
    if (source == "autoScanCatalogMinutes") {
        G::autoScanCatalogMinutes = qBound(1, v.toInt(), 1440);
        mw->settings->setValue("autoScanCatalogMinutes", G::autoScanCatalogMinutes);
    }

    if (source == "useIndexMetadata") {
        G::useIndexMetadata = v.toBool();
        mw->settings->setValue("useIndexMetadata", G::useIndexMetadata);
    }

    if (source == "thumbCacheSize") {
        G::thumbCacheMaxBytes = devPreviewCacheValue(v.toString());
        mw->settings->setValue("thumbCacheMaxBytes", G::thumbCacheMaxBytes);
        /* Applied now, not next launch, for the same reason the develop preview
           cap is: lowering the limit should reclaim the disk while the user is
           looking at the setting they just changed. */
        ThumbCache::instance().setMaxBytes(G::thumbCacheMaxBytes);
    }

    if (source == "buildDevPreviewsInBackground") {
        G::buildDevPreviewsInBackground = v.toBool();
        mw->settings->setValue("buildDevPreviewsInBackground",
                               G::buildDevPreviewsInBackground);
    }

    if (source == "autoRemoveCA") {
        G::autoRemoveCA = v.toBool();
        mw->settings->setValue("Develop/autoRemoveCA", G::autoRemoveCA);
        /* An unset recipe follows it: the Lens Remove CA box and the current render change.
           (Metadata::defaultRenderKey / devPreviewKey hash it, so cached previews built
           under the other setting are simply not served.) */
        if (mw->developProperties) {
            mw->developProperties->syncLensPanel();
            emit mw->developProperties->paramsChanged();
        }
    }

    if (source == "defaultRawRenderer") {
        /* The DEFAULT only: images with their own "Render using" keep it. Bases are
           filed per renderer (WorkingImageCache tags) and the render keys hash the
           renderer, so nothing needs clearing -- a re-render picks the new default up. */
        for (const RawRender::Choice &c : rawRendererChoices()) {
            if (c.label() != v.toString()) continue;
            G::decodeRawEngine = c.apple ? G::DecodeRawEngine::appleDecodeRawEngine
                                         : G::DecodeRawEngine::winnowDecodeRawEngine;
#ifdef Q_OS_MAC
            AppleRawDecode::SetVersionPin(c.applePin);
            mw->settings->setValue("appleRawDecoderVersion", c.applePin);
#endif
            mw->settings->setValue("decodeRawEngine", static_cast<int>(G::decodeRawEngine));
            if (mw->developProperties) {
                mw->developProperties->reflectEngine();
                emit mw->developProperties->paramsChanged();
            }
            break;
        }
    }

    if (source == "autoLensProfile") {
        G::autoLensProfile = v.toBool();
        mw->settings->setValue("Develop/autoLensProfile", G::autoLensProfile);
        /* Same as autoRemoveCA: an unset recipe follows it. */
        if (mw->developProperties) {
            mw->developProperties->syncLensPanel();
            emit mw->developProperties->paramsChanged();
        }
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
            }
            else if (size == "Auto Moderate") {
                as = ImageCache::AutoStrategy::Moderate;
            }
            else if (size == "Auto Greedy") {
                as = ImageCache::AutoStrategy::Greedy;
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
        #ifdef Q_OS_LINUX
        Linux::availableMemory();   // sets G::availableMemoryMB
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

    // Map module tile provider (see MapTileSource)
    if (source == "mapTileUrl") {
        /* Entering a URL is choosing it: switch the map to Custom. Clearing it goes
           back to Standard (applyMapProvider). */
        mw->mapTileUrl = v.toString();
        if (!mw->mapTileUrl.trimmed().isEmpty()) mw->mapStyle = "custom";
        mw->applyMapProvider();
    }
    if (source == "mapTileKey") {
        mw->mapTileKey = v.toString();
        mw->applyMapProvider();
    }
    if (source == "mapAttribution") {
        mw->mapAttribution = v.toString();
        mw->applyMapProvider();
    }
    if (source == "mapMaxZoom") {
        mw->mapMaxZoom = v.toInt();
        mw->applyMapProvider();
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

    if (source == "pickLossWarning") {
        mw->pickLossWarning = v.toBool();
    }

    if (source == "modifySourceFiles") {
        G::modifySourceFiles = v.toBool();
        // mw->embedThumbnailsAction->setEnabled(G::modifySourceFiles);
        // toggle status bar modify images button
        mw->toggleModifyImages();
        // setItemEnabled("backupBeforeModify", G::modifySourceFiles);
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

    if (source == "activityLog") {
        G::isLogger = v.toBool();
        mw->settings->setValue("isFileLogger", v.toBool());
    }

    if (source == "renderVideoThumb") {
        G::renderVideoThumb = v.toBool();
    }

    if (source == "isLogAllToFileForDebugging") {
        mw->isLogAllToFileForDebugging = v.toBool();
        G::isLogger = v.toBool();
        G::isIssueLogger = v.toBool();
        G::sendLogToFile = v.toBool();
        // if (G::isErrorLogger) mw->startLog();
    }

    if (source == "rememberLastDir") {
        mw->rememberLastDir = v.toBool();
    }

    if (source == "checkIfUpdate") {
        mw->checkIfUpdate = v.toBool();
    }

    /*  Also takes effect at the next start; the state is saved regardless of when this
        is turned on, from the next time the Library is left or Winnow quits. */
    if (source == "restoreLibraryState") {
        mw->restoreLibraryState = v.toBool();
    }

    if (source == "limitFit100Pct") {
        mw->imageView->limitFit100Pct = v.toBool();
        mw->imageView->refresh();
    }

    if (source == "wheelSensitivity") {
        G::wheelSensitivity = qBound(1, v.toInt(), 100);
    }

    if (source == "globalFontSize") {
        mw->setFontSize(v.toInt());     // recomputes column widths via resizeColumns()
        G::fontSize = v.toInt();
        setStyleSheet(G::css);
        // keep the dialog width matched to the new column widths so the
        // caption:value proportions are preserved (clamped to the screen)
        if (mw->preferencesDlg) mw->preferencesDlg->resizeForFontChange();
    }

    if (source == "globalBackgroundShade") {
        mw->setBackgroundShade(v.toInt());
        setStyleSheet(G::css);
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

    if (source == "fullScreenShowCatalog") {
        mw->fullScreenDocks.isCatalog = v.toBool();
    }

    if (source == "fullScreenShowKeywords") {
        mw->fullScreenDocks.isKeywords = v.toBool();
    }

    if (source == "fullScreenShowCollections") {
        mw->fullScreenDocks.isCollections = v.toBool();
    }

    if (source == "fullScreenShowQueries") {
        mw->fullScreenDocks.isQueries = v.toBool();
    }

    if (source == "fullScreenShowPlaces") {
        mw->fullScreenDocks.isPlaces = v.toBool();
    }

    if (source == "fullScreenShowModule") {
        mw->fullScreenDocks.isModule = v.toBool();
    }

    if (source == "fullScreenShowMetadata") {
        mw->fullScreenDocks.isMetadata = v.toBool();
    }

    if (source == "fullScreenShowDevelop") {
        mw->fullScreenDocks.isDevelop = v.toBool();
    }

    if (source == "fullScreenShowHistory") {
        mw->fullScreenDocks.isHistory = v.toBool();
    }

    if (source == "fullScreenShowEmbellish") {
        mw->fullScreenDocks.isEmbellish = v.toBool();
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

    if (source == "focusStackRemoveTemp") {
        mw->fsRemoveTemp = v.toBool();
        mw->writeSetting("focusStackRemoveTemp", v.toBool());  // also saved in MW::writeSettings
    }

    if (source == "focusStack8bit") {
        mw->fs8bit = v.toBool();
        mw->writeSetting("focusStack8bit", v.toBool());  // also saved in MW::writeSettings
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
    addCaching();
    addDevPreviews();
    addCatalog();
    addSlideShow();
    addMap();
    addFullScreen();
    addMetadataPanel();
    addTableView();
    if (G::isRory) addUtilities();
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

    /*  FIRST under General, because it is about what Winnow does before the user has
        done anything. Which source Winnow starts in is not a preference -- it is always
        the one left showing; this decides whether what was loaded comes back. */
    i.name = "restoreLibraryState";
    i.parentName = "GeneralHeader";
    i.captionText = "Reopen as it was left";
    i.tooltip = "Reopen what was loaded when Winnow last closed, with its filters\n"
                "and selection.\n"
                "Hold Shift at launch to start fresh.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->restoreLibraryState;
    i.key = "restoreLibraryState";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Allow source files to be changed
    i.name = "modifySourceFiles";
    i.parentName = "GeneralHeader";
    i.captionText = "Permit image file modification";
    i.tooltip = "Write ratings, labels, keywords and other metadata into JPEG,\n"
                "TIFF, PNG and DNG files, so Lightroom can see them.\n"
                "Off: files are never changed and edits go to a sidecar.\n\n"
                "There is a small risk of damaging a file. Consider \"Backup\n"
                "before modify files\".";
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
    // setItemEnabled("backupBeforeModify", G::modifySourceFiles);

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
    // addItem(i);

    // Auto advance
    i.name = "autoAdvance";
    i.parentName = "GeneralHeader";
    i.captionText = "Auto advance after pick, rate or label.";
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
    // addItem(i);

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

    // Picks not ingested warning
    i.name = "pickLossWarning";
    i.parentName = "GeneralHeader";
    i.captionText = "Warn if picks have not been ingested";
    i.tooltip = "Warn before loading something else or quitting would lose picks\n"
                "that have not been ingested.\n"
                "Turn off if you use picks for something other than ingesting.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->pickLossWarning;
    i.key = "pickLossWarning";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Render video thumbnails
    i.name = "renderVideoThumb";
    i.parentName = "GeneralHeader";
    i.captionText = "Render video thumbnails";
    i.tooltip = "Show the first frame of each video as its thumbnail.\n"
                "Turn off to show a plain video placeholder instead, which is\n"
                "faster in folders with many videos.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::renderVideoThumb;
    i.key = "renderVideoThumb";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Issue Logger
    i.name = "isLogAllToFileForDebugging";
    i.parentName = "GeneralHeader";
    i.captionText = "Turn error logging on";
    i.tooltip = "Turn this on to write errors to a log file.\n"
                "Warning: this will impact performance.  Use\n"
                "to help resolve bugs and crashes.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->isLogAllToFileForDebugging;
    i.key = "isLogAllToFileForDebugging";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Color management
    i.name = "disableColorCorrection";
    i.parentName = "GeneralHeader";
    i.captionText = "Disable color correction";
    i.tooltip = "Disabled color correction is faster.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = !G::colorManage;
    i.key = "disableColorCorrection";
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
    // addItem(i);

    // Allow source files to be changed
    i.name = "modifySourceFiles";
    i.parentName = "GeneralHeader";
    i.captionText = "Permit image file modification";
    i.tooltip = "Permit file modification to the image file.  In some\n"
                "cases, changes (ie orientation) may still be made to\n"
                "a sidecar xmp file."
                ;
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::modifySourceFiles;
    i.key = "modifySourceFiles";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    // addItem(i);       // cancel this for version 1.32 release

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
    // addItem(i);
    // setItemEnabled("backupBeforeModify", G::modifySourceFiles);

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
    // if (G::useMissingThumbs) addItem(i);
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
    // if (G::useMissingThumbs) addItem(i);
    setItemEnabled("ignoreAddThumbnailsDlg", G::modifySourceFiles);
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
    i.captionText = "Mouse wheel / trackpad sensitivity";
    i.tooltip = "How far the thumbnails scroll for each wheel click or swipe.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 50;
    i.value = G::wheelSensitivity;
    i.key = "wheelSensitivity";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 1;
    i.max = 100;
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

    // Thumbnail count number size
    i.name = "iconNumberSize";
    i.parentName = "FontSizeHeader";
    i.tooltip = "The image number is located in the top left corner.  This property\n"
                "adjusts the size of the number.";
    i.captionText = "Image count";
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

    // General category::Badge size subcategory
    i.name = "BadgeSizeHeader";
    i.parentName = "UserInterfaceHeader";
    i.captionText = "Classification badge / Icon number size";
    i.tooltip = "";
    i.hasValue = false;
    i.captionIsEditable = false;
    // addItem(i);

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
    i.value = mw->classificationBadgeInImageDiameter;
    i.key = "classificationBadgeInImageDiameter";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 0;
    i.max = 100;
    i.fixedWidth = 50;
    // addItem(i);

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
    i.parentName = "FontSizeHeader";
    i.captionText = "Filmstrip file name / title";
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
    i.captionText = "Show in filmstrip";
    i.tooltip = "Show or hide the file name / title at the bottom of each thumbnail in the filmstrip.";
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
    i.parentName = "FontSizeHeader";
    i.captionText = "Grid view file name / title";
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
    i.captionText = "Show in grid";
    i.tooltip = "Show or hide the file name / title at the bottom of each thumbnail in grid view.";
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
    // addItem(i);

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
    i.tooltip = "Adjust audio click volume.";
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

QString Preferences::devPreviewSizeLabel(int maxEdge)
{
    if (maxEdge == G::kDevPreviewSizeLarge)  return "Large (4096 px)";
    if (maxEdge == G::kDevPreviewSizeScreen) return "Screen (2560 px)";
    return "Full size";                      // kDevPreviewSizeFull, and the fallback
}

int Preferences::devPreviewSizeValue(const QString &label)
{
    if (label.startsWith("Large"))  return G::kDevPreviewSizeLarge;
    if (label.startsWith("Screen")) return G::kDevPreviewSizeScreen;
    return G::kDevPreviewSizeFull;
}

/*  Shared by the thumbnail ceiling and the search-result cap: both are "how many
    images at once", both offer the same steps, and both treat 0 as no limit. */
QString Preferences::maxIconChunkLabel(int n)
{
    if (n <= 0) return "No limit";
    /*  Snap to an offered value rather than inventing a label the combo has no
        item for -- an unmatched value leaves the control blank. */
    const QList<int> sizes {2000, 5000, 10000, 20000, 50000};
    int best = sizes.first();
    for (int s : sizes) if (qAbs(s - n) < qAbs(best - n)) best = s;
    return QLocale().toString(best);
}

int Preferences::maxIconChunkValue(const QString &label)
{
    if (label.startsWith("No")) return 0;          // no limit
    const int n = QLocale().toInt(label);
    return n > 0 ? n : 10000;
}

QString Preferences::devPreviewCacheLabel(qint64 bytes)
{
    const qint64 gb = bytes / (1024LL * 1024 * 1024);
    /* Snap to the nearest offered size rather than inventing a label the combo has no
       item for -- an unmatched value would leave the control blank. */
    const QList<qint64> sizes {5, 10, 20, 50, 100, 200};
    qint64 best = sizes.first();
    for (qint64 s : sizes) if (qAbs(s - gb) < qAbs(best - gb)) best = s;
    return QString("%1 GB").arg(best);
}

qint64 Preferences::devPreviewCacheValue(const QString &label)
{
    const qint64 gb = label.split(' ').first().toLongLong();
    return (gb > 0 ? gb : 20) * 1024LL * 1024 * 1024;
}

void Preferences::addCaching()
{
/*
    Everything Winnow keeps so it does not have to decode or read again: the image
    cache in memory, thumbnails in memory and on disk, and developed previews on disk.
    The status-bar cache progress opens this section.
*/
    // Caching header (Root)
    i.name = "CachingHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Caching";
    i.tooltip = "Memory and disk Winnow uses to show images and thumbnails faster.";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    // Image cache maximum size excluding metadata cache
    i.name = "imageCacheSize";
    i.parentName = "CachingHeader";
    i.captionText = "Image cache size";
    i.tooltip = "The maximum memory for the image cache in GB.  Auto allows\n"
                "Winnow to optimise the cache size dynamically.\n\n"
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
    // qDebug() << "pref maxMB =" << mw->imageCache->getMaxMB();
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

    // Show caching activity
    i.name = "hideCachingProgressBars";
    i.parentName = "CachingHeader";
    i.captionText = "Hide caching progress";
    i.tooltip = "Show or hide the cache progress bar in the status area on the lower right."
                "\nHiding is faster.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = !G::showCacheProgress;
    i.key = "hideCachingProgressBars";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Set the width of the cache status progress bar
    i.name = "progressWidthSlider";
    i.parentName = "CachingHeader";
    i.captionText = "Cache progress bar width";
    i.tooltip = "Change the width of the cache progress bar in the status bar.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 300;
    i.value = mw->cacheBarProgressWidth;
    i.key = "progressWidthSlider";
    i.delegateType = DT_Slider;
    i.type = "int";
    i.min = 100;
    i.max = 2000;
    i.fixedWidth = 50;
    addItem(i);

    i.name = "maxIconChunk";
    i.parentName = "CachingHeader";
    i.captionText = "Thumbnails held in memory";
    i.tooltip = "How many thumbnails are kept in memory (about 1.8 GB per 10,000).\n"
                "Higher scrolls large folders more smoothly but uses more memory.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = maxIconChunkLabel(G::maxIconChunk);
    i.key = "maxIconChunk";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "2,000" << "5,000" << "10,000" << "20,000" << "50,000" << "No limit";
    addItem(i);

    // Cache thumbnails JIT
    i.name = "cacheThumbsJIT";
    i.parentName = "CachingHeader";
    i.captionText = "Cache thumbs just in time";
    i.tooltip = "Load thumbnails only for the pages around where you are looking.\n"
                "Pages appear sooner in very large folders and the Library.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::useJitIconCache;
    i.key = "cacheThumbsJIT";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    i.name = "cacheThumbnails";
    i.parentName = "CachingHeader";
    i.captionText = "Cache thumbnails";
    i.tooltip = "Save thumbnails on disk so folders and the Library show them\n"
                "faster the next time.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::cacheThumbnails;
    i.key = "cacheThumbnails";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    i.name = "thumbCacheSize";
    i.parentName = "CachingHeader";
    i.captionText = "Thumbnail cache size";
    i.tooltip = "Disk space for saved thumbnails. The oldest are removed when it\n"
                "is full.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = devPreviewCacheLabel(G::thumbCacheMaxBytes);
    i.key = "thumbCacheSize";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "5 GB" << "10 GB" << "20 GB" << "50 GB" << "100 GB" << "200 GB";
    addItem(i);

    // devPreview cache limit
    i.name = "devPreviewCacheSize";
    i.parentName = "CachingHeader";
    i.captionText = "Developed preview cache size";
    i.tooltip = "Disk space for developed previews. The oldest are removed when\n"
                "it is full. Your edits are never lost.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = devPreviewCacheLabel(G::devPreviewCacheMaxBytes);
    i.key = "devPreviewCacheSize";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "5 GB"
               << "10 GB"
               << "20 GB"
               << "50 GB"
               << "100 GB"
               << "200 GB"
        ;
    addItem(i);
}

void Preferences::addDevPreviews()
{
/*
    Developed previews (devPreviews). An edited image has two pictures -- the camera's
    embedded preview and the render of its develop recipe -- and these settings govern how
    the second one is stored. See notes/Documentation.txt "Original and Developed Previews".
*/
    // Develop header (Root)
    i.name = "DevPreviewHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Develop";
    i.tooltip = "Developed previews and the default corrections applied to raw files.";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    // devPreview size
    i.name = "devPreviewSize";
    i.parentName = "DevPreviewHeader";
    i.captionText = "Developed preview size";
    i.tooltip = "Size of the saved developed previews.\n"
                "Full size stays sharp when you zoom in but uses more disk space.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = devPreviewSizeLabel(G::devPreviewMaxEdge);
    i.key = "devPreviewSize";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "Full size"
               << "Large (4096 px)"
               << "Screen (2560 px)"
        ;
    addItem(i);

    // devPreview quality
    i.name = "devPreviewQuality";
    i.parentName = "DevPreviewHeader";
    i.captionText = "Developed preview quality";
    i.tooltip = "JPEG quality of the saved developed previews (default 90).\n"
                "Higher is sharper but uses more disk space. Applies to new\n"
                "previews only.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = G::kDevPreviewQualityDefault;
    i.value = G::devPreviewQuality;
    i.key = "devPreviewQuality";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = G::kDevPreviewQualityMin;
    i.max = G::kDevPreviewQualityMax;
    i.fixedWidth = 50;
    addItem(i);

    // Build devPreviews in the background
    i.name = "buildDevPreviewsInBackground";
    i.parentName = "DevPreviewHeader";
    i.captionText = "Build developed previews in background";
    i.tooltip = "Make a developed preview for every raw file and edited image\n"
                "in the background, so they open as fast as JPEGs.\n\n"
                "In the Library this covers the whole Library and can take\n"
                "many hours. Camera cards are never included.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::buildDevPreviewsInBackground;
    i.key = "buildDevPreviewsInBackground";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Auto-remove chromatic aberration on raw files
    i.name = "autoRemoveCA";
    i.parentName = "DevPreviewHeader";
    i.captionText = "Remove chromatic aberration on raw files";
    i.tooltip = "Turn on Remove CA for raw files by default, to remove colour\n"
                "fringes near the corners. Your own setting on an image is kept.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::autoRemoveCA;
    i.key = "autoRemoveCA";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Lens profile corrections on raw files
    i.name = "autoLensProfile";
    i.parentName = "DevPreviewHeader";
    i.captionText = "Apply lens profile on raw files";
    i.tooltip = "Turn on Lens profile for raw files by default, to correct\n"
                "distortion and vignetting. Your own setting on an image is kept.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::autoLensProfile;
    i.key = "autoLensProfile";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    /* The default raw renderer: what a raw renders with until "Render using" is set on
       it (the choice is per image -- Develop/rawrender.h). Labels as the Raw panel's. */
    i.name = "defaultRawRenderer";
    i.parentName = "DevPreviewHeader";
    i.captionText = "Default raw renderer";
    i.tooltip = "The decoder a raw file renders with until you choose one for it\n"
                "with \"Render using\" in Develop's Raw panel. Images you have set\n"
                "keep their own choice.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = RawRender::defaultChoice().label();
    i.key = "defaultRawRenderer";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    for (const RawRender::Choice &c : rawRendererChoices()) i.dropList << c.label();
    addItem(i);
}

QVector<RawRender::Choice> Preferences::rawRendererChoices()
{
    /* Winnow, then Apple's newest and each version this Mac offers. */
    QVector<RawRender::Choice> out;
    out << RawRender::Choice();
#ifdef Q_OS_MAC
    RawRender::Choice newest;
    newest.apple = true;
    out << newest;
    for (int v : AppleRawDecode::AvailableVersions()) {
        RawRender::Choice c;
        c.apple = true;
        c.applePin = v;
        out << c;
    }
#endif
    return out;
}

void Preferences::addCatalog()
{
/*
    The catalog -- the local index that lets a keyword or title search answer across
    folders instead of only inside the one that is loaded. See notes/Documentation.txt
    "Keywords and Cataloguing". Captioned "Library" in the panel; it also holds the
    search-result cap and catalog-index settings.

    The root list is edited in its own dialog, opened from the Catalog panel's Manage
    button and File > Manage Catalog, not from here.
*/
    // Library header (Root)
    i.name = "CatalogHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Library";
    i.tooltip = "The Library: every photo in the folders you have chosen to catalogue, "
                "searchable together, not just the folder that is loaded.";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    i.name = "autoScanCatalog";
    i.parentName = "CatalogHeader";
    i.captionText = "Scan for changes when the Library is selected";
    i.tooltip = "When you open the Library, look for images added, moved or\n"
                "removed outside Winnow and update the Library in the background.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::autoScanCatalog;
    i.key = "autoScanCatalog";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    i.name = "autoScanCatalogMinutes";
    i.parentName = "CatalogHeader";
    i.captionText = "Minutes between automatic scans";
    i.tooltip = "The shortest time between automatic scans. Scan in Catalogued\n"
                "folders always runs at once.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.defaultValue = 60;
    i.value = G::autoScanCatalogMinutes;
    i.key = "autoScanCatalogMinutes";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 1440;
    i.fixedWidth = 50;
    addItem(i);

    // Search results and the catalog index
    i.name = "maxSearchResults";
    i.parentName = "CatalogHeader";
    i.captionText = "Maximum search results";
    i.tooltip = "The most images the Library loads at once. Leave at \"No limit\"\n"
                "unless you want a very large Library capped.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = maxIconChunkLabel(G::maxSearchResults);
    i.key = "maxSearchResults";
    i.delegateType = DT_Combo;
    i.type = "QString";
    i.dropList.clear();
    i.dropList << "2,000" << "5,000" << "10,000" << "20,000" << "50,000" << "No limit";
    addItem(i);

    i.name = "useIndexMetadata";
    i.parentName = "CatalogHeader";
    i.captionText = "Read metadata from the catalog";
    i.tooltip = "Load catalogued folders faster by reusing the metadata Winnow\n"
                "already has. Changed files are always read again.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::useIndexMetadata;
    i.key = "useIndexMetadata";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    i.name = "useScrollInVerify";
    i.parentName = "CatalogHeader";
    i.captionText = "Verify catalogued images against their files";
    i.tooltip = "Pick up changes made outside Winnow, for example keywords edited\n"
                "in Lightroom. Checks quickly in the background.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = G::useScrollInVerify;
    i.key = "useScrollInVerify";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

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

void Preferences::addMap()
{
/*
    The Map module's tile provider. OPTIONAL: left empty the map uses OpenStreetMap's own
    tiles, which need no key (MapTileSource::Provider::openStreetMap). These fields are
    for someone who wants a different provider they have an account with. See "Map
    Module" in notes/Documentation.txt.
*/
    i.name = "MapHeader";
    i.parentName = "";
    i.isHeader = true;
    i.isDecoration = true;
    i.decorateGradient = true;
    i.captionText = "Map";
    i.tooltip = "Optional.  Leave these empty to use OpenStreetMap, which needs no key.";
    i.hasValue = false;
    i.captionIsEditable = false;
    i.delegateType = DT_None;
    addItem(i);

    i.name = "mapTileUrl";
    i.parentName = "MapHeader";
    i.captionText = "Tile URL template (optional)";
    i.tooltip = "Optional: use another map provider. Leave empty for the built-in\n"
                "maps.\n\n"
                "Paste the tile address from the provider, with {z} {x} {y} and\n"
                "{key}, e.g.\n"
                "https://api.maptiler.com/maps/streets-v2/256/{z}/{x}/{y}.png?key={key}";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->mapTileUrl;
    i.key = "mapTileUrl";
    i.delegateType = DT_LineEdit;
    i.type = "string";
    addItem(i);

    i.name = "mapTileKey";
    i.parentName = "MapHeader";
    i.captionText = "API key";
    i.tooltip = "Your key from the map provider. Stored unencrypted in Winnow's settings.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->mapTileKey;
    i.key = "mapTileKey";
    i.delegateType = DT_LineEdit;
    i.type = "string";
    addItem(i);

    i.name = "mapAttribution";
    i.parentName = "MapHeader";
    i.captionText = "Attribution";
    i.tooltip = "The credit line your provider requires on the map, ie\n"
                "\"© MapTiler © OpenStreetMap contributors\".  Drawn in the\n"
                "bottom right corner of the map.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->mapAttribution;
    i.key = "mapAttribution";
    i.delegateType = DT_LineEdit;
    i.type = "string";
    addItem(i);

    i.name = "mapMaxZoom";
    i.parentName = "MapHeader";
    i.captionText = "Maximum zoom";
    i.tooltip = "The highest zoom level your provider serves (usually 18 to 22).";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->mapMaxZoom;
    i.key = "mapMaxZoom";
    i.delegateType = DT_Spinbox;
    i.type = "int";
    i.min = 1;
    i.max = 22;
    i.fixedWidth = 50;
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
    i.tooltip = "When you switch to full screen show the Folders panel.";
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

    // Full screen - show catalog
    i.name = "fullScreenShowCatalog";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show catalog";
    i.tooltip = "When you switch to full screen show the Catalog panel.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isCatalog;
    i.key = "fullScreenShowCatalog";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show keywords
    i.name = "fullScreenShowKeywords";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show keywords";
    i.tooltip = "When you switch to full screen show the Keywords panel.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isKeywords;
    i.key = "fullScreenShowKeywords";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show collections
    i.name = "fullScreenShowCollections";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show collections";
    i.tooltip = "When you switch to full screen show the Collections panel.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isCollections;
    i.key = "fullScreenShowCollections";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show queries
    i.name = "fullScreenShowQueries";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show queries";
    i.tooltip = "When you switch to full screen show the Queries panel.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isQueries;
    i.key = "fullScreenShowQueries";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show places
    i.name = "fullScreenShowPlaces";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show places";
    i.tooltip = "When you switch to full screen show the Places panel.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isPlaces;
    i.key = "fullScreenShowPlaces";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show module
    i.name = "fullScreenShowModule";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show module";
    i.tooltip = "When you switch to full screen show the Module bar across the top.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isModule;
    i.key = "fullScreenShowModule";
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

    // Full screen - show develop
    i.name = "fullScreenShowDevelop";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show develop";
    i.tooltip = "When you switch to full screen show the develop dock.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isDevelop;
    i.key = "fullScreenShowDevelop";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show develop history
    i.name = "fullScreenShowHistory";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show history panel";
    i.tooltip = "When you switch to full screen show the History panel."; 
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isHistory;
    i.key = "fullScreenShowHistory";
    i.delegateType = DT_Checkbox;
    i.type = "bool";
    addItem(i);

    // Full screen - show embellish
    i.name = "fullScreenShowEmbellish";
    i.parentName = "FullScreenHeader";
    i.captionText = "Show embellish";
    i.tooltip = "When you switch to full screen show the embellish dock.";
    i.hasValue = true;
    i.captionIsEditable = false;
    i.value = mw->fullScreenDocks.isEmbellish;
    i.key = "fullScreenShowEmbellish";
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
            i.name = "focusStackRemoveTemp";
            i.parentName = "FocusStackHeader";
            i.captionText = "Remove all temporary folders";
            i.tooltip =
                "If Focus Stack was initiated remotely (ie from Lightroom)\n"
                "then a subfolder called 'FocusStack' will have been created\n"
                "with the temporary input images.  Also, working subfolders\n"
                "will exist, and they will also be removed.";
            i.hasValue = true;
            i.captionIsEditable = false;
            i.value = mw->fsRemoveTemp;
            i.key = "focusStackRemoveTemp";
            i.delegateType = DT_Checkbox;
            i.type = "bool";
            addItem(i);

            // 8-bit pipeline
            i.name = "focusStack8bit";
            i.parentName = "FocusStackHeader";
            i.captionText = "Limit to 8-bit pipeline";
            i.tooltip =
                "If the input images are 16-bit they will be converted to\n"
                 "8-bit and the output fused image will be 8-bit.\n";
            i.hasValue = true;
            i.captionIsEditable = false;
            i.value = mw->fs8bit;
            i.key = "focusStack8bit";
            i.delegateType = DT_Checkbox;
            i.type = "bool";
            addItem(i);

}

    // enable/disable rory items
    // rory();

