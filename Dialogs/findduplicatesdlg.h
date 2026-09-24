#ifndef FINDDUPLICATESDLG_H
#define FINDDUPLICATESDLG_H

#include <QtWidgets>
#include <QDialog>
#include "Datamodel/datamodel.h"
#include "Metadata/metadata.h"
#include "Image/thumb.h"
#include "Image/pixmap.h"

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

class DragToList : public QListWidget
{
/*
    Include / exclude folder list. Paths are dropped in (FSTree, Finder, Explorer),
    added with the Add buttons, or typed: every item is editable, and double-clicking
    empty space appends a new one. Each item's checkbox = include its subfolder tree.
    See the implementation header in the .cpp.
*/
    Q_OBJECT

public:
    explicit DragToList(QWidget *parent = nullptr);
    QStringList list;
    bool addPath(const QString &path);
    void appendNew();
    bool includesSubfolders(int row) const;

signals:
    void pathsChanged();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void showEvent(QShowEvent *event) override;

protected slots:
    void closeEditor(QWidget *editor, QAbstractItemDelegate::EndEditHint hint) override;

private slots:
    void onItemChanged(QListWidgetItem *item);

private:
    static QString normalizePath(QString path);
    static QListWidgetItem *newPathItem(const QString &path);
    void markValidity(QListWidgetItem *item);
    bool removeEmptyAndDuplicates();
};

namespace Ui {
class FindDuplicatesDlg;
}

class FindDuplicatesDlg : public QDialog
{
    Q_OBJECT

public:
    explicit FindDuplicatesDlg(QWidget *parent, DataModel *dm);
    ~FindDuplicatesDlg() override;

protected:
    void resizeEvent(QResizeEvent*) override;
    void closeEvent(QCloseEvent *event) override;
    void changeEvent(QEvent *event) override;

public slots:
    void setImageFromVideoFrame(QString path, QImage image, QString source);

private slots:
    void on_samePixelsCB_clicked();
    void on_compareBtn_clicked();
    void on_prevToolBtn_clicked();
    void on_nextToolBtn_clicked();
    void on_tv_clicked(const QModelIndex &index);
    void on_updateDupsAndQuitBtn_clicked();
    void on_closeBtn_clicked();
    void on_abortBtn_clicked();
    void on_helpBtn_clicked();
    void on_deltaThresholdToolBtn_clicked();
    void on_clrFoldersBtn_clicked();
    void on_addIncludeBtn_clicked();
    void on_addExcludeBtn_clicked();
    void on_toggleTvHideChecked_clicked();
    void buildBList();
    void on_tv_doubleClicked(const QModelIndex &index);

    void on_reportBtn_clicked();

private:
    Ui::FindDuplicatesDlg *ui;
    DataModel *dm;
    /* Owned by the dialog, not MW's instance: the window is modeless, so MW may be
       using its own Metadata (folder loads) while a search runs here. */
    Metadata *metadata;
    Pixmap *pixmap;
    FrameDecoder *frameDecoder;

    struct B {
        QString fPath;
        QString name;
        QString type;
        QString createdDate;
        QString aspect;
        QString duration;
        QImage im;
    };

    // replaced by QHash<int, QList<M>> matches
    struct R {
        bool sameName;
        bool sameType;
        bool sameCreationDate;
        bool sameAspect;
        bool sameDuration;
        int deltaPixels;
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
    /* Candidate (A) snapshot, taken from the selection when the window opens. The
       window is modeless and FSTree drags load folders, so the datamodel can change
       under it; the candidates must not. Index 'a' == candidate table model row. */
    struct A {
        QString path;
        QString name;
        QString created;
        double aspect;
        bool video;
        QString duration;
        QImage icon;            // DecorationRole thumbnail, for the table + pixel compare
    };
    QList<A> aItems;
    int candidatesInDataModel() const;
    void updateApplyState();
    /* True if any candidate (selected image) is a video. When false, videos are
       excluded from the targets too (no point comparing against video duplicates
       when there are no video candidates). */
    bool candidatesHaveVideo() const;
    QStringList chooseFolders(const QString &title);
    void addFolders(DragToList *list, const QString &title);
    void getPreview(QString fPath, QImage &image, QString source);
    void showPreview(QString path, QImage image, QString source);
    void fit(QPixmap &pm, QLabel *label);
    // void buildBItemsList(QStringList &dPaths);
    void getMetadataBItems();
    void reportRGB(QImage &im);
    int compareRGB(QImage &imA, QImage &imB);
    /* Normalize a target (B) thumbnail to the candidate decoration icon size
       (256px long side) so pixel comparison aligns A and B. */
    QImage normalizeBThumb(const QImage &im) const;
    double compareImagesHues(QImage &imA, QImage &imB);
    void setupModel();
    QString currentMatchString(int a, int b);
    void clear();
    void showImageComparisonStuff(int a, int b, QString bPath);
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
    bool sameFilePath(int a, int b);
    bool sameFileName(int a, int b);
    bool sameFileType(int a, int b);
    bool sameCreationDate(int a, int b/*, ImageMetadata *m*/);
    bool sameAspect(int a, int b/*, ImageMetadata *m*/);
    bool sameDuration(int a, int b);

    int matchCount = 0;

    void progressMsg(QString msg);
    bool okToHide = true;

    bool abort;
    bool isRunning = false;
    bool closePending = false;  // close requested mid-search: close once the run unwinds
    bool isDebug = false;

    /* Count of queued video thumbnails (B targets) whose first frame has not yet
       arrived from FrameDecoder. getMetadataBItems waits for this to reach 0
       before comparison, since video frames are delivered asynchronously. */
    int pendingVideoFrames = 0;
};

#endif // FINDDUPLICATESDLG_H
