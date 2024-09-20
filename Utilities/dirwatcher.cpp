#include "dirwatcher.h"
#include <QDir>
#include <QDebug>

DirWatcher::DirWatcher(QObject *parent)
    : QObject(parent), delay(1000), watcherThread(new QThread(this)), timer(new QTimer()) {
    // Move the DirWatcher to a separate thread
    this->moveToThread(watcherThread);

    // Connect the timer's timeout signal to the checkDirectory slot
    connect(timer, &QTimer::timeout, this, &DirWatcher::checkDirectory);

    // Start the thread when needed
    watcherThread->start();
}

DirWatcher::~DirWatcher() {
    stopWatching();  // Ensure cleanup when object is destroyed
    watcherThread->quit();
    watcherThread->wait();  // Wait for the thread to finish
    delete timer;
}

void DirWatcher::startWatching(const QString &path, int delay) {
    stopWatching();
    this->directoryPath = path;
    this->delay = delay;

    // Start the timer to check at intervals
    timer->start(delay);
}

void DirWatcher::stopWatching() {
    if (timer->isActive()) {
        timer->stop();
    }
}

void DirWatcher::changeDirectory(const QString &newPath) {
    // Stop the current watcher and change the directory path
    stopWatching();
    this->directoryPath = newPath;
    timer->start(delay);  // Restart the timer for the new path
}

void DirWatcher::checkDirectory() {
    QDir dir(directoryPath);
    if (!dir.exists()) {
        emit folderDeleted(directoryPath);
        stopWatching();
    }
}
