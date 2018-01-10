

#ifndef INFOVIEW_H
#define INFOVIEW_H

#include <QtWidgets>
#include "metadata.h"

#include "global.h"

//class InfoDelegate : public QStyledItemDelegate
//{
//    Q_OBJECT

//public:
//    InfoDelegate(QObject *parent);

//    void paint(QPainter *painter, const QStyleOptionViewItem &option,
//               const QModelIndex &index) const;

//};

class InfoView : public QTreeView
{
	Q_OBJECT

public:
    InfoView(QWidget *parent, Metadata *metadata);
    void updateInfo(const QString &imageFullPath);
    void clearInfo();

    // datamodel for metadata to show or hide
    QStandardItemModel *ok;

    // root category indexes
    QModelIndex fileInfoIdx;
    QModelIndex imageInfoIdx;
    QModelIndex tagInfoIdx;
    QModelIndex statusInfoIdx;

    QAction *copyInfoAction;

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
        statusInfoRowsEnd   // insert additional items before this
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

//#ifndef INFOVIEW_H
//#define INFOVIEW_H

//#include <QtWidgets>
//#include "metadata.h"

//class InfoView : public QTableView
//{
//	Q_OBJECT

//public:
//    InfoView(QWidget *parent, Metadata *metadata);
//    void updateInfo(const QString &imageFullPath);
//    void clearInfo();

//    // datamodel for metadata to show or hide
//    QStandardItemModel *ok;

//    enum infoModelRows {
//        FolderRow,
//        FileNameRow,
//        LocationRow,
//        SizeRow,
//        CreatedRow,
//        ModifiedRow,
//        BlankEntryRow1,
//        DimensionsRow,
//        MegaPixelsRow,
//        ModelRow,
//        LensRow,
//        ShutterSpeedRow,
//        ApertureRow,
//        ISORow,
//        FocalLengthRow,
//        TitleRow,
//        CreatorRow,
//        CopyrightRow,
//        EmailRow,
//        UrlRow,
//        BlankEntryRow2,
//        PositionRow,
//        ZoomRow,
//        PickedRow,
//        AfterLastItem   // insert additional items before this
//    };


//public slots:
//    void showOrHide();
//	void showInfoViewMenu(QPoint pt);
//	void copyEntry();

//private slots:

//protected:

//private:
//	QModelIndex selectedEntry;
//	QMenu *infoMenu;
//	QAction *copyAction;
//    Metadata *metadata;

//    void setupOk();
//    void tweakHeaders();
//};

//#endif // INFOVIEW_H
