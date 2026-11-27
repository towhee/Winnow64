#include "Utilities/tipwrap.h"

#include <QApplication>
#include <QCursor>
#include <QEvent>
#include <QLabel>
#include <QScreen>
#include <QTextDocument>

void TipWrap::install()
{
    static TipWrap *filter = nullptr;
    if (filter || !qApp) return;
    filter = new TipWrap(qApp);
    qApp->installEventFilter(filter);
}

QSize TipWrap::wrappedSize(QLabel *tip)
/*
    The size the tooltip label should have once its text is wrapped at maxChars, or an
    invalid QSize when the text already fits (or there is none).  For plain text this
    also turns the label's word wrap on; Qt turns it back off itself on the next tip
    (QTipLabel::updateSize sets it from Qt::mightBeRichText every time).
*/
{
    const QString text = tip->text();
    if (text.isEmpty()) return {};

    const QFontMetrics fm(tip->font());
    const int maxTextW = fm.averageCharWidth() * maxChars;
    // QTipLabel::updateSize adds the same extra pixels (1 wide, 1 tall for the Mac font)
    const int extraW = 1;
    const int extraH = (fm.descent() == 2 && fm.ascent() >= 11) ? 1 : 0;

    if (!Qt::mightBeRichText(text)) {
        /* Same flags and measuring call QLabel uses for plain text, so the "chrome"
           (margins, indent, stylesheet border and padding) is exactly the sizeHint less
           the text. */
        const int flags = Qt::AlignLeft | Qt::TextExpandTabs;
        const int naturalW = fm.boundingRect(0, 0, 2000, 2000, flags, text).width();
        if (naturalW <= maxTextW) return {};
        tip->setWordWrap(false);
        const int chrome = tip->sizeHint().width() - naturalW;
        // tight width of the wrapped text (exceeds maxTextW only for an unbreakable word)
        const int wrappedW = fm.boundingRect(0, 0, maxTextW, 100000,
                                             flags | Qt::TextWordWrap, text).width();
        tip->setWordWrap(true);
        const int w = wrappedW + chrome + extraW;
        return QSize(w, tip->heightForWidth(w) + extraH);
    }

    // rich text: Qt has already turned word wrap on
    const QMargins cm = tip->contentsMargins();
    const int docMargin = 2 * int(QTextDocument().documentMargin());
    const int w = maxTextW + cm.left() + cm.right() + 2 * tip->margin()
                  + qMax(0, tip->indent()) + docMargin + extraW;
    if (tip->sizeHint().width() <= w) return {};
    tip->setWordWrap(true);
    return QSize(w, tip->heightForWidth(w) + extraH);
}

bool TipWrap::eventFilter(QObject *obj, QEvent *event)
/*
    Two moments, because QToolTip::showText has two paths (Qt 6.11 qtooltip.cpp):

    - Reuse: a tip is already showing and its text changes.  reuseTip -> updateSize ->
      resize() runs on a VISIBLE label, so the Resize event arrives synchronously, before
      placeTip positions it.  Narrowing it here means Qt places the narrow tip.

    - New: the label is created hidden, sized and placed (Resize/Move are only pending),
      then shown.  The Show event arrives before the window maps, but after placement
      was computed for the wide size, so the tip is re-placed here: it keeps its left
      edge (or right edge, if Qt flipped it to the left of the cursor) and its top edge
      (or bottom edge, if Qt flipped it above the cursor), then is kept on screen.
*/
{
    const QEvent::Type type = event->type();
    if (type != QEvent::Show && type != QEvent::Resize) return false;
    if (resizing || !obj->isWidgetType() || !obj->inherits("QTipLabel")) return false;

    auto *tip = static_cast<QLabel *>(obj);
    if (type == QEvent::Resize && !tip->isVisible()) return false;

    const QRect old = tip->geometry();
    const QSize size = wrappedSize(tip);
    if (!size.isValid() || size == old.size()) return false;

    resizing = true;
    if (type == QEvent::Resize) {
        tip->resize(size);
    }
    else {
        const QPoint cursor = QCursor::pos();
        int x = old.x();
        int y = old.y();
        if (old.right() < cursor.x()) x = old.right() + 1 - size.width();
        if (old.bottom() < cursor.y()) y = old.bottom() + 1 - size.height();
        if (const QScreen *screen = tip->screen()) {
            const QRect s = screen->geometry();
            x = qBound(s.left(), x, qMax(s.left(), s.right() + 1 - size.width()));
            y = qBound(s.top(), y, qMax(s.top(), s.bottom() + 1 - size.height()));
        }
        tip->setGeometry(QRect(QPoint(x, y), size));
    }
    resizing = false;
    return false;
}
