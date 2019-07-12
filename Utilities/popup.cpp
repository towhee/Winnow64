#include "Utilities/popup.h"
#include <QPainter>
#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>

/*
This class shows a pop up message for a given time with an assigned transparency.  The
default time is 1000 ms and the default transparency is 75%.

The widget will autosize to fit the text message if isAutoSize is true.

If there are a series of progress messages (example loading filters) then use the first
message to set the size and then set isAutoSize = false and setPopupText to change the
text while keeping the pop up box size constant.

In Winnow an instance of this class is created in global so that it is available to all
parts of the program.  It is created once in MW::initialize.
*/

PopUp::PopUp(QWidget *source, QWidget *parent) : QWidget(parent)
{
    this->source = source;
    setWindowFlags(Qt::FramelessWindowHint |        // Disable window decoration
                   Qt::Tool |                       // Discard display in a separate window
                   Qt::WindowStaysOnTopHint);       // Set on top of all windows
    setAttribute(Qt::WA_TranslucentBackground);     // Indicates that the background will be transparent
    setAttribute(Qt::WA_ShowWithoutActivating);     // At the show, the widget does not get the focus automatically

    animation.setTargetObject(this);                // Set the target animation
    animation.setPropertyName("popupOpacity");      //
    connect(&animation, &QAbstractAnimation::finished, this, &PopUp::hide);

//    label.setAlignment(Qt::AlignRight | Qt::AlignVCenter);
//    label.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    label.setTextFormat(Qt::RichText);
    label.setStyleSheet("QLabel { color : white; "
                        "font-size: 16px;"
                        "margin-top: 10px;"
                        "margin-bottom: 10px;"
                        "margin-left: 10px;"
                        "margin-right: 10px; }");

    layout.addWidget(&label, 0, 0);
    setLayout(&layout);

    timer = new QTimer();
    connect(timer, &QTimer::timeout, this, &PopUp::hide);
//    connect(timer, &QTimer::timeout, this, &PopUp::hideAnimation);
}

void PopUp::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QRect roundedRect;
    roundedRect.setX(rect().x() + 5);
    roundedRect.setY(rect().y() + 5);
    roundedRect.setWidth(rect().width() - 10);
    roundedRect.setHeight(rect().height() - 10);

    painter.setBrush(QBrush(QColor(0,0,0,180)));
    painter.setPen(Qt::NoPen);

    painter.drawRoundedRect(roundedRect, 10, 10);
}

void PopUp::keyReleaseEvent(QKeyEvent *event)
{
    qDebug() << __FUNCTION__ << event;
    QWidget::keyReleaseEvent(event);
}

void PopUp::focusOutEvent(QFocusEvent *event)
{
    qDebug() << __FUNCTION__ << event;
}

void PopUp::showPopup(const QString &text,
                 int msDuration,
                 bool isAutoSize,
                 float opacity,
                 Qt::Alignment alignment)
{
    timer->stop();

    popupDuration = msDuration;
    popupOpacity = opacity;
    popupAlignment = alignment;
    popupText = text;
    qDebug() << __FUNCTION__
             << "popupDuration =" << popupDuration
             << alignment;
    label.setAlignment(alignment/* | Qt::AlignVCenter*/);
    label.setText(popupText);
    if (isAutoSize) adjustSize();               // With the recalculation notice sizes

//    if (popupDuration > 0) {
//        setWindowOpacity(0.0);                  // Set the transparency to zero
//        animation.setDuration(150);             // Configuring the duration of the animation
//        animation.setStartValue(0.0);           // The start value is 0 (fully transparent widget)
//        animation.setEndValue(popupOpacity);    // End - completely opaque widget
//    }
//    else setWindowOpacity(popupOpacity);
    setWindowOpacity(popupOpacity);

    int pW = width();
    int pH = height();
    int sX = source->geometry().x();
    int sY = source->geometry().y();
    int sW = source->geometry().width();
    int sH = source->geometry().height();
    int sCtrX = sX + sW / 2;
    int sCtrY = sY + sH / 2;
    int pX = sCtrX - pW / 2;
    int pY = sCtrY - pH / 2;

    setGeometry(pX, pY, pW, pH);

    QWidget::show();

//    if (popupDuration > 0) animation.start();

//    // set popupDuration = 0 to keep open and manually close like a msgbox
    if (popupDuration > 0) timer->start(popupDuration);

    qApp->processEvents();
}

void PopUp::hideAnimation()
{
    timer->stop();
//    animation.setDuration(500);
//    animation.setStartValue(popupOpacity);
//    animation.setEndValue(0.0);
//    animation.start();
    popupOpacity = 0.0;
    hide();
}

void PopUp::hide()
{
    QWidget::hide();
    // If the widget is transparent, then hide it
//    if (popupOpacity == 0.0) {
//        QWidget::hide();
//    }
}

void PopUp::setPopupAlignment(Qt::Alignment alignment)
{
    label.setAlignment(alignment/* | Qt::AlignVCenter*/);
}

void PopUp::setPopupText(const QString &text)
{
    label.setText(text);
    repaint(0, 0, width(), height());
    qApp->processEvents();
}

void PopUp::setPopupOpacity(float opacity)
{
    popupOpacity = opacity;
    setWindowOpacity(opacity);
}

float PopUp::getPopupOpacity() const
{
    return popupOpacity;
}

void PopUp::setPopupDuration(int msDuration)
{
    popupDuration = msDuration;
}

void PopUp::setPopUpSize(int w, int h)
{
//    setFixedSize(w, h);
    setGeometry(0, 0, w, h);
    qApp->processEvents();
}
