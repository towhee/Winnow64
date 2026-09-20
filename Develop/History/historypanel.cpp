#include "Develop/History/historypanel.h"

#include <QCursor>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QMenu>
#include <QAbstractItemModel>
#include <QContextMenuEvent>
#include <QListWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QSettings>
#include <QVBoxLayout>

#include "Develop/History/historyview.h"
#include "Develop/Presets/presetsview.h"
#include "Main/dockwidget.h"
#include "Main/global.h"

/* ------------------------------------------------------------------ PanelSectionHeader */

PanelSectionHeader::PanelSectionHeader(const QString &title, QWidget *parent)
    : QWidget(parent)
{
    /*  Translucent for the same reason every other band in this dock is: under the app
        stylesheet a plain QWidget fills its background opaquely, which would paint over
        the gradient paintEvent draws. */
    setAttribute(Qt::WA_TranslucentBackground);

    hb = new QHBoxLayout(this);
    hb->setContentsMargins(4, 3, G::headerBtnRightInset, 3);
    hb->setSpacing(0);

    /*  The arrow, sized and styled as the Edits band's arrow (ScopeHeader) so the two
        read as the same control. */
    arrowBtn = new BarBtn();
    arrowBtn->setToolTip("Hide or show this section");
    arrowBtn->setIconSize(QSize(9, 9));
    arrowBtn->setFixedSize(9, 16);
    arrowBtn->setStyleSheet("QToolButton { border: none; padding: 0;"
                            " background: transparent; }");
    connect(arrowBtn, &BarBtn::clicked, this, [this]{
        setExpanded(!expanded);
        emit toggled(expanded);
    });
    hb->addWidget(arrowBtn, 0, Qt::AlignVCenter);
    hb->addSpacing(6);

    label = new QLabel(title, this);
    label->setStyleSheet(G::labelCss(G::header2Color, G::strFontSize.toInt()));
    /*  The caption is part of the arrow: clicking the word is the gesture users reach for
        before they find the 9px triangle. */
    label->installEventFilter(this);
    hb->addWidget(label, 0, Qt::AlignVCenter);

    hb->addStretch(1);

    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    updateArrowIcon();
}

void PanelSectionHeader::addButton(BarBtn *btn)
{
    if (!btn) return;
    hb->addWidget(btn, 0, Qt::AlignVCenter);
    hb->addSpacing(10);
}

void PanelSectionHeader::setExpanded(bool isExpanded)
{
    expanded = isExpanded;
    updateArrowIcon();
}

void PanelSectionHeader::updateArrowIcon()
{
    if (!arrowBtn) return;
    const QString path = expanded ? ":/images/branch-open-winnow.png"
                                  : ":/images/branch-closed-winnow.png";
    arrowBtn->setIcon(QIcon(QPixmap(path)));
}

