#ifndef TIPWRAP_H
#define TIPWRAP_H

#include <QObject>
#include <QSize>

class QLabel;

/*  TipWrap caps the width of EVERY tooltip in the application, wrapping the text at about
    maxChars average characters per line.  It is one application-wide event filter on Qt's
    tooltip label (QTipLabel), so no setToolTip() call, .ui file, model ToolTipRole or
    QToolTip::showText() caller needs to do anything: write tooltips as plain sentences
    and let them wrap.  A "\n" in the text is still a hard line break.

    Plain-text tooltips: Qt never wraps them (until they are wider than the screen), so
    the label gets word wrap and a width that fits the wrapped text tightly.

    Rich-text tooltips: Qt wraps them at its own QLabel heuristic (often ~80 characters);
    the label is narrowed to the same cap.

    The text itself is never changed: QToolTip compares the label's text with the next
    request to decide whether the tip "changed", and an altered text would make every
    mouse move re-place the tip.  See notes/Documentation.md "Tooltip Width". */

class TipWrap : public QObject
{
    Q_OBJECT
public:
    static constexpr int maxChars = 55;   // line width, in average character widths
    static void install();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    explicit TipWrap(QObject *parent) : QObject(parent) {}
    static QSize wrappedSize(QLabel *tip);
    bool resizing = false;
};

#endif // TIPWRAP_H
