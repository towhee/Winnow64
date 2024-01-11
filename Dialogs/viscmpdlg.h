#ifndef VISCMPDLG_H
#define VISCMPDLG_H

#include <QtWidgets>
#include <QDialog>
#include "main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Image/autonomousimage.h"
#include "Cache/imagedecoder.h"

class DragToList : public QListWidget
{
    Q_OBJECT

public:
    explicit DragToList(QWidget *parent = nullptr);
    QStringList list;

protected:
    //void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showEvent(QShowEvent *event) override;
};

namespace Ui {
class VisCmpDlg;
}

class VisCmpDlg : public QDialog
{
    Q_OBJECT

public:
    explicit VisCmpDlg(DataModel *dm, Metadata *metadata, QWidget *parent = nullptr);
    ~VisCmpDlg();

private slots:
    void on_compareBtn_clicked();
    void on_prevToolBtn_clicked();
    void on_nextToolBtn_clicked();
    void on_tv_clicked(const QModelIndex &index);

private:
    Ui::VisCmpDlg *ui;
    DataModel *dm;
    Metadata *metadata;
    AutonomousImage *autonomousImage;
    ImageDecoder *imageDecoder;

    struct B {
        int index;
        QString fPath;
        QImage im;
        double delta;
    };

    struct R {
        int index;
        QString fPath;
        double delta;
    };

    QList<B> bItems;
    QVector<QVector<R>> result;

    int previewLongSide;

    QStandardItemModel model;
    void preview(QString fPath, QImage &image);
    void buildBItemsList(QString dPath);
    int reportRGB(QImage &im);
    int compareRGB(QImage &imA, QImage &imB);
    double compareImagesHues(QImage &imA, QImage &imB);
    void setupModel();
};

#endif // VISCMPDLG_H