bool PanelSectionHeader::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == label && event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent*>(event);
        if (me->button() == Qt::LeftButton) {
            setExpanded(!expanded);
            emit toggled(expanded);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void PanelSectionHeader::paintEvent(QPaintEvent *)
{
    /*  The app's header band: the a -> b ramp PropertyDelegate paints behind the Develop
        editor's Basic / Color / Effects rows, so a section header reads the same
        wherever it appears. */
    QPainter p(this);
    const int a = G::backgroundShade + 5;
    const int b = G::backgroundShade - 15;
    QLinearGradient g(0, 0, 0, height());
    g.setColorAt(0, QColor(a, a, a));
    g.setColorAt(1, QColor(b, b, b));
    p.fillRect(rect(), g);
}

/* ----------------------------------------------------------------------- HistoryPanel */

HistoryPanel::HistoryPanel(QWidget *parent, QSettings *setting)
    : QWidget(parent), setting(setting)
{
    QVBoxLayout *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    v->setSpacing(0);

    /* ---- History ---- */
    histHeader = new PanelSectionHeader(tr("History"), this);
    BarBtn *histMenuBtn = new BarBtn();
    histMenuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    histMenuBtn->setToolTip("History actions");
    connect(histMenuBtn, &BarBtn::clicked, this, [this]{ showHistoryMenu(); });
    histHeader->addButton(histMenuBtn);
    v->addWidget(histHeader);

    histView = new HistoryView(this);
    v->addWidget(histView);

    /* ---- Presets ---- */
    presHeader = new PanelSectionHeader(tr("Presets"), this);
    /*  New preset: the same flow as Cmd+Shift+N, put where the presets are so it can be
        found without knowing the shortcut. */
    BarBtn *presNewBtn = new BarBtn();
    presNewBtn->setIcon(":/images/icon16/new.png", G::iconOpacity);
    presNewBtn->setToolTip("Create a develop preset from this image  (Cmd+Shift+N)");
    connect(presNewBtn, &BarBtn::clicked, this, &HistoryPanel::newPresetRequested);
    presHeader->addButton(presNewBtn);
    BarBtn *presMenuBtn = new BarBtn();
    presMenuBtn->setIcon(":/images/icon16/ellipsis_vertical.png", G::iconOpacity);
    presMenuBtn->setToolTip("Preset actions");
    presHeader->addButton(presMenuBtn);
    v->addWidget(presHeader);

    presView = new PresetsView(this);
    v->addWidget(presView);

    /*  THE ONLY STRETCH IN THE PANEL, and it is last: the two bands and their lists pack
        against the TOP, and whatever is left over falls to the bottom as empty panel.
        Giving the lists the stretch instead spread the leftover between them, which
        pushed the Presets band into the middle of the dock and left a pane of empty
        background under each band. */
    v->addStretch(1);

    /*  The band's menu is the list's own context menu (so there is one set of preset
        actions, not two that can drift), acting on the SELECTED preset, plus help. */
    connect(presMenuBtn, &BarBtn::clicked, this, [this, presMenuBtn]{
        presView->showSectionMenu(presMenuBtn->mapToGlobal(
            QPoint(0, presMenuBtn->height())));
    });
    connect(presView, &PresetsView::helpRequested, this, []{ showPresetsHelp(); });

    connect(histHeader, &PanelSectionHeader::toggled,
            this, [this](bool on){ setHistorySection(on); });
    connect(presHeader, &PanelSectionHeader::toggled,
            this, [this](bool on){ setPresetsSection(on); });

    /*  Restore the remembered state. Both sections open on a first run: the panel is off
        by default, so the session that turns it on wants to see what is in it. */
    solo = setting ? setting->value("Develop/SectionSolo", false).toBool() : false;
    const bool histOn = setting
        ? setting->value("Develop/SectionExpanded/HistorySection", true).toBool() : true;
    bool presOn = setting
        ? setting->value("Develop/SectionExpanded/PresetsSection", true).toBool() : true;
    /*  Solo mode was on when the settings were written, so the pair cannot both be open.
        Guard anyway: the two keys are written independently and a hand-edited (or
        half-written) settings file must not open the panel in a state solo mode forbids. */
    if (solo && histOn && presOn) presOn = false;
    histHeader->setExpanded(histOn);
    histView->setVisible(histOn);
    presHeader->setExpanded(presOn);
    presView->setVisible(presOn);

    /*  Re-fit on every rebuild. Hooked to the MODEL rather than to the views' refresh()
        so it catches every route that changes the rows -- a new edit, an image change, a
        preset added or deleted -- without either view having to report back. */
    for (QListWidget *view : {static_cast<QListWidget*>(histView),
                              static_cast<QListWidget*>(presView)}) {
        QAbstractItemModel *m = view->model();
        connect(m, &QAbstractItemModel::rowsInserted, this, [this, view]{ fitListToContent(view); });
        connect(m, &QAbstractItemModel::rowsRemoved,  this, [this, view]{ fitListToContent(view); });
        connect(m, &QAbstractItemModel::modelReset,   this, [this, view]{ fitListToContent(view); });
        view->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    }
    fitListsToContent();
}

bool HistoryPanel::historyExpanded() const { return histHeader && histHeader->isExpanded(); }
bool HistoryPanel::presetsExpanded() const { return presHeader && presHeader->isExpanded(); }

void HistoryPanel::setSectionExpanded(PanelSectionHeader *header, QWidget *body,
                                      const QString &key, bool expanded)
{
    if (!header || !body) return;
    header->setExpanded(expanded);
    body->setVisible(expanded);
    if (setting) setting->setValue("Develop/SectionExpanded/" + key, expanded);
}

/*  Both emit unconditionally, even when the section is already open: the callers (H, P,
    the View menu, the Develop action row's preset button) use the signal to re-sync their
    checked state, and an already-open section is exactly when that state can be stale. */

void HistoryPanel::setHistorySection(bool expanded)
{
    setSectionExpanded(histHeader, histView, "HistorySection", expanded);
    emit historyExpandedChanged(expanded);
    /*  Solo: opening this one folds the other. Only on the OPEN -- closing a section in
        solo mode leaves the panel with nothing open, which is a legitimate state. */
    if (expanded && solo && presetsExpanded()) {
        setSectionExpanded(presHeader, presView, "PresetsSection", false);
        emit presetsExpandedChanged(false);
    }
}

void HistoryPanel::setPresetsSection(bool expanded)
{
    setSectionExpanded(presHeader, presView, "PresetsSection", expanded);
    emit presetsExpandedChanged(expanded);
    if (expanded && solo && historyExpanded()) {
        setSectionExpanded(histHeader, histView, "HistorySection", false);
        emit historyExpandedChanged(false);
    }
}

void HistoryPanel::expandHistory() { setHistorySection(true); }
void HistoryPanel::expandPresets() { setPresetsSection(true); }

void HistoryPanel::setSoloMode(bool on)
{
    solo = on;
    if (setting) setting->setValue("Develop/SectionSolo", on);
    /*  Turning solo ON with both sections open has to resolve the contradiction now
        rather than wait for the next click. History wins: it is the panel's primary list
        and the one the panel is named for. */
    if (solo && historyExpanded() && presetsExpanded()) {
        setSectionExpanded(presHeader, presView, "PresetsSection", false);
        emit presetsExpandedChanged(false);
    }
}

void HistoryPanel::contextMenuEvent(QContextMenuEvent *event)
{
    QMenu menu(this);
    QAction *aExpand   = menu.addAction(tr("Expand all"));
    QAction *aCollapse = menu.addAction(tr("Collapse all"));
    menu.addSeparator();
    QAction *aSolo = menu.addAction(tr("Solo mode"));
    aSolo->setCheckable(true);
    aSolo->setChecked(solo);
    aSolo->setToolTip(tr("Keep one section open at a time"));

    QAction *chosen = menu.exec(event->globalPos());
    event->accept();
    if (!chosen) return;

    if (chosen == aExpand) {
        /*  "Expand all" in solo mode is a contradiction, and the user asking for it has
            said which half they meant: they want both sections open, so solo goes off
            rather than the command being refused. */
        setSoloMode(false);
        setHistorySection(true);
        setPresetsSection(true);
    }
    else if (chosen == aCollapse) {
        setHistorySection(false);
        setPresetsSection(false);
    }
    else if (chosen == aSolo) {
        setSoloMode(!solo);
    }
}

void HistoryPanel::fitListToContent(QListWidget *view)
{
/*
    A list is exactly as tall as its rows, so the next band sits under the last row.

    The ceiling is half the panel: a long history (up to 100 entries) would otherwise push
    the Presets band off the bottom, and a list that hits the ceiling simply scrolls. It
    is a MAXIMUM rather than a fixed height, so when both lists are full the layout can
    still squeeze them to fit rather than overflowing the dock.
*/
    if (!view) return;
    int h = 2 * view->frameWidth();
    for (int i = 0; i < view->count(); ++i) h += view->sizeHintForRow(i);
    /* An empty list keeps one row's worth, so it reads as an empty list rather than as
       two bands stacked on each other. */
    const int rowH = view->count() ? view->sizeHintForRow(0)
                                   : view->fontMetrics().height() + 8;
    const int minH = rowH + 2 * view->frameWidth();
    const int maxH = qMax(minH, height() / 2);
    view->setMaximumHeight(qBound(minH, h, maxH));
}

void HistoryPanel::fitListsToContent()
{
    fitListToContent(histView);
    fitListToContent(presView);
}

void HistoryPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    fitListsToContent();
}

