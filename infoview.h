

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

    enum infoModelRows {
        FolderRow,
        FileNameRow,
        LocationRow,
        SizeRow,
        DateTimeRow,
        ModifiedRow,
        BlankEntryRow1,
        DimensionsRow,
        MegaPixelsRow,
        ModelRow,
        LensRow,
        ShutterSpeedRow,
        ApertureRow,
        ISORow,
        FocalLengthRow,
        TitleRow,
        CreatorRow,
        CopyrightRow,
        EmailRow,
        UrlRow,
        BlankEntryRow2,
        PositionRow,
        ZoomRow,
        PickedRow,
        AfterLastItem   // insert additional items before this
    };


public slots:
    void showOrHide();
	void showInfoViewMenu(QPoint pt);
	void copyEntry();

private slots:

protected:

private:
	QModelIndex selectedEntry;
	QMenu *infoMenu;
	QAction *copyAction;
    Metadata *metadata;

    void setupOk();
    void tweakHeaders();
};

#endif // INFOVIEW_H
