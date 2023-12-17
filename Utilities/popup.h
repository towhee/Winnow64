#ifndef POPUP_H
#define POPUP_H

#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QGridLayout>
#include <QPropertyAnimation>
#include <QTimer>
//#include "Main/global.h"

class PopUp : public QWidget
{
    Q_OBJECT

    Q_PROPERTY(float popupOpacity READ getPopupOpacity WRITE setPopupOpacity)


public:
    explicit PopUp(QWidget *source, QWidget *centralWidget, QWidget *parent = 0);
    void setPopupOpacity(float opacity);
    float getPopupOpacity() const;
    void setPopupDuration(int msDuration);
    void setPopUpSize(int w, int h);
    void setPopupText(const QString& text); // Setting text notification
    void setPopupAlignment(Qt::Alignment);
    void setProgressVisible(bool isVisible);
    void setProgressMax(int maxProgress);
    void setProgress(int progress);
    QLabel label;
    QProgressBar progressBar;
    bool openAndNoTimeout = false;

protected:
    void paintEvent(QPaintEvent *event) override;    // The background will be drawn through the redraw method
    void focusOutEvent(QFocusEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;


public slots:
    void showPopup(const QString &text,
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
    };

#endif // POPUP_H
