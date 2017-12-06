

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

    // datamodel for metadata to show or hide
    QStandardItemModel *ok;

public slots:
    void showOrHide();
	void showInfoViewMenu(QPoint pt);
	void copyEntry();

private slots:

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
