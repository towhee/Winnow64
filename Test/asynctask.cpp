#include "Test/asynctask.h"

void ASyncTask::run()
{
    foreach (const QString &source, m_sourceFiles) {
        if (*m_stopped)
            return;
        QFileInfo fileInfo(source);
        QFile file;
        file.setFileName(source);
        file.open(QIODevice::ReadOnly);
        file.readAll();
        file.close();
        QString message =
            QObject::tr("Saved '%1'").arg(QDir::toNativeSeparators(source));
        QMetaObject::invokeMethod(m_receiver, "announceProgress",
                Qt::QueuedConnection, Q_ARG(bool, true),
                Q_ARG(QString, message));
    }
}
