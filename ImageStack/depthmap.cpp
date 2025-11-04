#include "depthmap.h"
#include <QFileInfoList>
#include <QElapsedTimer>

DepthMap::DepthMap(QObject *parent) : QObject(parent) {}

cv::Mat DepthMap::computeFocusMeasure(const cv::Mat &img)
{
    cv::Mat gray, lap, fm;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, gray, cv::Size(3, 3), 0.5);
    cv::Laplacian(gray, lap, CV_32F, 3);
    cv::pow(lap, 2, fm);
    return fm;
}

cv::Mat DepthMap::anisotropicSmooth(const cv::Mat &depth, int iterations, float k)
{
    cv::Mat d = depth.clone();
    for (int i = 0; i < iterations; ++i) {
        cv::Mat gx, gy;
        cv::Sobel(d, gx, CV_32F, 1, 0, 3);
        cv::Sobel(d, gy, CV_32F, 0, 1, 3);
        cv::Mat grad;
        cv::magnitude(gx, gy, grad);
        cv::Mat c;
        cv::exp(- (grad / k).mul(grad / k), c);

        cv::Mat north = d.clone(), south = d.clone(), east = d.clone(), west = d.clone();
        north.rowRange(1, d.rows).copyTo(north.rowRange(0, d.rows - 1));
        south.rowRange(0, d.rows - 1).copyTo(south.rowRange(1, d.rows));
        east.colRange(1, d.cols).copyTo(east.colRange(0, d.cols - 1));
        west.colRange(0, d.cols - 1).copyTo(west.colRange(1, d.cols));

        d += 0.25f * c.mul((north + south + east + west) - 4 * d);
        cv::threshold(d, d, 1.0, 1.0, cv::THRESH_TRUNC);
        cv::threshold(d, d, 0.0, 0.0, cv::THRESH_TOZERO);
    }
    return d;
}

