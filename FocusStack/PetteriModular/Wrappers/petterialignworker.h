#ifndef PETTERIALIGNWORKER_H
#define PETTERIALIGNWORKER_H

#include <QObject>
#include <QStringList>
#include <functional>

namespace FStack {
class Logger;
class Worker;
class Task_LoadImg;
class Task_Grayscale;
class Task_Align;
}

class PetteriAlignWorker : public QObject
{
    Q_OBJECT
public:
    explicit PetteriAlignWorker(QObject *parent = nullptr);

    // srcExt: without dot, e.g. "tif", "png"
    void setInput(const QStringList &sourcePaths,
                  const QStringList &alignedPaths,
                  const QStringList &alignedGrayPaths,
                  const QString &alignDir,
                  const QString &grayDir,
                  const QString &srcExt);

    void setStepCallback(const std::function<void()> &fn) { m_stepFn = fn; }
    void setIs16bit(bool v) { m_is16bit = v; }

    bool run(const std::function<bool()> &abortFn);

    QString msg;  // general use

signals:
    void updateStatus(bool isBase, const QString &msg, const QString &src);
    void progress(int current/*, int total*/);

private:
    QStringList m_sourcePaths;
    QStringList m_alignedPaths;
    QStringList m_alignedGrayPaths;
    QString     m_alignDir;
    QString     m_grayDir;
    QString     m_srcExt;

    std::function<void()> m_stepFn;
    inline void step() { if (m_stepFn) m_stepFn(); }

    bool m_is16bit = false;
};

#endif // PETTERIALIGNWORKER_H
