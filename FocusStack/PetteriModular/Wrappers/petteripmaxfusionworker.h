#ifndef PETTERIPMAXFUSIONWORKER_H
#define PETTERIPMAXFUSIONWORKER_H

#include <QObject>
#include <QStringList>
#include <functional>

namespace FStack {
class Logger;
class Worker;
class Task_LoadImg;
class Task_Wavelet;
class Task_Wavelet_OpenCL;
class Task_Merge;
class Task_Reassign_Map;
class Task_Reassign;
}

class PetteriPMaxFusionWorker : public QObject
{
    Q_OBJECT
public:
    explicit PetteriPMaxFusionWorker(QObject *parent = nullptr);

    void setInput(const QStringList &alignedColorPaths,
                  const QStringList &alignedGrayPaths,
                  const QString &fusionOutputPath,
                  bool useOpenCL = true,
                  int consistency = 1);

    void setStepCallback(const std::function<void()> &fn) { m_stepFn = fn; }

    bool run(const std::function<bool()> &abortFn);

signals:
    void updateStatus(bool isBase, const QString &msg, const QString &src);
    void progress(int current, int total);

private:
    QStringList m_alignedColorPaths;
    QStringList m_alignedGrayPaths;
    QString     m_fusionOutputPath;
    bool        m_useOpenCL { true };
    int         m_consistency { 1 };

    std::function<void()> m_stepFn;
    inline void step() { if (m_stepFn) m_stepFn(); }
};

#endif // PETTERIPMAXFUSIONWORKER_H
