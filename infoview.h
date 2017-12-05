

#ifndef INFOVIEW_H
#define INFOVIEW_H

#include <QtWidgets>
#include "metadata.h"

class InfoView : public QTableView
{
	Q_OBJECT

public:
    InfoView(QWidget *parent, Metadata *metadata);
    void updateInfo(const QString &imageFullPath);
    void clearInfo();

//    QMap<QString, bool> *okToShow;
    QStandardItemModel *ok;

public slots:
    // rgh context menu req'd??  Need to coppy??
	void showInfoViewMenu(QPoint pt);
	void copyEntry();

protected:

private:
    QStandardItemModel *infoModel;
	QModelIndex selectedEntry;
	QMenu *infoMenu;
	QAction *copyAction;
    Metadata *metadata;

//    void clear();
    void createOkToShow();
    void addEntry(QString &key, QString &value);
    void addTitleEntry(QString title);
    void tweakHeaders();
};

#endif // INFOVIEW_H
