#include "depthmap.h"
#include <QFileInfoList>
#include <QElapsedTimer>
#include <algorithm>    // std::clamp

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
        east .colRange(1, d.cols).copyTo(east .colRange(0, d.cols - 1));
        west .colRange(0, d.cols - 1).copyTo(west .colRange(1, d.cols));

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

    emit updateStatus(false, "Generating depth map.", src);

    // --- Project structure ------------------------------------------------
    QDir alignDir(alignedFolderPath);
    if (!alignDir.exists()) {
        qWarning() << src << "aligned folder not found:" << alignedFolderPath;
        return false;
    }

    QDir projectDir = alignDir;
    projectDir.cdUp();

    const QDir masksDir(projectDir.filePath("masks"));
    const QDir depthDir(projectDir.filePath("depth"));

    // --- Input source -----------------------------------------------------
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

    // --- Load stack or focus maps ----------------------------------------
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
        emit updateStatus(false,
                          QString("Depth: processing focus map %1 of %2").arg(i + 1).arg(n),
                          src);

        cv::Mat img = cv::imread(files[i].absoluteFilePath().toStdString(),
                                 useFocusMaps ? cv::IMREAD_GRAYSCALE : cv::IMREAD_COLOR);
        if (img.empty()) continue;
        if (img.rows != h || img.cols != w)
            cv::resize(img, img, cv::Size(w, h));

        cv::Mat fm = useFocusMaps ? cv::Mat() : computeFocusMeasure(img);
        if (useFocusMaps) img.convertTo(fm, CV_32F, 1.0 / 255.0);
        focusMaps.push_back(fm);
    }

    // --- Winner-take-all --------------------------------------------------
    cv::Mat depthIdx(h, w, CV_8U, cv::Scalar(0));
    cv::Mat maxVal(h, w, CV_32F, cv::Scalar(-1.0f));

    for (int i = 0; i < n; ++i) {
        const cv::Mat &fm = focusMaps[i];
        for (int y = 0; y < h; ++y) {
            const float *srcp = fm.ptr<float>(y);
            float *maxp = maxVal.ptr<float>(y);
            uchar *dst = depthIdx.ptr<uchar>(y);
            for (int x = 0; x < w; ++x) {
                float v = srcp[x];
                if (v > maxp[x]) {
                    maxp[x] = v;
                    dst[x] = static_cast<uchar>(i);
                }
            }
        }
    }

    // --- Normalize (for file) & optional smoothing -----------------------
    cv::Mat depthFloat;
    depthIdx.convertTo(depthFloat, CV_32F, 1.0f / (n - 1));
    if (smooth) depthFloat = anisotropicSmooth(depthFloat);

    // --- Save core outputs ------------------------------------------------
    QDir().mkpath(depthDir.absolutePath());
    const QString idxPath     = depthDir.filePath("depth_idx.png");
    const QString floatPath   = depthDir.filePath("depth_float.png");
    const QString previewPath = depthDir.filePath("depth_preview.png");

    cv::imwrite(idxPath.toStdString(), depthIdx);
    cv::imwrite(floatPath.toStdString(), depthFloat * 255.0f);

    // --- Preview (discrete colors) + Legend (exact match) ----------------
    try {
        // Build Jet palette
        cv::Mat palette(1, 256, CV_8UC1);
        for (int i = 0; i < 256; ++i)
            palette.at<uchar>(0, i) = static_cast<uchar>(i);
        cv::Mat colorRamp;
        cv::applyColorMap(palette, colorRamp, cv::COLORMAP_JET);

        // Colorize from discrete indices (exact legend match)
        cv::Mat colorMap(h, w, CV_8UC3);
        for (int y = 0; y < h; ++y) {
            const uchar *di = depthIdx.ptr<uchar>(y);
            cv::Vec3b *co = colorMap.ptr<cv::Vec3b>(y);
            for (int x = 0; x < w; ++x) {
                float t = static_cast<float>(di[x]) / std::max(1, n - 1);
                int lutIdx = std::clamp(static_cast<int>(t * 255.0f + 0.5f), 0, 255);
                co[x] = colorRamp.at<cv::Vec3b>(0, lutIdx);
            }
        }

        cv::Mat preview = colorMap.clone();

        // Legend sizing + fonts
        const double LEGEND_FONT_SCALE = 1.8; // ~3x larger
        const int    LEGEND_THICKNESS  = 3;
        const int    BOX_WIDTH         = std::max(30, w / n);

        // Measure text for height
        int baseline = 0;
        cv::Size txtSize = cv::getTextSize("99", cv::FONT_HERSHEY_SIMPLEX,
                                           LEGEND_FONT_SCALE, LEGEND_THICKNESS, &baseline);
        const int legendHeight = std::max(60, txtSize.height + baseline + 20);
        const int legendWidth  = BOX_WIDTH * n;

        cv::Mat legend(legendHeight, legendWidth, CV_8UC3, cv::Scalar(30, 30, 30));

        for (int i = 0; i < n; ++i) {
            float t = static_cast<float>(i) / std::max(1, n - 1);
            int lutIdx = std::clamp(static_cast<int>(t * 255.0f + 0.5f), 0, 255);
            cv::Vec3b color = colorRamp.at<cv::Vec3b>(0, lutIdx);

            // swatch
            cv::rectangle(legend,
                          cv::Point(i * BOX_WIDTH, 0),
                          cv::Point((i + 1) * BOX_WIDTH - 1, legendHeight - 1),
                          cv::Scalar(color[0], color[1], color[2]),
                          cv::FILLED);

            // centered label
            std::string label = QString::number(i).toStdString();
            cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX,
                                          LEGEND_FONT_SCALE, LEGEND_THICKNESS, &baseline);
            int tx = i * BOX_WIDTH + (BOX_WIDTH - ts.width) / 2;
            int ty = (legendHeight + ts.height) / 2 - 4;

            double luminance = 0.2126 * color[2] + 0.7152 * color[1] + 0.0722 * color[0];
            cv::Scalar textColor = (luminance < 100) ? cv::Scalar(255,255,255)
                                                     : cv::Scalar(0,0,0);

            cv::putText(legend, label, cv::Point(tx, ty),
                        cv::FONT_HERSHEY_SIMPLEX, LEGEND_FONT_SCALE, textColor,
                        LEGEND_THICKNESS, cv::LINE_AA);
        }

        // Header above legend (scaled up)
        const double HEADER_FONT_SCALE = 1.8;
        const int    HEADER_THICKNESS  = 3;
        int headerBaseline = 0;
        cv::Size headerTS = cv::getTextSize("Depth Index (0 = nearest, N-1 = farthest)",
                                            cv::FONT_HERSHEY_SIMPLEX,
                                            HEADER_FONT_SCALE, HEADER_THICKNESS, &headerBaseline);
        const int headerHeight = std::max(28, headerTS.height + headerBaseline + 14);

        cv::Mat header(headerHeight, w, CV_8UC3, cv::Scalar(20, 20, 20));
        int hx = std::max(20, (w - headerTS.width) / 2);
        int hy = (headerHeight + headerTS.height) / 2 - 4;
        cv::putText(header, "Depth Index (0 = nearest, N-1 = farthest)",
                    cv::Point(hx, hy),
                    cv::FONT_HERSHEY_SIMPLEX, HEADER_FONT_SCALE,
                    cv::Scalar(255, 255, 255), HEADER_THICKNESS, cv::LINE_AA);

        // Combine preview + header + legend
        int totalHeight = preview.rows + header.rows + legend.rows;
        cv::Mat combined(totalHeight, preview.cols, CV_8UC3, cv::Scalar(0,0,0));

        preview.copyTo(combined(cv::Rect(0, 0, preview.cols, preview.rows)));
        header .copyTo(combined(cv::Rect(0, preview.rows, header.cols, header.rows)));

        int xOffset = std::max(0, (preview.cols - legendWidth) / 2);
        legend.copyTo(combined(cv::Rect(xOffset, preview.rows + header.rows,
                                        legendWidth, legendHeight)));

        cv::imwrite(previewPath.toStdString(), combined);
        qDebug().noquote() << QString("%1 — preview + legend written to %2")
                                  .arg(src).arg(previewPath);
    } catch (const std::exception &e) {
        qWarning() << src << "preview generation failed:" << e.what();
    }

    qDebug().noquote() << QString("%1 — depth maps written to %2 (%.2f s)")
                              .arg(src)
                              .arg(depthDir.absolutePath())
                              .arg(timer.elapsed() / 1000.0);
    return true;
}

