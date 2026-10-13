#include "Dialogs/applecamerasdlg.h"
#include "ImageFormats/Raw/applerawdecode.h"
#include "Main/global.h"

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>

AppleCamerasDlg::AppleCamerasDlg(int initialVersion, QWidget *parent)
    : QDialog(parent)
{
    if (G::isLogger) G::log("AppleCamerasDlg::AppleCamerasDlg");
    setWindowTitle("Cameras Supported by Apple Raw");
    resize(420, 560);

    QLabel *versionLbl = new QLabel("Apple Raw version");
    versionCombo = new QComboBox;
    filterEdit = new QLineEdit;
    filterEdit->setPlaceholderText("Filter, e.g. Nikon or D850");
    filterEdit->setClearButtonEnabled(true);
    list = new QListWidget;
    list->setSelectionMode(QAbstractItemView::NoSelection);
    countLabel = new QLabel;
    QLabel *note = new QLabel(
        "Apple's list, by camera model and in Apple's spelling. A particular file can "
        "offer fewer versions than its camera is listed under (an uncompressed file, "
        "for example). The Render using list always shows what the current image "
        "actually supports.");
    note->setWordWrap(true);

#ifdef Q_OS_MAC
    const QList<int> versions = AppleRawDecode::AvailableVersions();
    bool perVersion = false;
    AppleRawDecode::SupportedCameraModels(versions.value(0), &perVersion);
    if (perVersion) {
        for (int v : versions) versionCombo->addItem(QString("Apple Raw %1").arg(v), v);
        const int at = versionCombo->findData(initialVersion);
        versionCombo->setCurrentIndex(at >= 0 ? at : 0);
    }
    else {
        /* Before macOS 27 Core Image has one list for every version. Said in the
           dropdown, disabled, rather than offering choices that all show the same. */
        versionCombo->addItem("All versions (per-version lists need macOS 27)", 0);
        versionCombo->setEnabled(false);
    }
#else
    versionCombo->addItem("Apple Raw is macOS only", 0);
    versionCombo->setEnabled(false);
#endif

    QHBoxLayout *top = new QHBoxLayout;
    top->addWidget(versionLbl);
    top->addWidget(versionCombo, 1);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    QVBoxLayout *l = new QVBoxLayout(this);
    l->addLayout(top);
    l->addWidget(filterEdit);
    l->addWidget(list, 1);
    l->addWidget(countLabel);
    l->addWidget(note);
    l->addWidget(buttons);

    connect(versionCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int){ loadVersion(); });
    connect(filterEdit, &QLineEdit::textChanged, this, [this]{ applyFilter(); });
    loadVersion();
}

void AppleCamerasDlg::loadVersion()
{
    list->clear();
#ifdef Q_OS_MAC
    /* ~0.2 s the first time per version (cached after): a wait cursor, not a progress
       popup -- it is well under the one-second line. */
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const QStringList models =
        AppleRawDecode::SupportedCameraModels(versionCombo->currentData().toInt());
    QApplication::restoreOverrideCursor();
    list->addItems(models);
#endif
    applyFilter();
}

void AppleCamerasDlg::applyFilter()
{
    const QString f = filterEdit->text().trimmed();
    int shown = 0;
    for (int i = 0; i < list->count(); ++i) {
        QListWidgetItem *it = list->item(i);
        const bool match = f.isEmpty() || it->text().contains(f, Qt::CaseInsensitive);
        it->setHidden(!match);
        if (match) ++shown;
    }
    countLabel->setText(f.isEmpty()
        ? QString("%1 cameras").arg(list->count())
        : QString("%1 of %2 cameras").arg(shown).arg(list->count()));
}
