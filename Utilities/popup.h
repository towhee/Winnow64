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

protected:
    void paintEvent(QPaintEvent *event);    // The background will be drawn through the redraw method

public slots:
    void show(const QString &text, int msDuration = 1000, bool isAutoSize = true, float opacity = 0.75);                            /* own widget displaying method
                                             * It is necessary to pre-animation settings
                                             * */
    void hide();                            /* At the end of the animation, it is checked in a given slot,
                                             * Does the widget visible, or to hide
                                             * */

private slots:
    void hideAnimation();                   // Slot start the animation hide

private:
    QLabel label;
    QGridLayout layout;
    QPropertyAnimation animation;
    float popupOpacity;
    QTimer *timer;
    int popupDuration = 2000;
    QWidget *source;
};

#endif // POPUP_H
