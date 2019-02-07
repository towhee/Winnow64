#ifndef ASYNCTASK_H
#define ASYNCTASK_H

#include <QRunnable>
#include <QStringList>
#include <QtWidgets>

class ASyncTask : public QRunnable
{
//T    Q_OBJECT
public:
    explicit ASyncTask(QObject *receiver,
                       volatile bool *stopped,
                       const QStringList &sourceFiles)
                     : m_receiver(receiver),
                       m_stopped(stopped),
                       m_sourceFiles(sourceFiles) {}

private:
    void run();

    QObject *m_receiver;
    volatile bool *m_stopped;
    const QStringList m_sourceFiles;
    const QString m_targetType;
};

#endif // ASYNCTASK_H
