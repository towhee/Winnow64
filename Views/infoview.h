

#ifndef INFOVIEW_H
#define INFOVIEW_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "Views/iconview.h"

#include "Main/global.h"

class InfoView : public QTreeView
{
	Q_OBJECT

public:
    InfoView(QWidget *parent, DataModel *dm, Metadata *metadata, IconView *thumbView);
    void updateInfo(const int &row);
    void clearInfo();
    void refreshLayout();

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
        SelectedRow,
        PickedRow,
        CacheRow,
        MonitorRow,
        statusInfoRowsEnd   // insert additional items before this
    };

signals:
    void dataEdited();

public slots:
    void showOrHide();
	void showInfoViewMenu(QPoint pt);
	void copyEntry();
    void setColumn0Width();

private slots:
    void dataChanged(const QModelIndex &idx1, const QModelIndex, const QVector<int> &roles);

protected:
    void mousePressEvent(QMouseEvent *event);

private:
    void setupOk();

    QModelIndex selectedEntry;
	QMenu *infoMenu;
	QAction *copyAction;
    DataModel *dm;
    Metadata *metadata;
    IconView *thumbView;
    QString fPath;
};

#endif // INFOVIEW_H
