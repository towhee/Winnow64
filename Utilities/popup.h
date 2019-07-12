#ifndef POPUP_H
#define POPUP_H

#include <QWidget>
#include <QLabel>
#include <QGridLayout>
#include <QPropertyAnimation>
#include <QTimer>

class PopUp : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(float popupOpacity READ getPopupOpacity WRITE setPopupOpacity)


public:
    explicit PopUp(QWidget *source, QWidget *parent = 0);
    void setPopupOpacity(float opacity);
    float getPopupOpacity() const;
    void setPopupDuration(int msDuration);
    void setPopUpSize(int w, int h);
    void setPopupText(const QString& text); // Setting text notification
    void setPopupAlignment(Qt::Alignment);
    QLabel label;

protected:
    void paintEvent(QPaintEvent *event);    // The background will be drawn through the redraw method
    void focusOutEvent(QFocusEvent *event);
    void keyReleaseEvent(QKeyEvent *event);


public slots:
    void showPopup(const QString &text,
              int msDuration = 1000,
              bool isAutoSize = true,
              float opacity = 0.75,
              Qt::Alignment alignment = Qt::AlignHCenter);                            /* own widget displaying method
                                             * It is necessary to pre-animation settings
                                             * */
    void hide();                            /* At the end of the animation, it is checked in a given slot,
                                             * Does the widget visible, or to hide
                                             * */

private slots:
    void hideAnimation();                   // Slot start the animation hide

private:
    QGridLayout layout;
    QPropertyAnimation animation;
    float popupOpacity;
    Qt::Alignment popupAlignment;
    QTimer *timer;
    int popupDuration = 2000;
    QString popupText;
    QWidget *source;
};

#endif // POPUP_H
