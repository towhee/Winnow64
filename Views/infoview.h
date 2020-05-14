

#ifndef INFOVIEW_H
#define INFOVIEW_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"

#include "Main/global.h"

class InfoView : public QTreeView
{
	Q_OBJECT

public:
    InfoView(QWidget *parent, DataModel *dm, Metadata *metadata);
    void updateInfo(const int &row);
    void clearInfo();

    // datamodel for metadata to show or hide
    QStandardItemModel *ok;

    // root category indexes
    QModelIndex fileInfoIdx;
    QModelIndex imageInfoIdx;
    QModelIndex tagInfoIdx;
    QModelIndex statusInfoIdx;

    QAction *copyInfoAction;
    bool isNewImageDataChange;

    enum categories {
        fileInfoCat,
        imageInfoCat,
        tagInfoCat,
        statusInfoCat,
        categoriesEnd
    };

    enum fileInfoRows {
        FolderRow,
        FileNameRow,
        LocationRow,
        SizeRow,
        CreatedRow,
        ModifiedRow,
        DimensionsRow,
        MegaPixelsRow,
        fileInfoRowsEnd // insert additional items before this
    };

    enum imageInfoRows {
        ModelRow,
        LensRow,
        ShutterSpeedRow,
        ApertureRow,
        ISORow,
        ExposureCompensationRow,
        FocalLengthRow,
        imageInfoRowsEnd    // insert additional items before this
    };

    enum tagInfoRows {
        TitleRow,
        CreatorRow,
        CopyrightRow,
        EmailRow,
        UrlRow,
        tagInfoRowsEnd
    };

    enum statusInfoRows {
        PositionRow,
        ZoomRow,
        PickedRow,
        CacheRow,
        MonitorRow,
        statusInfoRowsEnd   // insert additional items before this
    };


public slots:
    void showOrHide();
	void showInfoViewMenu(QPoint pt);
	void copyEntry();
    void setColumn0Width();

private slots:

protected:
    void mousePressEvent(QMouseEvent *event);

private:
    void setupOk();

    QModelIndex selectedEntry;
	QMenu *infoMenu;
	QAction *copyAction;
    DataModel *dm;
    Metadata *metadata;
    QString fPath;
};

#endif // INFOVIEW_H
