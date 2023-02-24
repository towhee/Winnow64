#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <QtWidgets>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Views/iconview.h"
#include "Utilities/utilities.h"

class TableView : public QTableView
{
    Q_OBJECT

public:
    TableView(DataModel *dm);
    void scrollToCurrent();
    void scrollToRow(int row, QString source);
    bool isRowVisible(int row);
    bool scrollWhenReady;
    QStandardItemModel *ok;

    int firstVisibleRow;
    int midVisibleRow;
    int lastVisibleRow;
    bool isCurrentVisible;

public slots:
    void showOrHide();
    QModelIndex pageUpIndex();
    QModelIndex pageDownIndex();
    void selectPageUp();    // rgh req'd?
    void selectPageDown();  // rgh req'd?
    void updateVisible();
    void resizeColumns();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
//    int defaultCulumnWidth(int column) const override;

private:
    IconView *thumbView;
    DataModel *dm;
    void createOkToShow();
    int defaultCulumnWidth(int column);

private slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected) override;

signals:
    void displayLoupe();
    void selectionChange(bool, QString, QString);
    void fileSelectionChange(QModelIndex idx,
                             QModelIndex idx2 = QModelIndex(),
                             bool clearSelection = false,
                             QString src = "TableView::fileSelectionChange") ;
};

#include <QStyledItemDelegate>

//class FileItemDelegate : public QStyledItemDelegate {
//    Q_OBJECT
//public:
//    explicit FileItemDelegate(QObject* parent = 0);
//    virtual QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
//};

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