void HistoryPanel::showHistoryMenu()
{
    /*  One item today. It is a menu rather than a bare [?] so the band matches the
        Presets band beside it, and so a later "Clear history" has somewhere to go. */
    QMenu menu(this);
    QAction *aHelp = menu.addAction(tr("History help"));
    if (menu.exec(QCursor::pos()) == aHelp) showHistoryHelp();
}

void HistoryPanel::showHistoryHelp()
{
    if (G::popup) G::popup->showPopup(
        "<b>Develop History</b><br>"
        "Every develop action for this image, newest first.<br>"
        "Hover an entry to preview that state; click it to go back to it.<br>"
        "Editing from an earlier entry discards the entries after it.<br><br>"
        "History is per image and lasts for this session -- the sidecar keeps the "
        "current state, not the steps.", 6000);
}

void HistoryPanel::showPresetsHelp()
{
    if (G::popup) G::popup->showPopup(
        "<b>Develop Presets</b><br>"
        "Your saved develop recipes. Hover one to preview it on this image; "
        "click it to apply it.<br>"
        "A preset holds only the settings you ticked when you saved it, so "
        "applying it leaves everything else alone.<br>"
        "It is applied to the scope selected in the Develop panel.<br><br>"
        "Click + (or Cmd+Shift+N) to make one from the current image. "
        "Right-click a preset to update, rename or delete it.<br><br>"
        "For a one-off, skip the preset: Cmd+Opt+C copies the settings you tick "
        "and Cmd+Opt+V pastes them onto another image.", 7000);
}
