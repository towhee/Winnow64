#ifndef FOCUSHALO_H
#define FOCUSHALO_H

#include <QObject>
#include <QImage>
#include <QDir>
#include <QString>
#include <QDebug>

#include <opencv2/opencv.hpp>

/**
 * @brief FocusHalo
 * Removes bright and dark halos around high-contrast edges
 * introduced by focus fusion (especially PMax-type methods).
 */
class FocusHalo : public QObject
{
    Q_OBJECT
public:
    explicit FocusHalo(QObject *parent = nullptr);

    void setOutputDir(const QDir &dir);   // halo diagnostic directory
    void setStrength(float s);            // 0..1
    void setRadius(int r);                // smoothing radius

    /**
     * @brief removeHalos
     * Applies halo suppression to the fused image and saves diagnostics.
     * @param fusedPath Input fused image path
     * @param outputName Name of result (default "halo_reduced.png")
     * @return true if saved successfully
     */
    bool removeHalos(const QString &fusedPath,
                     const QString &outputName = "halo_reduced.png");

signals:
    void updateStatus(bool keepBase, const QString &msg, const QString &src);

private:
    QDir haloDir;
    float strength = 0.7f;
    int radius = 3;

    cv::Mat detectHalos(const cv::Mat &srcGray);
    cv::Mat suppressHalos(const cv::Mat &srcBgr, const cv::Mat &haloMask);
    cv::Mat removeBroadHalo(const cv::Mat& bgr,
                            int haloWidthPx = 12,       // ~10–12 for your case
                            float k = 1.2f,             // clamp > k*local_sigma
                            float edgeThresh = 0.08f);  // edge gate in [0..1]

    void saveDebug(const cv::Mat &mat, const QString &name);
};

#endif // FOCUSHALO_H
