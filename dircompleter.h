

#ifndef DIRCOMPLETER_H
#define DIRCOMPLETER_H

#include <QCompleter>

class DirCompleter : public QCompleter
{
	Q_OBJECT
public:
	DirCompleter(QObject *parent = 0);
	QString pathFromIndex(const QModelIndex &index) const;

public slots:
	QStringList splitPath(const QString &path) const;
};

#endif // DIRCOMPLETER_H
