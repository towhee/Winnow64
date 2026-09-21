#include "Dialogs/managemodelsdlg.h"
#include "Dialogs/modeldownloaddlg.h"
#include "Main/global.h"

#include <QDesktopServices>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QUrl>
#include <QVBoxLayout>

#ifdef Q_OS_WIN
#include "Utilities/win.h"
#endif

using ModelStore::Model;
using ModelStore::State;

namespace {
constexpr int kColName = 0, kColFeature = 1, kColSize = 2, kColStatus = 3;
constexpr int kRoleModel = Qt::UserRole + 1;
}

ManageModelsDlg::ManageModelsDlg(QWidget *parent) : QDialog(parent)
{
    if (G::isLogger) G::log("ManageModelsDlg::ManageModelsDlg");
    setWindowTitle(tr("Manage AI Models"));

    table = new QTableWidget(this);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels(
        QStringList() << tr("Model") << tr("Used by") << tr("Size") << tr("Status"));
    table->verticalHeader()->hide();
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(kColName, QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(kColFeature, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(kColSize, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(kColStatus, QHeaderView::ResizeToContents);
    connect(table, &QTableWidget::itemSelectionChanged,
            this, &ManageModelsDlg::onSelectionChanged);

    downloadBtn    = new QPushButton(tr("Download"), this);
    deleteBtn      = new QPushButton(tr("Delete"), this);
    downloadAllBtn = new QPushButton(tr("Download All"), this);
    connect(downloadBtn,    &QPushButton::clicked, this, &ManageModelsDlg::onDownloadClicked);
    connect(deleteBtn,      &QPushButton::clicked, this, &ManageModelsDlg::onDeleteClicked);
    connect(downloadAllBtn, &QPushButton::clicked, this, &ManageModelsDlg::onDownloadAllClicked);

    auto *btnRow = new QHBoxLayout;
    btnRow->addWidget(downloadBtn);
    btnRow->addWidget(deleteBtn);
    btnRow->addStretch();
    btnRow->addWidget(downloadAllBtn);

    orphanLabel = new QLabel(this);
    orphanLabel->setWordWrap(true);
    orphanList  = new QListWidget(this);
    orphanList->setMaximumHeight(80);
    deleteOrphansBtn = new QPushButton(tr("Delete Unused"), this);
    connect(deleteOrphansBtn, &QPushButton::clicked,
            this, &ManageModelsDlg::onDeleteOrphansClicked);

    auto *orphanRow = new QHBoxLayout;
    orphanRow->addStretch();
    orphanRow->addWidget(deleteOrphansBtn);

    footerLabel = new QLabel(this);
    footerLabel->setWordWrap(true);

    auto *revealBtn = new QPushButton(tr("Reveal Folder"), this);
    connect(revealBtn, &QPushButton::clicked, this, &ManageModelsDlg::onRevealClicked);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto *footRow = new QHBoxLayout;
    footRow->addWidget(revealBtn);
    footRow->addStretch();
    footRow->addWidget(buttons);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("Winnow downloads large AI models only when a feature needs one. "
           "They are kept in your application data folder and are not removed when "
           "Winnow is uninstalled."), this));
    layout->addWidget(table);
    layout->addLayout(btnRow);
    layout->addWidget(orphanLabel);
    layout->addWidget(orphanList);
    layout->addLayout(orphanRow);
    layout->addWidget(footerLabel);
    layout->addLayout(footRow);
    resize(680, 520);

    setStyleSheet(G::css);
#ifdef Q_OS_WIN
    Win::setTitleBarColor(winId(), G::backgroundColor);
#endif

    refresh();
}

void ManageModelsDlg::refresh()
{
    const QVector<ModelStore::ModelInfo> &cat = ModelStore::catalog();
    table->setRowCount(cat.size());
    bool anyDownloadable = false;

    for (int row = 0; row < cat.size(); ++row) {
        const ModelStore::ModelInfo &mi = cat.at(row);
        const State s = ModelStore::state(mi.id);

        auto *nameItem = new QTableWidgetItem(QString::fromLatin1(mi.fileName));
        nameItem->setData(kRoleModel, int(mi.id));
        table->setItem(row, kColName,    nameItem);
        table->setItem(row, kColFeature, new QTableWidgetItem(QString::fromLatin1(mi.feature)));
        table->setItem(row, kColSize,
                       new QTableWidgetItem(ModelDownloadDlg::formatBytes(mi.bytes)));

        auto *statusItem = new QTableWidgetItem(ModelStore::stateText(s));
        if (s == State::Stale || s == State::Corrupt) statusItem->setForeground(Qt::yellow);
        table->setItem(row, kColStatus, statusItem);

        if (!mi.bundled && s != State::Ready) anyDownloadable = true;
    }

    downloadAllBtn->setEnabled(anyDownloadable);
    downloadAllBtn->setText(anyDownloadable ? tr("Download All") : tr("All Downloaded"));

    const QVector<QString> orphanFiles = ModelStore::orphans();
    orphanList->clear();
    for (const QString &name : orphanFiles) orphanList->addItem(name);
    const bool hasOrphans = !orphanFiles.isEmpty();
    orphanLabel->setText(hasOrphans
        ? tr("<b>Unused - safe to delete (%1)</b><br>These models are no longer used by "
             "this version of Winnow.").arg(ModelDownloadDlg::formatBytes(ModelStore::orphanBytes()))
        : QString());
    orphanLabel->setVisible(hasOrphans);
    orphanList->setVisible(hasOrphans);
    deleteOrphansBtn->setVisible(hasOrphans);

    footerLabel->setText(tr("Using %1 in %2")
        .arg(ModelDownloadDlg::formatBytes(ModelStore::bytesOnDisk()), ModelStore::dir()));

    onSelectionChanged();
}

ModelStore::Model ManageModelsDlg::selectedModel(bool *ok) const
{
    const QList<QTableWidgetItem *> sel = table->selectedItems();
    if (sel.isEmpty()) { if (ok) *ok = false; return Model::U2Net; }
    QTableWidgetItem *nameItem = table->item(sel.first()->row(), kColName);
    if (!nameItem) { if (ok) *ok = false; return Model::U2Net; }
    if (ok) *ok = true;
    return Model(nameItem->data(kRoleModel).toInt());
}

void ManageModelsDlg::onSelectionChanged()
{
    bool ok = false;
    const Model m = selectedModel(&ok);
    if (!ok) {
        downloadBtn->setEnabled(false);
        deleteBtn->setEnabled(false);
        return;
    }
    const ModelStore::ModelInfo &mi = ModelStore::info(m);
    const State s = ModelStore::state(m);

    /* A bundled model lives inside the app: nothing to fetch, and not ours to delete. */
    downloadBtn->setEnabled(!mi.bundled && s != State::Ready);
    downloadBtn->setText(s == State::Stale ? tr("Update") : tr("Download"));
    deleteBtn->setEnabled(!mi.bundled && s != State::Missing);
}

void ManageModelsDlg::onDownloadClicked()
{
    bool ok = false;
    const Model m = selectedModel(&ok);
    if (!ok) return;
    ModelDownloadDlg dlg(QVector<Model>() << m, this);
    dlg.exec();
    ModelStore::invalidate(m);
    refresh();
}

void ManageModelsDlg::onDeleteClicked()
{
    bool ok = false;
    const Model m = selectedModel(&ok);
    if (!ok) return;
    const ModelStore::ModelInfo &mi = ModelStore::info(m);

    QMessageBox box(QMessageBox::Question, tr("Manage AI Models"),
                    tr("Delete %1 (%2)?<br><br>%3 will download it again the next time "
                       "it is needed.")
                        .arg(QString::fromLatin1(mi.fileName),
                             ModelDownloadDlg::formatBytes(mi.bytes),
                             QString::fromLatin1(mi.feature)),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setTextFormat(Qt::RichText);
    box.setStyleSheet(G::css);
    if (box.exec() != QMessageBox::Yes) return;

    ModelStore::remove(m);
    refresh();
}

void ManageModelsDlg::onDownloadAllClicked()
{
    QVector<Model> want;
    for (const ModelStore::ModelInfo &mi : ModelStore::catalog()) {
        if (mi.bundled) continue;
        if (ModelStore::state(mi.id) != State::Ready) want << mi.id;
    }
    if (want.isEmpty()) return;
    ModelDownloadDlg dlg(want, this);
    dlg.exec();
    for (Model m : want) ModelStore::invalidate(m);
    refresh();
}

void ManageModelsDlg::onDeleteOrphansClicked()
{
    const QVector<QString> names = ModelStore::orphans();
    if (names.isEmpty()) return;

    QMessageBox box(QMessageBox::Question, tr("Manage AI Models"),
                    tr("Delete %1 unused model file(s), freeing %2?")
                        .arg(names.size())
                        .arg(ModelDownloadDlg::formatBytes(ModelStore::orphanBytes())),
                    QMessageBox::Yes | QMessageBox::No, this);
    box.setStyleSheet(G::css);
    if (box.exec() != QMessageBox::Yes) return;

    const QDir d(ModelStore::dir());
    for (const QString &name : names) QFile::remove(d.filePath(name));
    refresh();
}

void ManageModelsDlg::onRevealClicked()
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(ModelStore::dir()));
}
