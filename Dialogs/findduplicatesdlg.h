#ifndef FINDDUPLICATESDLG_H
#define FINDDUPLICATESDLG_H

#include <QtWidgets>
#include <QDialog>
#include "Main/global.h"
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Image/autonomousimage.h"
#include "Cache/imagedecoder.h"
#include "ui_helpfindduplicates.h"
#include "ui_helppixeldelta.h"

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

class DragToList : public QListWidget
{
    Q_OBJECT

public:
    explicit DragToList(QWidget *parent = nullptr);
    QStringList list;

signals:
    void dropped();

protected:
    //void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showEvent(QShowEvent *event) override;
};

namespace Ui {
class FindDuplicatesDlg;
}

class FindDuplicatesDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FindDuplicatesDlg(QWidget *parent, DataModel *dm,
                               Metadata *metadata, FrameDecoder *frameDecoder);
    ~FindDuplicatesDlg();

protected:
    void resizeEvent(QResizeEvent*) override;

public slots:
    void setImageFromVideoFrame(QString path, QImage image, QString source);

private slots:
    void on_samePixelsCB_clicked();
    void on_compareBtn_clicked();
    void on_prevToolBtn_clicked();
    void on_nextToolBtn_clicked();
    void on_tv_clicked(const QModelIndex &index);
    void on_updateDupsAndQuitBtn_clicked();
    void on_cancelBtn_clicked();
    void on_abortBtn_clicked();
    void on_helpBtn_clicked();
    void on_deltaThresholdToolBtn_clicked();
    void on_clrFoldersBtn_clicked();
    void on_toggleTvHideChecked_clicked();
    void buildBList();
    void on_tv_doubleClicked(const QModelIndex &index);

    void on_reportBtn_clicked();

private:
    Ui::FindDuplicatesDlg *ui;
    DataModel *dm;
    Metadata *metadata;
    AutonomousImage *autonomousImage;
    ImageDecoder *imageDecoder;
    FrameDecoder *frameDecoder;

    struct B {
        QString fPath;
        QString type;
        QString createdDate;
        QString aspect;
        QString duration;
        QImage im;
    };

    // replaced by QHash<int, QList<M>> matches
    struct R {
        bool sameType;              //
        bool sameCreationDate;      //
        bool sameAspect;            //
        bool sameDuration;          //
        int deltaPixels;         //
        bool match;                 // for reporting only
    };

    struct Matches {
        QString path;
        int deltaPixels;
    };

    QList<B> bItems;
    QHash<int, QList<Matches>> matches;
    QVector<QVector<R>> results;
    int currentMatch;

    // model columns
    enum MC {
        CheckBox,
        Count,
        Delta,
        Thumbnail,
        FileName
    };

    int previewLongSide;
    QSize previewSize;
    QPixmap pA;
    QPixmap pB;

    QStandardItemModel model;
    void getPreview(QString fPath, QImage &image, QString source);
    void showPreview(QString path, QImage image, QString source);
    void fit(QPixmap &pm, QLabel *label);
    // void buildBItemsList(QStringList &dPaths);
    void getMetadataBItems();
    int reportRGB(QImage &im);
    int compareRGB(QImage &imA, QImage &imB);
    double compareImagesHues(QImage &imA, QImage &imB);
    void setupModel();
    QString currentMatchString(int a, int b);
    void clear();
    void showImageComparisonStuff(int a, QString bPath);
    void initializeResultsVector();
    void pixelCompare();
    void findMatches();
    void buildResults();
    //int updateResults();
    void reportFindMatch(int a, int b);
    void reportbItems();
    void reportMatches();
    void reportResults();
    void reportAspects();
    bool sameFileType(int a, int b);
    bool sameCreationDate(int a, int b/*, ImageMetadata *m*/);
    bool sameAspect(int a, int b/*, ImageMetadata *m*/);
    bool sameDuration(int a, int b);

    int matchCount = 0;

    void progressMsg(QString msg);
    bool okToHide = true;

    bool abort;
    bool isRunning = false;
    bool isDebug = false;
};

#endif // FINDDUPLICATESDLG_H
