#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H

#include "Views/videowidget.h"
#include "Main/global.h"
#include "Views/iconview.h"

//class VideoView : public QVideoWidget
class VideoView : public QWidget
{
    Q_OBJECT
public:
    explicit VideoView(QWidget *parent, IconView *thumbView);
    void load(QString fPath);
    void play();
    void pause();
    void stop();

signals:
    void togglePick();

protected:
    void wheelEvent(QWheelEvent *event) override;
    bool event(QEvent *event) override;

public slots:
    void durationChanged(qint64 duration);
    void positionChanged(qint64 progress);
    void scrubMoved(int ms);
    void scrubPressed();
    void playOrPause();

private:
    void updateDurationInfo(qint64 currentInfo);
    IconView *thumbView;
    VideoWidget *video;
    QToolButton *playPauseBtn;
    QSlider *scrub;
    QLabel *position;
    qint64 duration;
};

#endif // VIDEOVIEW_H
