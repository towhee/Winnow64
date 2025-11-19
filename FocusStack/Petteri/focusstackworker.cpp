#include "focusstackworker.h"
#include "FocusStack/Petteri/worker.h"

void FocusStackWorker::process()
{
    using namespace focusstack;

    const QString src = "FocusStackWorker::process";
    emit updateStatus(false, "Preparing FocusStack...", src);

    // --- Build simulated CLI arguments --------------------------------
    // for (const QString &path : m_selection)
    //     args.emplace_back(path.toStdString());

    // --- Convert selection to fPaths and sort them ---
    QStringList sortedPaths = m_selection;
    std::sort(sortedPaths.begin(), sortedPaths.end(),
              [](const QString &a, const QString &b) {
                  return a < b;   // lexicographic path order
              });

    // --- Build CLI args in the sorted order ---
    std::vector<std::string> args;
    args.emplace_back("focus-stack"); // dummy argv[0]
    for (const QString &path : sortedPaths) {
        args.emplace_back(path.toStdString());
        qDebug() << "FocusStackWorker::process" << path;
    }

    QFileInfo fi(sortedPaths.first());
    QString basePath = fi.path() + "/" + fi.completeBaseName();
    QString outputPath = basePath + "_FocusStack.png";
    QString depthmapPath = basePath + "_DepthMap.png";

    int counter = 1;
    while (QFile::exists(outputPath))
        outputPath = QString("%1_FocusStack_%2.png")
                         .arg(basePath)
                         .arg(counter++, 2, 10, QLatin1Char('0'));

    counter = 1;
    while (QFile::exists(depthmapPath))
        depthmapPath = QString("%1_DepthMap_%2.png")
                           .arg(basePath)
                           .arg(counter++, 2, 10, QLatin1Char('0'));

    args.emplace_back("--output=" + outputPath.toStdString());
    // args.emplace_back("--depthmap=" + depthmapPath.toStdString());
    args.emplace_back("--verbose");

    int argc = static_cast<int>(args.size());
    std::vector<const char*> argv(argc);
    for (int i = 0; i < argc; ++i)
        argv[i] = args[i].c_str();

    // --- Run FocusStack -----------------------------------------------
    Options options(argc, argv.data());
    FocusStack stack;
    stack.set_inputs(options.get_filenames());
    stack.set_output(options.get_arg("--output", "output.jpg"));
    stack.set_depthmap(options.get_arg("--depthmap", ""));
    stack.set_verbose(options.has_flag("--verbose"));
    stack.set_consistency(2);
    stack.set_denoise(1.0f);
    stack.set_disable_opencl(false);

    emit updateStatus(false,
                      QString("Running FocusStack on %1 images...")
                          .arg(m_selection.size()),
                      src);

    // Attach our Qt signal bridge before running
    stack.set_progress_callback([this](int done, int total) {
        emit updateProgress(done, total, QColor(Qt::darkYellow));
    });

    // Normal run() call — it will internally apply the callback
    bool ok = stack.run();

    if (ok) {
        emit updateStatus(false,
                          QString("Focus stack complete.\nOutput: %1\nDepthMap: %2")
                              .arg(outputPath, depthmapPath),
                          src);
    } else {
        emit updateStatus(false, "Focus stack failed.", src);
    }

    emit clearProgress();
    emit finished(ok, outputPath, depthmapPath);
}
