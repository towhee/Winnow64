#ifndef SELECTION_H
#define SELECTION_H

#include <QtWidgets>
#include "Datamodel/datamodel.h"
#include "Views/iconview.h"
#include "Views/tableview.h"

class Selection : public QObject
{
    Q_OBJECT
public:
    Selection(QWidget *parent, DataModel *dm, IconView *thumbView,
              IconView *gridView, TableView *tableView);
    void select(QString &fPath, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void select(int sfRow, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void select(QModelIndex sfIdx,
                Qt::KeyboardModifiers modifiers = Qt::NoModifier,
                QString src = "");
    void select(QModelIndex sfIdx, QModelIndex sfIdx2);
    void updateCurrentIndex(QModelIndex sfIdx);
    void setCurrentIndex(QModelIndex sfIdx, bool clearSelection = true);
    void setCurrentRow(int sfRow);
    void setCurrentPath(QString &fPath);
    void toggleSelect(QModelIndex sfIdx);
    void all();
    void random();
    QModelIndex nearestSelectedIndex(int sfRow);
    void invert();
    void chkForDeselection(int sfRow);
    bool isSelected(int sfRow);
    int startSelectionBlock(int rowInBlock);
    int endSelectionBlock(int rowInBlock);
    void clear();
    int count();
    void save(QString src = "");
    void recover(QString src = "");
    //void report();
    QString diagnostics();

    QModelIndexList selectedRows;
    QModelIndexList selectedIndexes;
    QModelIndexList dmSelectedRows;
    QModelIndexList dmSelectedIndexes;
    QModelIndex dmCurrentIndex;
    QItemSelectionModel *sm;

signals:
    void fileSelectionChange(QModelIndex idx,
                        QModelIndex idx2 = QModelIndex(),
                        bool clearSelection = false,
                        QString src = "Selection::setCurrentIndex");
    void updateChange(int sfRow, bool isCurrent = true, QString src = "");
    void updateStatus(bool, QString, QString);
    void updateCurrent(QModelIndex sfIdx, int instance);
    void updateMissingThumbnails();

public slots:
    void selectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
    void next(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void prev(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void up(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void down(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void first(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void last(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void nextPage(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void prevPage(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void nextPick(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void prevPick(Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void okToSelect(bool isOk);

private:
    QMutex mutex;
    DataModel *dm;
    IconView *thumbView;
    IconView *gridView;
    TableView *tableView;
    QModelIndex shiftAnchorIndex;
    QModelIndex shiftExtendIndex;

    bool ok;

    bool isDebug;
};

#endif // SELECTION_H
