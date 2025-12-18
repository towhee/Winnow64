#include "Main/mainwindow.h"
#include "FocusStack/fs.h"
// #include "FocusStack/Pipeline/PipelineBase.h"
// #include "FocusStack/Pipeline/pipelinepmax.h"


/*
Winnow64/FocusStack/

    Petteri/
        (untouched original sources)

    PetteriModular/
        Core/
        Tasks/
        IO/
        Wrappers/
        Namespace: FStack

    Contracts/
        IAlign.h
        IFocus.h
        IDepth.h
        IFusion.h

    Stages/
        Align/
            PetteriAlign.h/.cpp
            ECCAlign.h/.cpp
            NoOpAlign.h/.cpp
        Fusion/
            PetteriPMaxFusion.h/.cpp
            WeightedPMax.h/.cpp
            DepthAwareFusion.h/.cpp
        Depth/
            PetteriDepth.h/.cpp
            ContrastDepth.h/.cpp
        Focus/
            PetteriFocus.h/.cpp
            GradientFocus.h/.cpp

    Pipeline/
        PipelineBase.h/.cpp
        PipelinePMax.h/.cpp
        PipelineWavePMax.h/.cpp
        PipelineDepthAware.h/.cpp
        PipelineLegacyPetteri.h/.cpp

    Mask/
        maskoptions.h/.cpp
        maskgenerator.h/.cpp
        maskrefiner.h/.cpp
        maskassessor.h/.cpp

    Utils/
        pyramid.h/.cpp
        blend.h/.cpp
        filehelpers.h/.cpp

    Cache/
        PyramidCache.h/.cpp
        ImageCache.h/.cpp
*/

void MW::generateFocusStackFromSelection()
{

    QString src = "MW::generateFocusStackFromSelection";
    if (G::isLogger) G::log(src);

    QStringList paths;
    if (!dm->getSelection(paths) || paths.isEmpty()) {
        QString msg = "No images selected for focus stacking.";
        updateStatus(false, msg, src);
        return;
    }

    QString method = "PMax";
    // QString method = "LegacyPetteri";
    generateFocusStack(paths, method, src);
}

void MW::generateFocusStack(const QStringList paths,
                            const QString method,
                            const QString source)
{
    if (G::isLogger) G::log("MW::generateFocusStack", "paths " + method);
    QString srcFun = "MW::generateFocusStack";

    G::isRunningFocusStack = true;

    bool isLocal = (source == "MW::generateFocusStackFromSelection");

    // Options
    // clean (send all project folders to the trash)
    bool isClean = false;       // send all project folders to the trash
    bool isRedoAlign = true;    //

    // Source images folder (used after pipeline finishes)
    QFileInfo info(paths.first());
    const QString srcFolder = info.absolutePath();

    // --------------------------------------------------------------------
    // Create worker thread + FS pipeline object
    // --------------------------------------------------------------------
    QThread *thread = new QThread(this);
    fsPipeline = new FS();
    fsPipeline->moveToThread(thread);
    // local req'd for lamda
    FS *pipeline = fsPipeline.data();

    // Set input + project root *before* thread runs
    QString projectRoot = srcFolder + "/" + info.completeBaseName() + "_" + method;
    pipeline->setProjectRoot(projectRoot);
    pipeline->setInput(paths);

    // FOCUS STACK CONTROL:
    FS::Options opt;
    opt.method                  = method;
    opt.keepIntermediates       = true;
    opt.useIntermediates        = true;
    opt.useCache                = true;

    opt.enableAlign             = true;
    opt.keepAlign               = true;

    opt.enableFocusMaps         = true;
    opt.previewFocusMaps        = true;
    opt.keepFocusMaps           = true;

    opt.enableDepthMap          = true;
    opt.previewDepthMap         = true;
    opt.keepDepthMap            = true;

    opt.enableBackgroundMask    = true;
    opt.enableBackgroundReplace = true;
    opt.previewBackgroundMask   = true;
    opt.backgroundMethod        = "Depth+Focus";

    opt.enableFusion            = true;
    opt.previewFusion           = true;

    opt.enableArtifactDetect = false;

    opt.enableOpenCL        = true;
    pipeline->setOptions(opt);

    // When the thread starts → run the FS pipeline
    connect(thread, &QThread::started, pipeline, [pipeline, thread]()
    {
        pipeline->run();     // runs synchronously inside worker thread
        QMetaObject::invokeMethod(thread, "quit", Qt::QueuedConnection);
    });

    // Status / progress → UI
    connect(pipeline, &FS::updateStatus,
            this, &MW::updateStatus, Qt::QueuedConnection);

    connect(pipeline, &FS::progress,
            this, [this](int current, int total)
    {
        this->cacheProgressBar->updateUpperProgress(current, total, Qt::darkYellow);
    },
    Qt::QueuedConnection);

    // cleanup when finished
    connect(thread, &QThread::finished, pipeline, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread,   &QObject::deleteLater);

    // When FS finishes (thread quits)
    // We detect success by checking filesystem or pipeline signals (later)
    connect(thread, &QThread::finished, this, [=]()
    {
        // Clear progress
        cacheProgressBar->clearUpperProgress();

        // If aborted


        // Example: pick the last aligned color image as “fusedPath”
        // (temporary until focus/depth/fusion implemented)
        QString alignedDir = projectRoot + "/align";
        QDir ad(alignedDir);
        QStringList files = ad.entryList(QStringList() << "*.png",
                                         QDir::Files,
                                         QDir::Name);

        QString fusedPath;
        // if (!files.isEmpty())
        //     fusedPath = alignedDir + "/" + files.last();   // temporarily treat as result

        if (!fusedPath.isEmpty())
        {
            // Copy metadata from first source using your existing logic
            ExifTool et;
            et.setOverWrite(true);
            et.copyAll(paths.first(), fusedPath);
            et.close();

            // Copy into source folder
            QFileInfo fi(fusedPath);
            QString destPath = srcFolder + "/" + fi.fileName();
            QFile::copy(fusedPath, destPath);

            // update datamodel
            dm->insert(destPath);

            // select fused image
            if (isLocal) sel->select(destPath);
            else folderAndFileSelectionChange(destPath, srcFun);

            waitUntilMetadataLoaded(5000, srcFun);

            // highlight
            setColorClassForRow(dm->currentSfRow, "Red");

            embedThumbnails();
        }
        G::isRunningFocusStack = false;
    });

    // Start
    thread->start();
}
