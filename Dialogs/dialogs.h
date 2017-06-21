

#ifndef DIALOGS_H
#define DIALOGS_H

#include <QtWidgets>
#include "thumbview.h"
#include "imageview.h"

class WsDlg : public QDialog
{
    Q_OBJECT

public:
    WsDlg(QList<QString> *wsList, QWidget *parent = 0);
    ~WsDlg();

signals:
    void deleteWorkspace(int);
    void reassignWorkspace(int);
    void renameWorkspace(int, QString);

private slots:
    void on_deleteBtn_clicked();
    void on_reassignBtn_clicked();
    void on_doneBtn_clicked();
    void on_cb_activated(const QString name);
    void on_cb_currentTextChanged(const QString &name);
    void on_cb_editTextChanged(const QString &name);
    void on_cb_currentIndexChanged(const int idx);
    void on_cb_highlighted(int idx);

private:
    QComboBox *cb;
    bool isNewItem;
    QString currentName;
};


int cpMvFile(bool isCopy, QString &srcFile, QString &srcPath, QString &dstPath, QString &dstDir);

//class CpMvDialog : public QDialog
//{
//    Q_OBJECT

//public slots:
//	void abort();

//public:
//    CpMvDialog(QWidget *parent);
//	void exec(ThumbView *thumbView, QString &destDir, bool pasteInCurrDir);
//	int nfiles;
//	int latestRow;

//private:
//	QLabel *opLabel;
//	QPushButton *cancelButton;
//	QFileInfo *dirInfo;
//	bool abortOp;
//};

class ShortcutsTableView : public QTableView
{
	Q_OBJECT

public:
	ShortcutsTableView();
	void addRow(QString action, QString description, QString shortcut);

public slots:
	void showShortcutsTableMenu(QPoint pt);
	void clearShortcut();

protected:
	void keyPressEvent(QKeyEvent *e);

private:
	QStandardItemModel *keysModel;
	QModelIndex selectedEntry;
	QMenu *shortcutsMenu;
	QAction *clearAction;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
	static int const nZoomRadios = 5;
    SettingsDialog(QWidget *parent);

private slots:
	void pickColor();
	void pickThumbsColor();
	void pickThumbsTextColor();
	void pickStartupDir();
	void pickBgImage();

public slots:
	void abort();
	void saveSettings();

private:
	QRadioButton *fitLargeRadios[nZoomRadios];
	QRadioButton *fitSmallRadios[nZoomRadios];
	QCheckBox *compactLayoutCb;
	QToolButton *backgroundColorButton;
    QToolButton *colThumbButton;
    QToolButton *colThumbTextButton;
	QSpinBox *thumbSpacingSpin;
	QSpinBox *thumbPagesSpin;
	QSpinBox *saveQualitySpin;
	QColor bgColor;
	QColor thumbBgColor;
	QColor thumbTextColor;
	QCheckBox *exitCliCb;
	QCheckBox *wrapListCb;
	QCheckBox *enableAnimCb;
	QCheckBox *enableExifCb;
	QCheckBox *enableThumbExifCb;
	QCheckBox *imageInfoCb;
	QCheckBox *noSmallThumbCb;
	QCheckBox *reverseMouseCb;
	QCheckBox *deleteConfirmCb;
	QSpinBox *slideDelaySpin;
	QCheckBox *slideRandomCb;
	QRadioButton *startupDirRadios[3];
	QLineEdit *startupDirEdit;
	QLineEdit *thumbsBackImageEdit;

	void setButtonBgColor(QColor &color, QToolButton *button);
};

class AppMgmtDialog : public QDialog
{
    Q_OBJECT

public:
    AppMgmtDialog(QWidget *parent);

public slots:
	void ok();

private slots:
	void add();
	void remove();
	void entry();

private:
	QTableView *appsTable;
	QStandardItemModel *appsTableModel;

	void addTableModelItem(QStandardItemModel *model, QString &key, QString &val);
};

class CopyMoveToDialog : public QDialog
{
    Q_OBJECT

public:
    CopyMoveToDialog(QWidget *parent, QString thumbsPath, bool move);
	QString selectedPath;
	bool copyOp;

private slots:
	void copyOrMove();
	void justClose();
	void add();
	void remove();
	void selection(const QItemSelection&, const QItemSelection&);
	void pathDoubleClick(const QModelIndex &idx);

private:
	QTableView *pathsTable;
	QStandardItemModel *pathsTableModel;
	QString currentPath;
	QLabel *destinationLab;

	void savePaths();
};

class ProgressDialog : public QDialog
{
    Q_OBJECT

public slots:
	void abort();

public:
	QLabel *opLabel;
	bool abortOp;
	
    ProgressDialog(QWidget *parent);

private:
	QPushButton *cancelButton;
};

#endif // DIALOGS_H