bool DepthMap::generate(const QString &alignedFolderPath, bool smooth)
{
    const QString src = "DepthMap::generate";
    QElapsedTimer timer;
    timer.start();

    QString msg = "Generating depth map.";
    emit updateStatus(false, msg, src);

    // --- Establish project structure -------------------------------------
    QDir alignDir(alignedFolderPath);
    if (!alignDir.exists()) {
        qWarning() << src << "aligned folder not found:" << alignedFolderPath;
        return false;
    }

    QDir projectDir = alignDir;
    projectDir.cdUp();

    const QString projectRoot = projectDir.absolutePath();
    const QDir masksDir(projectDir.filePath("masks"));
    const QDir depthDir(projectDir.filePath("depth"));

    // --- Determine input source ------------------------------------------
    bool useFocusMaps = masksDir.exists() &&
                        !masksDir.entryList(QStringList() << "focus_raw_*.png", QDir::Files).isEmpty();

    QDir inputDir = useFocusMaps ? masksDir : alignDir;
    QStringList filters = useFocusMaps
                              ? QStringList{"focus_raw_*.png"}
                              : QStringList{"*.tif", "*.tiff", "*.jpg", "*.png"};

    QFileInfoList files = inputDir.entryInfoList(filters, QDir::Files, QDir::Name);
    int n = files.size();

    if (n < 2) {
        qWarning() << src << "need at least two input images to compute depth map.";
        return false;
    }

    qDebug().noquote() << QString("%1 — using %2 (%3 files)")
                              .arg(src)
                              .arg(useFocusMaps ? "FocusMeasure results (/masks)" : "aligned images (/aligned)")
                              .arg(n);

    // --- Load input stack -------------------------------------------------
    std::vector<cv::Mat> focusMaps;
    focusMaps.reserve(n);

    cv::Mat first = cv::imread(files[0].absoluteFilePath().toStdString(), cv::IMREAD_UNCHANGED);
    if (first.empty()) {
        qWarning() << src << "failed to load first image:" << files[0].absoluteFilePath();
        return false;
    }

    int h = first.rows;
    int w = first.cols;

    for (int i = 0; i < n; ++i) {
        QString msg = QString("Depth: processing focus map %1 of %2").arg(i).arg(n);
        emit updateStatus(false, msg, src);

        cv::Mat img = cv::imread(files[i].absoluteFilePath().toStdString(),
                                 useFocusMaps ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR);
        if (img.empty()) continue;
        cv::resize(img, img, cv::Size(w, h));

        cv::Mat fm;
        if (useFocusMaps) {
            img.convertTo(fm, CV_32F, 1.0 / 255.0);
        } else {
            fm = computeFocusMeasure(img);
        }
        focusMaps.push_back(fm);
    }

    // --- Winner-take-all depth index -------------------------------------
    cv::Mat depthIdx(h, w, CV_8U, cv::Scalar(0));
    cv::Mat maxVal(h, w, CV_32F, cv::Scalar(-1.0f));

    for (int i = 0; i < n; ++i) {
        const cv::Mat &fm = focusMaps[i];
        for (int y = 0; y < h; ++y) {
            const float *srcp = fm.ptr<float>(y);
            float *maxp = maxVal.ptr<float>(y);
            uchar *dst = depthIdx.ptr<uchar>(y);
            for (int x = 0; x < w; ++x) {
                if (srcp[x] > maxp[x]) {
                    maxp[x] = srcp[x];
                    dst[x] = static_cast<uchar>(i);
                }
            }
        }
    }

    // --- Normalize & smooth ----------------------------------------------
    cv::Mat depthFloat;
    depthIdx.convertTo(depthFloat, CV_32F, 1.0f / (n - 1));
    if (smooth)
        depthFloat = anisotropicSmooth(depthFloat);

    // --- Save outputs -----------------------------------------------------
    QDir().mkpath(depthDir.absolutePath());
    const QString idxPath   = depthDir.filePath("depth_idx.png");
    const QString floatPath = depthDir.filePath("depth_float.png");
    const QString previewPath = depthDir.filePath("depth_preview.png");

    cv::imwrite(idxPath.toStdString(), depthIdx);
    cv::imwrite(floatPath.toStdString(), depthFloat * 255.0f);

    // --- Create visual depth preview -------------------------------------
    try {
        cv::Mat colorMap;
        cv::Mat depth8U;
        depthFloat.convertTo(depth8U, CV_8U, 255.0);
        cv::applyColorMap(depth8U, colorMap, cv::COLORMAP_JET);

        // Load a background for overlay — prefer first aligned image
        QStringList imgFilters{"*.tif", "*.tiff", "*.jpg", "*.png"};
        QStringList alignedFiles = alignDir.entryList(imgFilters, QDir::Files, QDir::Name);
        cv::Mat base = alignedFiles.isEmpty()
                           ? colorMap.clone()
                           : cv::imread(alignDir.filePath(alignedFiles.first()).toStdString(), cv::IMREAD_COLOR);

        if (!base.empty()) {
            cv::resize(base, base, cv::Size(w, h));
            cv::Mat preview;
            cv::addWeighted(base, 0.6, colorMap, 0.4, 0.0, preview);
            cv::imwrite(previewPath.toStdString(), preview);
        } else {
            cv::imwrite(previewPath.toStdString(), colorMap);
        }

        qDebug().noquote() << QString("%1 — preview saved to %2").arg(src).arg(previewPath);
    } catch (const std::exception &e) {
        qWarning() << src << "preview generation failed:" << e.what();
    }

    qDebug().noquote() << QString("%1 — depth maps written to %2 (%.2f s)")
                              .arg(src)
                              .arg(depthDir.absolutePath())
                              .arg(timer.elapsed() / 1000.0);
    return true;
}


// #include "depthmap.h"
// #include <QFileInfoList>
// #include <QElapsedTimer>

// DepthMap::DepthMap(QObject *parent) : QObject(parent) {}

// cv::Mat DepthMap::computeFocusMeasure(const cv::Mat &img)
// {
//     cv::Mat gray, lap, fm;
//     cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
//     cv::GaussianBlur(gray, gray, cv::Size(3, 3), 0.5);
//     cv::Laplacian(gray, lap, CV_32F, 3);
//     cv::pow(lap, 2, fm);
//     return fm;
// }

// cv::Mat DepthMap::anisotropicSmooth(const cv::Mat &depth, int iterations, float k)
// {
//     cv::Mat d = depth.clone();
//     for (int i = 0; i < iterations; ++i) {
//         cv::Mat gx, gy;
//         cv::Sobel(d, gx, CV_32F, 1, 0, 3);
//         cv::Sobel(d, gy, CV_32F, 0, 1, 3);
//         cv::Mat grad;
//         cv::magnitude(gx, gy, grad);
//         cv::Mat c;
//         cv::exp(- (grad / k).mul(grad / k), c);

//         cv::Mat north = d.clone(), south = d.clone(), east = d.clone(), west = d.clone();
//         north.rowRange(1, d.rows).copyTo(north.rowRange(0, d.rows - 1));
//         south.rowRange(0, d.rows - 1).copyTo(south.rowRange(1, d.rows));
//         east.colRange(1, d.cols).copyTo(east.colRange(0, d.cols - 1));
//         west.colRange(0, d.cols - 1).copyTo(west.colRange(1, d.cols));

