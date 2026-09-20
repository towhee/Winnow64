#ifndef PANELBUTTONBAR_H
#define PANELBUTTONBAR_H

#include <QAbstractButton>
#include <QAction>
#include <QBoxLayout>
#include <QList>
#include <QPushButton>
#include <QString>
#include <QVariant>
#include <QWidget>

#include "Main/global.h"

/*
    PanelButtonBar -- a row of push buttons pinned to the TOP or the BOTTOM edge of a dock
    panel, each button stretched to an equal share of the panel width.

        | > History                              [:] |
        | > Presets                          [+] [:] |
        |                                            |
        | ------------------------------------------ |
        |  [      Copy      ] [      Paste      ]    |   <- this

    The row is a sibling of the panel's other children in the panel's own QVBoxLayout, so
    it moves with the panel as the dock is resized and stays against the edge it was given.
    A BOTTOM row is APPENDED, which puts it after the trailing addStretch(1) every packed
    panel ends with (HistoryPanel's is the model) -- the stretch eats the slack above it
    and the row lands on the bottom edge. A TOP row is inserted at index 0.

    Every in-dock button row before this one was hand-rolled at its call site
    (CatalogView's Load/Add row, MaskPanel's commit row), and each re-discovered the same
    two traps, both handled here:

      - The app stylesheet's "QPushButton { min-width: 100px }" (WidgetCSS::pushButton) is a
        real minimum, so a row of buttons becomes a hard FLOOR on how narrow the dock can
        be dragged. Each button gets "min-width: 0" to defeat it.
      - Under the app stylesheet a plain QWidget fills its background opaquely, which would
        paint a slab over the panel. The bar's background is explicitly transparent.

    The bar carries the same panel separator rule as every other Develop panel boundary
    (G::panelBorderHeight in G::panelSeparatorColor()), on the side facing the panel
    contents. That colour is derived from G::backgroundShade, and a per-widget stylesheet is
    NOT regenerated when the Preferences brightness slider moves, so MW::setBackgroundShade
    calls Restyle().

    PLAIN FUNCTIONS OVER A PLAIN QWidget, NO Q_OBJECT: nothing here declares a signal or
    overrides paintEvent, so it needs no moc and lives entirely in this header -- the same
    shape as Utilities/gradientheader.h.
*/
namespace PanelButtonBar
{
    enum Edge { Top, Bottom };

    /* Set on the bar container so Restyle() knows which side its rule is on. */
    const char *const kEdgeProperty = "panelButtonBarEdge";

    /* The ID-scoped sheet: transparent background (so the panel shows through) plus the
       panel separator on the side facing the panel contents. The ID selector keeps the
       border off the child buttons. */
    inline void ApplyBarStyle(QWidget *bar)
    {
        if (!bar) return;
        const Edge edge = static_cast<Edge>(bar->property(kEdgeProperty).toInt());
        const QString side = (edge == Top) ? "border-bottom" : "border-top";
        bar->setStyleSheet(
            QString("QWidget#panelButtonBar { background: transparent; %1: %2px solid %3; }")
                .arg(side)
                .arg(G::panelBorderHeight)
                .arg(G::panelSeparatorColor().name()));
    }

