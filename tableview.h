#ifndef TABLEVIEW_H
#define TABLEVIEW_H

#include <QtWidgets>
#include "global.h"
#include "datamodel.h"
#include "thumbview.h"

class TableView : public QTableView
{
    Q_OBJECT

public:
    TableView(DataModel *dm, ThumbView *thumbView);

protected:
    void mouseDoubleClickEvent(QMouseEvent *event);

private:
    ThumbView *thumbView;

private slots:
    void delaySelectCurrentThumb();

signals:
    void displayLoupe();
};

#include <QStyledItemDelegate>

class ApertureItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ApertureItemDelegate(QObject* parent = 0);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const;
};

class ExposureTimeItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit ExposureTimeItemDelegate(QObject* parent = 0);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const;
};

class FocalLengthItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FocalLengthItemDelegate(QObject* parent = 0);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const;
};

class FileSizeItemDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    explicit FileSizeItemDelegate(QObject* parent = 0);
    virtual QString displayText(const QVariant & value, const QLocale & locale) const;
};

#endif // TABLEVIEW_H
