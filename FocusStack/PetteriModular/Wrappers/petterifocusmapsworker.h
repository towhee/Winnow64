#ifndef PETTERIFOCUSMAPSWORKER_H
#define PETTERIFOCUSMAPSWORKER_H

#include <QObject>
#include <QStringList>
#include <functional>

namespace FStack {
class Logger;
class Worker;
class Task_LoadImg;
class Task_FocusMeasure;
}

class PetteriFocusMapsWorker : public QObject
{
    Q_OBJECT
public:
    explicit PetteriFocusMapsWorker(QObject *parent = nullptr);

    void setInput(const QStringList &alignedPaths,
                  const QStringList &focusMapPaths,
                  const QString &focusDir);

    void setStepCallback(const std::function<void()> &fn) { m_stepFn = fn; }
    void setIs16bit(bool v) { m_is16bit = v; }
    bool run(const std::function<bool()> &abortFn);

signals:
    void updateStatus(bool isBase, const QString &msg, const QString &src);
    void progress(int current, int total);

private:
    QStringList m_alignedPaths;
    QStringList m_focusMapPaths;
    QString     m_focusDir;

    std::function<void()> m_stepFn;
    inline void step() { if (m_stepFn) m_stepFn(); }

    bool m_is16bit = false;
};

#endif // PETTERIFOCUSMAPSWORKER_H
