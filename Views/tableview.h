#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "Views/iconview.h"

class TableView : public QTableView
{
    Q_OBJECT

public:
    TableView(QWidget *parent, DataModel *dm);
    QTableView *frozenView = nullptr;
    void scrollToCurrent();
    void scrollToRow(int row, QString source);
    bool isRowVisible(int row);
    bool scrollWhenReady;
    QStandardItemModel *ok;
    QModelIndex shiftAnchorIndex;

    int firstVisibleRow;
    int midVisibleRow;
    int lastVisibleRow;
    int visibleRowCount;
    bool isCurrentVisible;

    bool isColumnVisibleInViewport(int columnIndex);
    QList<int> visibleColumns();

    void test();

public slots:
    void showOrHide();
    QModelIndex pageUpIndex(int fromRow = 0);
    QModelIndex pageDownIndex(int fromRow);
    void updateVisible(QString src);
    void resizeColumns();
    void onHorizontalScrollBarChanged(int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    IconView *thumbView;
    DataModel *dm;
    QMap<int,int>defaultColumnWidth;
    int frozenColumns;
    bool isDebug;
    void createOkToShow();
    void setDefaultColumnWidths();
    void setFrozenModel(QAbstractItemModel *model);
    void updateFrozenViewGeometry();

signals:
    void displayLoupe(QString src);
};

#include <QStyledItemDelegate>

class RowNumberItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit RowNumberItemDelegate(QObject* parent = nullptr);
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class CreatedItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit CreatedItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class PickItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit PickItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class SearchItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit SearchItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class IngestedItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit IngestedItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class VideoItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit VideoItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class DimensionItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit DimensionItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class ApertureItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ApertureItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class ExposureTimeItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ExposureTimeItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class FocalLengthItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FocalLengthItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class ISOItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ISOItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class ExposureCompensationItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ExposureCompensationItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class KeywordsItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit KeywordsItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class FileSizeItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FileSizeItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

class ErrItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ErrItemDelegate(QObject* parent = nullptr);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const override;
};

#endif // TABLEVIEW_H
