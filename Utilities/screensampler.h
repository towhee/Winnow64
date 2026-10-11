#ifndef SCREENSAMPLER_H
#define SCREENSAMPLER_H

#include <QWidget>
#include <QColor>
#include <QImage>
#include <QList>
#include <QPointer>

class QLabel;
class QTableWidget;
class QPushButton;
class QTimer;
class SamplerOverlay;
class SamplerLoupe;

/*
    SCREEN SAMPLER -- a 5x5 colour average from ANYWHERE on screen, not just in Winnow.

    WHAT IT IS FOR: comparing renders across apps. Sample a spot in Lightroom's window,
    then the same spot in Winnow's, and the two readings sit side by side in the
    table (RGB as percentages, HSL as degrees and percentages; the Copy button
    puts it on the clipboard as tab-separated text, Esc closes the sampler). The in-image droppers (Color Range,
    white balance) read Winnow's own pixels; this one reads what the DISPLAY shows,
    whoever drew it.

    HOW IT AIMS. A click on another app's window would go to that app, so while armed a
    near-invisible overlay (alpha 1/255 -- macOS passes clicks through fully transparent
    pixels) covers every screen and takes the clicks, and a magnifier follows the cursor
    showing the pixels around it with the 5x5 block outlined. The overlay tints what it
    covers by at most one level, so the FINAL sample is taken with overlay and magnifier
    hidden; the live preview tolerates it.

    WHAT IT READS: the display's own values, 0..255, averaged over 5x5 PHYSICAL pixels (on
    a 2x screen that is 2.5x2.5 logical points). They are in the display's colour space
    (Display P3 on most Macs), not sRGB -- two windows on one display compare exactly,
    which is the point; a number copied out is not an sRGB value.

    macOS NEEDS SCREEN RECORDING PERMISSION to read other apps' windows; without it the
    system returns the wallpaper in their place. The sampler checks first and, when it is
    missing, says so INSIDE its window with a button to the right Settings pane rather
    than returning wrong numbers. Granting it takes effect after Winnow restarts. Run from
    Qt Creator, the permission belongs to Qt Creator, which launched the process.

    Windows needs no permission. Both platforms grab through QScreen::grabWindow.
*/
class ScreenSampler : public QWidget
{
    Q_OBJECT
public:
    explicit ScreenSampler(QWidget *parent = nullptr);
    ~ScreenSampler() override;

    bool isArmed() const { return armed; }

    /* The 5x5 average around globalPos (logical coordinates) as 0..255 floats, and the
       grab it came from. False when there is no screen there or the grab failed. */
    static bool sampleAt(QPoint globalPos, float rgb[3], QImage *patch = nullptr,
                         QPoint *patchCentre = nullptr);

    /* rgb 0..255 -> {R%, G%, B%, H deg, S%, L%}, the conventional HSL. Shared by the
       table and the magnifier so the two always agree. */
    static void toTableValues(const float rgb[3], float out[6]);

public slots:
    void arm();
    void disarm();
    void toggle();

signals:
    void armedChanged(bool armed);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    friend class SamplerOverlay;
    void aim(QPoint globalPos);         // overlay: cursor moved
    void pick(QPoint globalPos);        // overlay: left click
    void takeSample();                  // after the overlay has gone
    void addSample(const float rgb[3]);
    void copyTable();
    bool havePermission(bool request);
    void showPermissionReason(bool show);

    bool armed = false;
    QPoint pendingPos;
    QList<QPointer<QWidget>> overlays;  // SamplerOverlay, one per screen
    SamplerLoupe *loupe = nullptr;
    QTimer *aimTimer = nullptr;         // coalesces mouse moves into ~30 grabs/s
    QPoint aimPos;

    QLabel *status = nullptr;
    QLabel *reason = nullptr;
    QPushButton *settingsBtn = nullptr;
    QPushButton *sampleBtn = nullptr;
    QTableWidget *table = nullptr;
    int sampleCount = 0;
};

#endif // SCREENSAMPLER_H
