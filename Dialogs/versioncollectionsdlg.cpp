#include "Dialogs/versioncollectionsdlg.h"
#include "Datamodel/collectionstore.h"
#include "Main/global.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

VersionCollectionsDlg::VersionCollectionsDlg(const QVector<qint64> &offered,
                                             const QHash<qint64, int> &imagesIn,
                                             int images, QWidget *parent)
    : QDialog(parent)
{
    if (G::isLogger) G::log("VersionCollectionsDlg::VersionCollectionsDlg");
    setWindowTitle(tr("New Version"));
    resize(380, 320);

    QLabel *msg = new QLabel(images == 1
        ? tr("This image is in the collections below. Tick the ones the new version "
             "should be in too.")
        : tr("These images are in the collections below. Tick the ones their new "
             "versions should be in too -- each version joins only the collections "
             "its own image is in."));
    msg->setWordWrap(true);

    /*  Each collection by its place in the tree ("Trips / 2024"), as the Collections
        panel nests it, so two of the same name in different branches can be told
        apart. */
    QHash<qint64, CollectionStore::Node> byId;
    for (const CollectionStore::Node &n
         : CollectionStore::instance().nodes(CollectionStore::Kind::Collection))
        byId.insert(n.id, n);
    auto pathOf = [&](qint64 id) {
        QStringList parts;
        for (int guard = 0; id && byId.contains(id) && guard < 64; ++guard) {
            parts.prepend(byId.value(id).name);
            id = byId.value(id).parent;
        }
        return parts.join(" / ");
    };

    list = new QListWidget;
    list->setSelectionMode(QAbstractItemView::NoSelection);
    for (qint64 id : offered) {
        QString text = pathOf(id);
        if (images > 1)
            text += "  " + tr("(%n image(s))", "", imagesIn.value(id));
        QListWidgetItem *it = new QListWidgetItem(text, list);
        it->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        it->setCheckState(Qt::Checked);
        it->setData(Qt::UserRole, id);
    }

    QPushButton *all = new QPushButton(tr("Select All"));
    QPushButton *none = new QPushButton(tr("Select None"));
    auto setAll = [this](Qt::CheckState s) {
        for (int i = 0; i < list->count(); ++i) list->item(i)->setCheckState(s);
    };
    connect(all, &QPushButton::clicked, this, [setAll] { setAll(Qt::Checked); });
    connect(none, &QPushButton::clicked, this, [setAll] { setAll(Qt::Unchecked); });
    QHBoxLayout *pick = new QHBoxLayout;
    pick->addWidget(all);
    pick->addWidget(none);
    pick->addStretch(1);

    QDialogButtonBox *buttons =
        new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttons->button(QDialogButtonBox::Ok)->setText(
        images == 1 ? tr("Create Version") : tr("Create Versions"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *l = new QVBoxLayout(this);
    l->addWidget(msg);
    l->addWidget(list, 1);
    l->addLayout(pick);
    l->addWidget(buttons);
}

QVector<qint64> VersionCollectionsDlg::checkedIds() const
{
    QVector<qint64> out;
    for (int i = 0; i < list->count(); ++i) {
        const QListWidgetItem *it = list->item(i);
        if (it->checkState() == Qt::Checked) out << it->data(Qt::UserRole).toLongLong();
    }
    return out;
}