//         d += 0.25f * c.mul((north + south + east + west) - 4 * d);
//         cv::threshold(d, d, 1.0, 1.0, cv::THRESH_TRUNC);
//         cv::threshold(d, d, 0.0, 0.0, cv::THRESH_TOZERO);
//     }
//     return d;
// }

// bool DepthMap::generate(const QString &alignedFolderPath, bool smooth)
// {
//     const QString src = "DepthMap::generate";
//     QElapsedTimer timer;
//     timer.start();

//     // --- Establish project structure -------------------------------------
//     QDir alignDir(alignedFolderPath);
//     if (!alignDir.exists()) {
//         qWarning() << src << "aligned folder not found:" << alignedFolderPath;
//         return false;
//     }

//     QDir projectDir = alignDir;
//     projectDir.cdUp();
//     const QString projectRoot = projectDir.absolutePath();
//     const QDir depthFocusDir(projectDir.filePath("depth/focus"));
//     const QDir depthDir(projectDir.filePath("depth"));

//     // --- Determine input source ------------------------------------------
//     bool useFocusMaps = depthFocusDir.exists()
//                         && !depthFocusDir.entryList(QStringList() << "focus_raw_*.png", QDir::Files).isEmpty();

//     QDir inputDir = useFocusMaps ? depthFocusDir : alignDir;
//     QStringList filters = useFocusMaps
//                               ? QStringList{"focus_raw_*.png"}
//                               : QStringList{"*.tif", "*.tiff", "*.jpg", "*.png"};

//     QFileInfoList files = inputDir.entryInfoList(filters, QDir::Files, QDir::Name);
//     int n = files.size();

//     if (n < 2) {
//         qWarning() << src << "need at least two input images to compute depth map.";
//         return false;
//     }

//     qDebug().noquote() << QString("%1 — using %2 (%3 files)")
//                               .arg(src)
//                               .arg(useFocusMaps ? "precomputed focus maps" : "aligned images")
//                               .arg(n);

//     // --- Load input stack -------------------------------------------------
//     std::vector<cv::Mat> focusMaps;
//     focusMaps.reserve(n);

//     cv::Mat first = cv::imread(files[0].absoluteFilePath().toStdString(), cv::IMREAD_UNCHANGED);
//     if (first.empty()) {
//         qWarning() << src << "failed to load first image:" << files[0].absoluteFilePath();
//         return false;
//     }

//     int h = first.rows;
//     int w = first.cols;

//     for (int i = 0; i < n; ++i) {
//         cv::Mat img = cv::imread(files[i].absoluteFilePath().toStdString(),
//                                  useFocusMaps ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR);
//         if (img.empty()) continue;
//         cv::resize(img, img, cv::Size(w, h));

//         cv::Mat fm;
//         if (useFocusMaps) {
//             img.convertTo(fm, CV_32F, 1.0 / 255.0);
//         } else {
//             fm = computeFocusMeasure(img);
//         }
//         focusMaps.push_back(fm);
//     }

//     // --- Winner-take-all depth index -------------------------------------
//     cv::Mat depthIdx(h, w, CV_8U, cv::Scalar(0));
//     cv::Mat maxVal(h, w, CV_32F, cv::Scalar(-1.0f));

//     for (int i = 0; i < n; ++i) {
//         const cv::Mat &fm = focusMaps[i];
//         for (int y = 0; y < h; ++y) {
//             const float *src = fm.ptr<float>(y);
//             float *maxp = maxVal.ptr<float>(y);
//             uchar *dst = depthIdx.ptr<uchar>(y);
//             for (int x = 0; x < w; ++x) {
//                 if (src[x] > maxp[x]) {
//                     maxp[x] = src[x];
//                     dst[x] = static_cast<uchar>(i);
//                 }
//             }
//         }
//     }

//     // --- Normalize & smooth ----------------------------------------------
//     cv::Mat depthFloat;
//     depthIdx.convertTo(depthFloat, CV_32F, 1.0f / (n - 1));
//     if (smooth)
//         depthFloat = anisotropicSmooth(depthFloat);

//     // --- Save outputs -----------------------------------------------------
//     QDir().mkpath(depthDir.absolutePath());
//     const QString idxPath = depthDir.filePath("depth_idx.png");
//     const QString floatPath = depthDir.filePath("depth_float.png");

//     cv::imwrite(idxPath.toStdString(), depthIdx);
//     cv::imwrite(floatPath.toStdString(), depthFloat * 255.0f);

//     qDebug().noquote() << QString("%1 — depth maps written to %2 (%.2f s)")
//                               .arg(src)
//                               .arg(depthDir.absolutePath())
//                               .arg(timer.elapsed() / 1000.0);
//     return true;
// }
