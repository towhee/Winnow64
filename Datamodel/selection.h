#ifndef SELECTION_H
#define SELECTION_H

#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "Views/iconview.h"
#include "Views/tableview.h"
#include "Main/global.h"

class Selection : public QObject
{
    Q_OBJECT
public:
    Selection(QWidget *parent, DataModel *dm, IconView *thumbView,
              IconView *gridView, TableView *tableView);
    void select(QModelIndex sfIdx, QModelIndex sfIdx2 = QModelIndex());
    void select(int sfRow);
    void select(QString &fPath);
    void currentIndex(QModelIndex sfIdx);
    void currentRow(int sfRow);
    void currentPath(QString &fPath);
    void toggleSelect(QModelIndex sfIdx);
    void next();
    void prev();
    void up();
    void down();
    void first();
    void last();
    void nextPage();
    void prevPage();
    void nextPick();
    void prevPick();
    void all();
    void random();
    QModelIndex nearestSelectedIndex(int sfRow);
    void invert();
    void chkForDeselection(int sfRow);
    bool isSelected(int sfRow);
    void save();
    void recover();

    QModelIndexList selectedRows;

signals:
    void fileSelectionChange(QModelIndex idx,
                        QModelIndex idx2 = QModelIndex(),
                        bool clearSelection = false,
                        QString src = "Selection::currentChanged") ;
//    void setCurrentRow(int row, QString src);
    void loadConcurrent(int sfRow, bool scrollOnly = false);

public slots:
//    void currentChanged(QModelIndex idx, QModelIndex idx2 = QModelIndex());
//    void current(QModelIndex sfIdx);

private:
    DataModel *dm;
    IconView *thumbView;
    IconView *gridView;
    TableView *tableView;
    QItemSelectionModel *sm;

    bool isDebug;
};

#endif // SELECTION_H
