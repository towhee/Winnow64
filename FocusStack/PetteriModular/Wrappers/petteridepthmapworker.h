#ifndef PETTERIDEPTHMAPWORKER_H
#define PETTERIDEPTHMAPWORKER_H

#include <QObject>
#include <QStringList>
#include <functional>

namespace FStack {
class Logger;
class Worker;
class Task_LoadImg;
class Task_Depthmap;
class Task_Depthmap_Inpaint;
}

class PetteriDepthMapWorker : public QObject
{
    Q_OBJECT
public:
    explicit PetteriDepthMapWorker(QObject *parent = nullptr);

    void setInput(const QStringList &focusMapPaths,
                  const QString &depthRawPath,
                  const QString &depthFilteredPath,
                  const QString &depthDir,
                  bool saveSteps = false,
                  int smoothXY   = 32,
                  int smoothZ    = 64,
                  int haloRadius = 30);

    void setStepCallback(const std::function<void()> &fn) { m_stepFn = fn; }

    bool run(const std::function<bool()> &abortFn);

signals:
    void updateStatus(bool isBase, const QString &msg, const QString &src);
    void progress(int current, int total);

private:
    QStringList m_focusMapPaths;
    QString     m_depthRawPath;
    QString     m_depthFilteredPath;
    QString     m_depthDir;
    bool        m_saveSteps { false };
    int         m_smoothXY  { 32 };
    int         m_smoothZ   { 64 };
    int         m_haloRadius{ 30 };

    std::function<void()> m_stepFn;
    inline void step() { if (m_stepFn) m_stepFn(); }
};

#endif // PETTERIDEPTHMAPWORKER_H
