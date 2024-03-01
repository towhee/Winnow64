

#ifndef INFOVIEW_H
#define INFOVIEW_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "Datamodel/buildfilters.h"
#include "Views/iconview.h"
#include "Utilities/utilities.h"

#include "Main/global.h"

class InfoView : public QTreeView
{
	Q_OBJECT

public:
    InfoView(QWidget *parent, DataModel *dm, Metadata *metadata, IconView *thumbView,
             Filters *filters, BuildFilters *buildFilters);
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
    bool ignoreDataChange;

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
        PreviewDimensionsRow,
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
        GPSCoordRow,
        KeywordRow,
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
    void setValue(QModelIndex dmIdx, QVariant value, int instance, QString src,
                    int role = Qt::EditRole, int align = Qt::AlignLeft);
    void dataEdited();
    void updateFilter(BuildFilters::Category category, BuildFilters::AfterAction afterAction);
    void filterChange(QString source);
//    void addToImageCache(ImageMetadata m);
    void setCurrentPosition(QString fPath, QString src);

public slots:
    void showOrHide();
	void showInfoViewMenu(QPoint pt);
	void copyEntry();
    void setColumn0Width();

private slots:
    void dataChanged(const QModelIndex &idx1, const QModelIndex&, const QVector<int> &roles) override;

protected:
    void mousePressEvent(QMouseEvent *event) override;

private:
    void setupOk();

    QModelIndex selectedEntry;
	QMenu *infoMenu;
	QAction *copyAction;
    DataModel *dm;
    Metadata *metadata;
    IconView *thumbView;
    Filters *filters;
    BuildFilters *buildFilters;
    QString fPath;
    QStringList editFields;

    QString srcFunction;
};

#endif // INFOVIEW_H