    /*  Build a QPushButton that triggers action. The label defaults to the action's text
        with the menu decorations stripped (& accelerators, a trailing ellipsis); pass an
        explicit label when the action's text is long or is rewritten at runtime, as
        developPasteSettingsAction's is ("Paste Develop Settings from <image>").
        Enabled state FOLLOWS THE ACTION rather than being tracked again here, so a button
        greys with its menu item wherever the action is gated. */
    inline QPushButton *MakeButton(QAction *action, const QString &label = QString(),
                                   QWidget *parent = nullptr)
    {
        if (!action) return nullptr;

        QString text = label;
        if (text.isEmpty()) {
            text = action->text();
            text.remove('&');
            if (text.endsWith(QString::fromUtf8("…"))) text.chop(1);
            if (text.endsWith("...")) text.chop(3);
            text = text.trimmed();
        }

        QPushButton *btn = new QPushButton(text, parent);

        QString tip = action->toolTip();
        if (tip.isEmpty() || tip == action->text()) tip = text;
        if (!action->shortcut().isEmpty())
            tip += "  (" + action->shortcut().toString(QKeySequence::NativeText) + ")";
        btn->setToolTip(tip);

        btn->setEnabled(action->isEnabled());
        QObject::connect(action, &QAction::changed, btn, [btn, action]() {
            btn->setEnabled(action->isEnabled());
        });
        QObject::connect(btn, &QPushButton::clicked, action, [action]() {
            action->trigger();
        });

        return btn;
    }

    /*  Add buttons as a row pinned to edge of panel. Returns the bar container, or nullptr
        if the panel cannot take one (no buttons, or a layout that is not vertical -- better
        to say nothing was added than to drop the row in an arbitrary cell). */
    inline QWidget *Add(QWidget *panel, const QList<QAbstractButton*> &buttons,
                        Edge edge = Bottom)
    {
        if (!panel || buttons.isEmpty()) return nullptr;

        QVBoxLayout *v = qobject_cast<QVBoxLayout*>(panel->layout());
        if (!v) {
            if (panel->layout()) return nullptr;     // some other layout: not ours to reorder
            v = new QVBoxLayout(panel);
            v->setContentsMargins(0, 0, 0, 0);
            v->setSpacing(0);
        }

        QWidget *bar = new QWidget(panel);
        bar->setObjectName("panelButtonBar");
        bar->setProperty(kEdgeProperty, static_cast<int>(edge));
        ApplyBarStyle(bar);
        /*  Preferred/Fixed, never setFixedHeight or a minimum: DockWidget::setCollapsed
            zeroes the body's minimum height to fold the dock, and a hard floor anywhere
            inside the body propagates back up and blocks it. */
        bar->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

        QHBoxLayout *hb = new QHBoxLayout(bar);
        /*  The extra margin on the ruled side reserves the separator's own space, the same
            way the Develop action row does it. */
        hb->setContentsMargins(6, 4 + (edge == Bottom ? G::panelBorderHeight : 0),
                               6, 4 + (edge == Top    ? G::panelBorderHeight : 0));
        hb->setSpacing(6);

        for (QAbstractButton *btn : buttons) {
            if (!btn) continue;
            btn->setParent(bar);
            /*  Defeats the global "QPushButton { min-width: 100px }", which would otherwise
                floor the width of the whole dock at buttons x 100px. */
            btn->setStyleSheet("QPushButton { min-width: 0; }");
            btn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            hb->addWidget(btn, 1);          // equal stretch = equal share of the width
        }

        /*  Appended (not inserted) for a bottom row, so it sits AFTER the trailing
            addStretch(1) a packed panel ends with and lands on the bottom edge. */
        if (edge == Top) v->insertWidget(0, bar);
        else             v->addWidget(bar);

        return bar;
    }

    /*  Convenience: build the buttons from their actions first. */
    inline QWidget *Add(QWidget *panel, const QList<QAction*> &actions, Edge edge = Bottom)
    {
        QList<QAbstractButton*> buttons;
        for (QAction *a : actions) {
            if (QPushButton *btn = MakeButton(a)) buttons << btn;
        }
        return Add(panel, buttons, edge);
    }

    /*  Re-apply the separator colour to every bar under root. Called from
        MW::setBackgroundShade, where G::panelSeparatorColor() changes. */
    inline void Restyle(QWidget *root)
    {
        if (!root) return;
        const QList<QWidget*> bars = root->findChildren<QWidget*>("panelButtonBar");
        for (QWidget *bar : bars) ApplyBarStyle(bar);
    }
}

#endif // PANELBUTTONBAR_H