/*
void DepthMap::convertZereneDepthMap()
{
    // one time convert Zerene depth map to depth_idx_from_zerene and vis version
    cv::Mat img = cv::imread("/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/depth/Zerene_depthmap.PNG", cv::IMREAD_GRAYSCALE);
    int n = 12; // number of focus slices
    cv::Mat norm, idx;
    cv::normalize(img, norm, 0, 255, cv::NORM_MINMAX);
    norm.convertTo(idx, CV_8U, (n - 1) / 255.0);
    // cv::imwrite("/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/depth/depth_idx_from_zerene.png", idx);

    // --- Create Jet-colored depth preview from Zerene depth -------------------
    cv::Mat palette(1, 256, CV_8UC1);
    for (int i = 0; i < 256; ++i)
        palette.at<uchar>(0, i) = static_cast<uchar>(i);
    cv::Mat colorRamp;
    cv::applyColorMap(palette, colorRamp, cv::COLORMAP_JET);

    // Convert indices to colors
    cv::Mat colorMap(img.rows, img.cols, CV_8UC3);
    for (int y = 0; y < img.rows; ++y) {
        const uchar *p = idx.ptr<uchar>(y);
        cv::Vec3b *c = colorMap.ptr<cv::Vec3b>(y);
        for (int x = 0; x < img.cols; ++x) {
            float t = static_cast<float>(p[x]) / std::max(1, n - 1);
            int lutIdx = std::clamp(static_cast<int>(t * 255.0f + 0.5f), 0, 255);
            c[x] = colorRamp.at<cv::Vec3b>(0, lutIdx);
        }
    }

    // --- Legend ---------------------------------------------------------------
    const double LEGEND_FONT_SCALE = 1.8;
    const int    LEGEND_THICKNESS  = 3;
    const int    BOX_WIDTH         = std::max(30, img.cols / n);

    int baseline = 0;
    cv::Size txtSize = cv::getTextSize("99", cv::FONT_HERSHEY_SIMPLEX,
                                       LEGEND_FONT_SCALE, LEGEND_THICKNESS, &baseline);
    const int legendHeight = std::max(60, txtSize.height + baseline + 20);
    const int legendWidth  = BOX_WIDTH * n;
    cv::Mat legend(legendHeight, legendWidth, CV_8UC3, cv::Scalar(30, 30, 30));

    for (int i = 0; i < n; ++i) {
        float t = static_cast<float>(i) / std::max(1, n - 1);
        int lutIdx = std::clamp(static_cast<int>(t * 255.0f + 0.5f), 0, 255);
        cv::Vec3b color = colorRamp.at<cv::Vec3b>(0, lutIdx);

        cv::rectangle(legend,
                      cv::Point(i * BOX_WIDTH, 0),
                      cv::Point((i + 1) * BOX_WIDTH - 1, legendHeight - 1),
                      cv::Scalar(color[0], color[1], color[2]),
                      cv::FILLED);

        std::string label = std::to_string(i);
        cv::Size ts = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX,
                                      LEGEND_FONT_SCALE, LEGEND_THICKNESS, &baseline);
        int tx = i * BOX_WIDTH + (BOX_WIDTH - ts.width) / 2;
        int ty = (legendHeight + ts.height) / 2 - 4;

        double luminance = 0.2126 * color[2] + 0.7152 * color[1] + 0.0722 * color[0];
        cv::Scalar textColor = (luminance < 100) ? cv::Scalar(255,255,255)
                                                 : cv::Scalar(0,0,0);

        cv::putText(legend, label, cv::Point(tx, ty),
                    cv::FONT_HERSHEY_SIMPLEX, LEGEND_FONT_SCALE, textColor,
                    LEGEND_THICKNESS, cv::LINE_AA);
    }

    // --- Header ---------------------------------------------------------------
    const double HEADER_FONT_SCALE = 1.8;
    const int    HEADER_THICKNESS  = 3;
    int headerBaseline = 0;
    cv::Size headerTS = cv::getTextSize("Zerene Depth Index (0 = nearest, N-1 = farthest)",
                                        cv::FONT_HERSHEY_SIMPLEX,
                                        HEADER_FONT_SCALE, HEADER_THICKNESS, &headerBaseline);
    const int headerHeight = std::max(28, headerTS.height + headerBaseline + 14);

    cv::Mat header(headerHeight, img.cols, CV_8UC3, cv::Scalar(20, 20, 20));
    int hx = std::max(20, (img.cols - headerTS.width) / 2);
    int hy = (headerHeight + headerTS.height) / 2 - 4;
    cv::putText(header, "Zerene Depth Index (0 = nearest, N-1 = farthest)",
                cv::Point(hx, hy),
                cv::FONT_HERSHEY_SIMPLEX, HEADER_FONT_SCALE,
                cv::Scalar(255, 255, 255), HEADER_THICKNESS, cv::LINE_AA);

    // --- Combine color map + header + legend ---------------------------------
    int totalHeight = colorMap.rows + header.rows + legend.rows;
    cv::Mat combined(totalHeight, colorMap.cols, CV_8UC3, cv::Scalar(0,0,0));

    colorMap.copyTo(combined(cv::Rect(0, 0, colorMap.cols, colorMap.rows)));
    header.copyTo(combined(cv::Rect(0, colorMap.rows, header.cols, header.rows)));

    int xOffset = std::max(0, (colorMap.cols - legendWidth) / 2);
    legend.copyTo(combined(cv::Rect(xOffset, colorMap.rows + header.rows,
                                    legendWidth, legendHeight)));

    // --- Save -----------------------------------------------------------------
    cv::imwrite("/Users/roryhill/Projects/Stack/Mouse/2025-11-01_0227/depth/depth_preview_from_zerene.png", combined);
    qDebug() << "Written: depth_preview_from_zerene.png";
}
*/
