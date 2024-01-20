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
#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

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
    explicit VisCmpDlg(QWidget *parent, DataModel *dm, Metadata *metadata);
    ~VisCmpDlg();

protected:
    void resizeEvent(QResizeEvent*) override;

private slots:
    void on_compareBtn_clicked();
    void on_prevToolBtn_clicked();
    void on_nextToolBtn_clicked();
    void on_tv_clicked(const QModelIndex &index);
    void on_updateDupsAndQuitBtn_clicked();
    void on_cancelBtn_clicked();
    void on_matchBtn_clicked();
    void on_abortBtn_clicked();
    void on_helpBtn_clicked();

    void on_clrFoldersBtn_clicked();

    void on_toggleTvHideChecked_clicked();

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
        QString type;
        QString createdDate;
        QString aspect;
        QString duration;
        double delta;
    };

    struct R {
        int bId;
        QString fPath;
        bool sameType;
        bool sameCreationDate;
        bool sameAspect;
        bool sameDuration;
        bool sameMeta;
        double delta;
        int sameMetaId;
        int samePixelsId;
    };

    QList<B> bItems;
    QVector<QVector<R>> results;
    int currentMatch;

    int previewLongSide;
    QSize previewSize;
    QPixmap pA;
    QPixmap pB;

    QStandardItemModel model;
    void preview(QString fPath, QImage &image);
    void buildBItemsList(QStringList &dPaths);
    int reportRGB(QImage &im);
    int compareRGB(QImage &imA, QImage &imB);
    double compareImagesHues(QImage &imA, QImage &imB);
    void setupModel();
    QString currentBString(int b);
    void clear();
    void buildBList();
    void pixelCompare();
    void buildResults();
    int updateResults();
    void reportResults();
    void reportAspects();
    bool sameFileType(int a, int b);
    bool sameCreationDate(int a, int b/*, ImageMetadata *m*/);
    bool sameAspect(int a, int b/*, ImageMetadata *m*/);
    bool sameDuration(int a, int b);

    void progressMsg(QString msg);
    bool okToHide = true;

    bool abort;
    bool isRunning = false;
    bool isDebug = false;
};

#endif // VISCMPDLG_H
