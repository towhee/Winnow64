#ifndef GRADIENTHEADER_H
#define GRADIENTHEADER_H

#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QString>
#include <QWidget>

#include "Main/global.h"

/*
    A titled gradient band -- the app's section header, as a widget a panel can just add
    to a layout.

    The Develop panels each paint this band themselves (ReplacePanel, TransformPanel,
    MaskPanel, SubmaskList, ScopeHeader) because each also hangs its own eye / menu /
    button row off it. A panel that wants nothing but the title has no reason to carry a
    fourth copy of the gradient, so it uses this.

    PLAIN QWidget WITH NO _Q_OBJECT: it overrides paintEvent and declares no signals, so
    it needs no moc and can live entirely in this header.
*/
class GradientHeader : public QWidget
{
public:
    explicit GradientHeader(const QString &text, QWidget *parent = nullptr)
        : QWidget(parent)
    {
        /*  Translucent for the same reason the Develop bands are: under the app
            stylesheet a plain QWidget fills its background opaquely, which would paint
            over the gradient this draws. */
        setAttribute(Qt::WA_TranslucentBackground);
        QHBoxLayout *hb = new QHBoxLayout(this);
        hb->setContentsMargins(4, 3, G::headerBtnRightInset, 3);
        hb->setSpacing(0);
        label = new QLabel(text, this);
        label->setStyleSheet(G::labelCss(G::header2Color, G::strFontSize.toInt()));
        hb->addWidget(label);
        hb->addStretch(1);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    }

    void setText(const QString &text) { label->setText(text); }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        const int a = G::backgroundShade + 5;
        const int b = G::backgroundShade - 15;
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, QColor(a, a, a));
        g.setColorAt(1, QColor(b, b, b));
        p.fillRect(rect(), g);
    }

private:
    QLabel *label = nullptr;
};

#endif // GRADIENTHEADER_H
