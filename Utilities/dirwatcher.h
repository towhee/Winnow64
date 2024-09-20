#ifndef DIRWATCHER_H
#define DIRWATCHER_H

#include <QObject>
#include <QThread>
#include <QTimer>

class DirWatcher : public QObject {
    Q_OBJECT

public:
    explicit DirWatcher(QObject *parent = nullptr);
    ~DirWatcher() override;

    // Starts watching a directory with a delay in milliseconds
    void startWatching(const QString &path, int delay);

    // Stops the watcher and terminates the thread
    void stopWatching();

    // Changes the directory to watch
    void changeDirectory(const QString &newPath);

signals:
    // Signal emitted when the folder no longer exists
    void folderDeleted(QString path);

private slots:
    // Slot that checks if the directory still exists
    void checkDirectory();

private:
    QString directoryPath;
    QThread *watcherThread;
    QTimer *timer;
    int delay;
};

#endif // DIRWATCHER_H
