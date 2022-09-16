#ifndef VIDEOVIEW_H
#define VIDEOVIEW_H

#include "Views/videowidget.h"
#include "Main/global.h"
#include "Views/iconview.h"

class VideoView : public QWidget
{
    Q_OBJECT
public:
    explicit VideoView(QWidget *parent, IconView *thumbView);
    void load(QString fPath);
    void play();
    void pause();
    void stop();
    void updatePositionLabel();

signals:
    void togglePick();
    void handleDrop(QString fPath);

protected:
    void wheelEvent(QWheelEvent *event) override;
    bool event(QEvent *event) override;

public slots:
    void durationChanged(qint64 duration_ms);
    void positionChanged();
//    void positionChanged(qint64 progress_ms);
    void scrubMoved(int ms);
    void scrubPressed();
    void playOrPause();
    void relayDrop(QString fPath);

private:
    IconView *thumbView;
    VideoWidget *video;
    QToolButton *playPauseBtn;
    QSlider *scrub;
    QLabel *positionLabel;
    qint64 duration = 0;
    qint64 position = 0;
    QTimer *t;
};

#endif // VIDEOVIEW_H
