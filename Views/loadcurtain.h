#ifndef LOADCURTAIN_H
#define LOADCURTAIN_H

#include <QtWidgets>

/*
    An opaque cover laid over a view while what it would show is not yet the answer --
    see MW::raiseLoadCurtain. It is a CHILD of the view, raised above everything in it,
    rather than a hide() of the view itself: a hidden view has no visible cells, so the
    icon range, the scroll sync and every isVisible() gate downstream would change their
    answers for the length of the wait. Covered, the view goes on working exactly as
    usual underneath, and lifting the curtain shows it as it already is.

    It follows its parent's size by an event filter, and covers the parent's contentsRect
    so a FrameLineBox keeps its border. An optional centred message takes the place of the
    central message pane while the curtain is up.
*/
class LoadCurtain : public QWidget
{
public:
    explicit LoadCurtain(QWidget *parent) : QWidget(parent)
    {
        setObjectName("LoadCurtain");
        setAttribute(Qt::WA_StyledBackground, true);
        setAutoFillBackground(true);
        label = new QLabel(this);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(true);
        auto *layout = new QVBoxLayout(this);
        layout->addWidget(label);
        parent->installEventFilter(this);
        QWidget::hide();
    }

    void cover(const QString &css)
    {
        setStyleSheet(css);
        fit();
        show();
        raise();
    }

    void setMessage(const QString &text) { label->setText(text); }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == parentWidget() && isVisible()) {
            if (event->type() == QEvent::Resize) fit();
            // a child added after the curtain went up would otherwise land on top of it
            else if (event->type() == QEvent::ChildAdded) raise();
        }
        return QWidget::eventFilter(watched, event);
    }

private:
    void fit() { setGeometry(parentWidget()->contentsRect()); }
    QLabel *label;
};

#endif // LOADCURTAIN_H
