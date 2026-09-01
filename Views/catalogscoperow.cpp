#include "Views/catalogscoperow.h"
#include "Main/global.h"

#include <QLocale>

CatalogScopeRow::CatalogScopeRow(QWidget *parent) : QToolButton(parent)
{
    if (G::isLogger) G::log("CatalogScopeRow::CatalogScopeRow");

    setCheckable(true);
    setChecked(false);
    setAutoRaise(false);
    setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    setIcon(QIcon(":/images/icon16/catalog_white.png"));
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setToolTip("Every image Winnow has catalogued, across all folders.\n"
               "Filter it the same way you filter a folder.");
    refreshText();
    updateStyle();
}

void CatalogScopeRow::setImageCount(qint64 count)
{
    if (count == imageCount) return;
    imageCount = count;
    refreshText();
}

void CatalogScopeRow::refreshText()
{
    if (imageCount < 0) {
        setText("  Catalog");
        return;
    }
    setText(QString("  Catalog   %1").arg(QLocale().toString(imageCount)));
}

void CatalogScopeRow::updateStyle()
{
/*
    Painted from G::backgroundShade rather than left to the app stylesheet.

    The app-wide QToolButton rule (Main/widgetcss.cpp) is "background:transparent;
    border:none" with NO :checked state, because every other tool button in Winnow is a
    momentary icon button. Inheriting it would leave this row looking identical whether
    it is the current scope or not, which for a selector is the whole of its job -- the
    same reason the Find dock's scope buttons carry their own stylesheet. It is styled
    here rather than in widgetcss for that reason too: a global :checked rule would light
    up every checkable tool button in the app.

    The CHECKED colour is G::selectionColor, so the row reads as selected in the same
    language as a selected folder in the tree beneath it. That pairing is the point --
    the two are alternatives, and they should look like alternatives.
*/
    const QString hover = QColor(G::backgroundShade + 14, G::backgroundShade + 14,
                                 G::backgroundShade + 14).name();
    const QString css = QString(
        "QToolButton {"
        "  background:transparent; border:none; padding:4px 6px;"
        "  color:%1; text-align:left;"
        "}"
        "QToolButton:checked {"
        "  background:%2; color:%3; font-weight:bold;"
        "}"
        "QToolButton:hover:!checked { background:%4; }"
    ).arg(G::textColor.name(), G::selectionColor.name(),
          G::textColor.name(), hover);
    setStyleSheet(css);
}
