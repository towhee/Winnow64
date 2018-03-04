#include "Utilities/popup.h"
#include <QPainter>
#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>

PopUp::PopUp(QWidget *parent) : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint |        // Disable window decoration
                   Qt::Tool |                       // Discard display in a separate window
                   Qt::WindowStaysOnTopHint);       // Set on top of all windows
    setAttribute(Qt::WA_TranslucentBackground);     // Indicates that the background will be transparent
    setAttribute(Qt::WA_ShowWithoutActivating);     // At the show, the widget does not get the focus automatically

    animation.setTargetObject(this);                // Set the target animation
    animation.setPropertyName("popupOpacity");      //
    connect(&animation, &QAbstractAnimation::finished, this, &PopUp::hide);

    label.setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
    label.setStyleSheet("QLabel { color : white; "
                        "font-size: 20px;"
                        "margin-top: 6px;"
                        "margin-bottom: 6px;"
                        "margin-left: 10px;"
                        "margin-right: 10px; }");

    layout.addWidget(&label, 0, 0);
    setLayout(&layout);

    timer = new QTimer();
    connect(timer, &QTimer::timeout, this, &PopUp::hideAnimation);
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

void PopUp::setPopupText(const QString &text)
{
    label.setText(text);    // Set the text in the Label
    adjustSize();           // With the recalculation notice sizes
}

void PopUp::show()
{
    if (popupDuration > 0) {
        setWindowOpacity(0.0);                  // Set the transparency to zero
        animation.setDuration(150);             // Configuring the duration of the animation
        animation.setStartValue(0.0);           // The start value is 0 (fully transparent widget)
        animation.setEndValue(popupOpacity);    // End - completely opaque widget
    }
    else setWindowOpacity(popupOpacity);

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

    if (popupDuration > 0) animation.start();
//    animation.start();

    // set popupDuration = 0 to keep open and manually close like a msgbox
    if (popupDuration > 0) timer->start(popupDuration);
}

void PopUp::hideAnimation()
{
    timer->stop();
    animation.setDuration(500);
    animation.setStartValue(popupOpacity);
    animation.setEndValue(0.0);
    animation.start();
}

void PopUp::hide()
{
    // If the widget is transparent, then hide it
    if(getPopupOpacity() == 0.0){
        QWidget::hide();
    }
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

void PopUp::showPopup(QWidget *widget, const QString &text, int msDuration, float opacity)
{
    popupDuration = msDuration;
    popupOpacity = opacity;
    source = widget;
    label.setText(text);    // Set the text in the Label
    adjustSize();           // With the recalculation notice sizes
    show();
}
