#ifndef MANAGEMODELSDLG_H
#define MANAGEMODELSDLG_H

#include <QDialog>
#include "Utilities/modelstore.h"

class QLabel;
class QPushButton;
class QTableWidget;
class QListWidget;

/*
    Help > Manage AI Models. Lists every model in the catalog with its feature, size and
    ModelStore::State, so the user can fetch models before going offline, pull an update,
    or reclaim the ~771 MB the on-demand models can occupy.

    The second list is the orphans -- files in the Models folder that no catalog entry
    claims, left behind when a newer Winnow stopped using a model. They are deliberately
    NOT deleted automatically: a user who downgrades Winnow would then have to re-fetch
    them. Offer, do not act.
*/
class ManageModelsDlg : public QDialog
{
    Q_OBJECT

public:
    explicit ManageModelsDlg(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onDownloadClicked();
    void onDeleteClicked();
    void onDownloadAllClicked();
    void onDeleteOrphansClicked();
    void onRevealClicked();
    void onSelectionChanged();

private:
    ModelStore::Model selectedModel(bool *ok) const;

    QTableWidget *table         = nullptr;
    QLabel       *orphanLabel   = nullptr;
    QListWidget  *orphanList    = nullptr;
    QPushButton  *downloadBtn   = nullptr;
    QPushButton  *deleteBtn     = nullptr;
    QPushButton  *downloadAllBtn = nullptr;
    QPushButton  *deleteOrphansBtn = nullptr;
    QLabel       *footerLabel   = nullptr;
};

#endif // MANAGEMODELSDLG_H
