#include <QDirModel>
#include "dircompleter.h"

DirCompleter::DirCompleter(QObject *parent) : QCompleter(parent)
{
    QDirModel *model = new QDirModel;
    model->setFilter(QDir::AllDirs | QDir::NoDotAndDotDot);
    model->setLazyChildCount(true);
    setModel(model);
}

QString DirCompleter::pathFromIndex(const QModelIndex &index) const
{
    return QCompleter::pathFromIndex(index) + "/";
}

QStringList DirCompleter::splitPath(const QString &path) const
{
    if (path.startsWith("~")) {
        return QCompleter::splitPath(QString(path).replace(0, 1, QDir::homePath()));
    }
    return QCompleter::splitPath(path);
}
