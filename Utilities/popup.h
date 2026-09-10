#ifndef POPUP_H
#define POPUP_H

#include <QWidget>
#include <QElapsedTimer>
#include <QLabel>
#include <QProgressBar>
#include <QGridLayout>
#include <QPropertyAnimation>
#include <QTimer>

class Popup : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(float popupOpacity READ getPopupOpacity WRITE setPopupOpacity)

public:
    explicit Popup(QWidget *source, QWidget *centralWidget, QWidget *parent = nullptr);
    void setPopupOpacity(float opacity);
    float getPopupOpacity() const;
    void setPopupDuration(int msDuration);
    void setPopUpSize(int w, int h);
    void setPopupText(const QString& text); // Setting text notification
    void setPopupAlignment(Qt::Alignment);
    void setProgressVisible(bool isVisible);
    void setProgressMax(int maxProgress);
    void setProgress(int progress);

    /*  THE GUI THREAD'S OWN ROUTE TO THE SCREEN. showPopup defers through a zero timer;
        these two do the work now, so a synchronous loop does not have to pump the whole
        application to be seen. See the definitions. */
    void showPopupNow(const QString &text,
                      int msDuration = 1500,
                      bool isAutoSize = true,
                      float opacity = 0.75,
                      Qt::Alignment alignment = Qt::AlignHCenter);
    void pulse(int minIntervalMs = 100);
    QLabel label;
    QProgressBar progressBar;
    bool openAndNoTimeout = false;

protected:
    void paintEvent(QPaintEvent *event) override;    // The background will be drawn through the redraw method

public slots:
    void showPopup(const QString &text,
                   int msDuration = 1500,
                   bool isAutoSize = true,
                   float opacity = 0.75,
                   Qt::Alignment alignment = Qt::AlignHCenter);
    void showPopup1(const QString &text,
              int msDuration = 1500,
              bool isAutoSize = true,
              float opacity = 0.75,
              Qt::Alignment alignment = Qt::AlignHCenter);
    void reset();

private slots:
    void hide();

private:
    QWidget *centralWidget;
    QGridLayout layout;
    bool okayToHide = true;
    float popupOpacity;
    Qt::Alignment popupAlignment;
    QTimer *hideTimer;
    int popupDuration = 2000;
    bool isProgressBar = false;
    QString popupText;
    QWidget *source;
    QElapsedTimer pulseClock;
};

#endif // POPUP_H
