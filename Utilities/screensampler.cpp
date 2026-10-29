#include "Utilities/screensampler.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QCursor>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QHeaderView>
#include <QShortcut>
#include <QTableWidget>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>

#ifdef Q_OS_MAC
#include <CoreGraphics/CoreGraphics.h>
#endif

namespace {

constexpr int kGrabRadius = 8;      // logical points around the cursor, for the magnifier
constexpr int kBlock = 5;           // the sample: kBlock x kBlock physical pixels
constexpr int kLoupeCells = 11;     // physical pixels across the magnifier
constexpr int kLoupeZoom = 10;      // screen pixels per magnified pixel

QColor colorOf(const float rgb[3])
{
    return QColor(qBound(0, int(std::lround(rgb[0])), 255),
                  qBound(0, int(std::lround(rgb[1])), 255),
                  qBound(0, int(std::lround(rgb[2])), 255));
}

} // namespace

/*
    One per screen while armed: takes the clicks that would otherwise go to whichever app
    is under the cursor. Painted at alpha 1/255 -- fully transparent pixels let clicks
    through on macOS -- which shifts what it covers by at most one level, so the final
    sample is taken with it closed.
*/
class SamplerOverlay : public QWidget
{
public:
    SamplerOverlay(ScreenSampler *sampler, QScreen *screen)
        : QWidget(nullptr, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint
                               | Qt::NoDropShadowWindowHint),
          sampler(sampler)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setAttribute(Qt::WA_DeleteOnClose);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::CrossCursor);
        setGeometry(screen->geometry());
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(0, 0, 0, 1));
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        sampler->aim(e->globalPosition().toPoint());
    }
    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) sampler->pick(e->globalPosition().toPoint());
        else sampler->disarm();
    }
    void keyPressEvent(QKeyEvent *e) override
    {
        // Esc or Return ends the sampler
        if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return
            || e->key() == Qt::Key_Enter) sampler->close();
        else QWidget::keyPressEvent(e);
    }

private:
    ScreenSampler *sampler;
};

/*
    The magnifier that follows the cursor: kLoupeCells physical pixels across, the 5x5
    block that will be averaged outlined in the middle, and the live average under it.
    Transparent to the mouse and never activated, so it cannot take the click it is
    helping to aim, and it sits off the cursor so it is never inside its own grab.
*/
class SamplerLoupe : public QWidget
{
public:
    SamplerLoupe()
        : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint
                               | Qt::WindowStaysOnTopHint | Qt::NoDropShadowWindowHint)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_ShowWithoutActivating);
        const int side = kLoupeCells * kLoupeZoom;
        setFixedSize(side + 2, side + 2 + fontMetrics().height() * 2 + 8);
    }

    void setPatch(const QImage &im, QPoint centre, const float rgb[3])
    {
        patch = im;
        c = centre;
        for (int k = 0; k < 3; ++k) avg[k] = rgb[k];
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.fillRect(rect(), QColor(30, 30, 30));
        const int half = kLoupeCells / 2;
        for (int j = -half; j <= half; ++j)
            for (int i = -half; i <= half; ++i) {
                const int x = c.x() + i, y = c.y() + j;
                const QColor col = patch.valid(x, y) ? QColor(patch.pixel(x, y))
                                                     : QColor(30, 30, 30);
                p.fillRect(1 + (i + half) * kLoupeZoom, 1 + (j + half) * kLoupeZoom,
                           kLoupeZoom, kLoupeZoom, col);
            }
        /* The averaged block, in two contrasting strokes so it reads on any colour. */
        const int b0 = 1 + (half - kBlock / 2) * kLoupeZoom;
        const QRect box(b0, b0, kBlock * kLoupeZoom, kBlock * kLoupeZoom);
        p.setPen(QPen(Qt::black, 3));
        p.drawRect(box);
        p.setPen(QPen(Qt::white, 1));
        p.drawRect(box);

        const int side = kLoupeCells * kLoupeZoom + 2;
        p.setPen(QColor(225, 225, 225));
        float v[6];
        ScreenSampler::toTableValues(avg, v);
        auto n = [&v](int k) {
            const long r = std::lround(v[k]);
            return QString::number(k == 3 && r == 360 ? 0 : r);
        };
        p.drawText(QRect(4, side + 2, width() - 8, height() - side - 4),
                   Qt::AlignLeft | Qt::AlignVCenter,
                   QString("R %1  G %2  B %3\nH %4  S %5  L %6")
                       .arg(n(0), n(1), n(2), n(3), n(4), n(5)));
    }

private:
    QImage patch;
    QPoint c;
    float avg[3] = {0, 0, 0};
};

ScreenSampler::ScreenSampler(QWidget *parent)
    : QWidget(parent, Qt::Tool | Qt::WindowStaysOnTopHint)
{
    setWindowTitle("Screen Sampler");
    /* A Tool window hides when Winnow loses focus (macOS panels do), but the point is to
       look at another app's window beside these numbers. */
    setAttribute(Qt::WA_MacAlwaysShowToolWindow);

    status = new QLabel("Click Sample, then click anywhere on any screen.");
    status->setWordWrap(true);

    reason = new QLabel(
        "Winnow needs Screen Recording permission to read other apps' windows -- "
        "without it macOS shows the desktop in their place, and the numbers would be "
        "wrong. Turn Winnow on in System Settings > Privacy & Security > Screen "
        "Recording, then restart Winnow.");
    reason->setWordWrap(true);
    settingsBtn = new QPushButton("Open Settings");
    connect(settingsBtn, &QPushButton::clicked, this, [] {
        QDesktopServices::openUrl(QUrl("x-apple.systempreferences:com.apple.preference."
                                       "security?Privacy_ScreenCapture"));
    });

    /* One row per sample, oldest first so the numbering reads down. The Sample cell
       carries a chip of the colour. */
    table = new QTableWidget(0, 7);
    table->setHorizontalHeaderLabels({"Sample", "R", "G", "B", "H", "S", "L"});
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setIconSize(QSize(12, 12));
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->setToolTip("R G B in percent; H in degrees, S and L in percent.\n"
                      "Copy puts the table on the clipboard as text.  Esc or Return "
                      "closes the sampler.");

    /* Esc or Return closes the sampler. Window shortcuts, so they work wherever focus
       sits in this window. */
    for (int key : {int(Qt::Key_Escape), int(Qt::Key_Return), int(Qt::Key_Enter)}) {
        QShortcut *sc = new QShortcut(QKeySequence(key), this);
        sc->setContext(Qt::WindowShortcut);
        connect(sc, &QShortcut::activated, this, &QWidget::close);
    }

    sampleBtn = new QPushButton("Sample");
    sampleBtn->setToolTip("Pick points anywhere on screen, one sample per click.  "
                          "Right-click stops picking; Esc or Return closes the "
                          "sampler.");
    connect(sampleBtn, &QPushButton::clicked, this, &ScreenSampler::arm);
    QPushButton *copyBtn = new QPushButton("Copy");
    copyBtn->setToolTip("Copy the table to the clipboard as tab-separated text");
    connect(copyBtn, &QPushButton::clicked, this, &ScreenSampler::copyTable);
    QPushButton *clearBtn = new QPushButton("Clear");
    connect(clearBtn, &QPushButton::clicked, this, [this] {
        table->setRowCount(0);
        sampleCount = 0;
    });

    QHBoxLayout *btns = new QHBoxLayout;
    btns->addStretch(1);
    btns->addWidget(copyBtn);
    btns->addWidget(clearBtn);
    btns->addWidget(sampleBtn);
    QVBoxLayout *lay = new QVBoxLayout(this);
    lay->addWidget(status);
    lay->addWidget(reason);
    lay->addWidget(settingsBtn, 0, Qt::AlignLeft);
    lay->addWidget(table, 1);
    lay->addLayout(btns);
    showPermissionReason(false);
    resize(360, 360);

    loupe = new SamplerLoupe;
    aimTimer = new QTimer(this);
    aimTimer->setSingleShot(true);
    aimTimer->setInterval(33);
    connect(aimTimer, &QTimer::timeout, this, [this] {
        if (!armed) return;
        float rgb[3];
        QImage im;
        QPoint centre;
        if (!sampleAt(aimPos, rgb, &im, &centre)) { loupe->hide(); return; }
        loupe->setPatch(im, centre, rgb);
        /* Below-right of the cursor, flipped to stay on its screen. */
        QPoint at = aimPos + QPoint(24, 24);
        if (QScreen *scr = QGuiApplication::screenAt(aimPos)) {
            const QRect g = scr->availableGeometry();
            if (at.x() + loupe->width() > g.right()) at.setX(aimPos.x() - 24 - loupe->width());
            if (at.y() + loupe->height() > g.bottom())
                at.setY(aimPos.y() - 24 - loupe->height());
        }
        loupe->move(at);
        if (!loupe->isVisible()) loupe->show();
        loupe->raise();
    });
}

ScreenSampler::~ScreenSampler()
{
    for (const QPointer<QWidget> &o : overlays) if (o) o->close();
    delete loupe;
}

bool ScreenSampler::sampleAt(QPoint globalPos, float rgb[3], QImage *patch,
                             QPoint *patchCentre)
{
    QScreen *scr = QGuiApplication::screenAt(globalPos);
    if (!scr) return false;
    /* Grab a little square around the point, clipped to the screen (grabWindow is in
       the screen's own logical coordinates), and find the cursor's PHYSICAL pixel in it
       -- the grab comes back at the display's device pixel ratio. */
    const QPoint local = globalPos - scr->geometry().topLeft();
    const QRect want(local.x() - kGrabRadius, local.y() - kGrabRadius,
                     2 * kGrabRadius + 1, 2 * kGrabRadius + 1);
    const QRect r = want.intersected(QRect(QPoint(0, 0), scr->geometry().size()));
    if (r.isEmpty()) return false;
    const QPixmap pm = scr->grabWindow(0, r.x(), r.y(), r.width(), r.height());
    if (pm.isNull()) return false;
    const QImage im = pm.toImage().convertToFormat(QImage::Format_RGB32);
    if (im.isNull() || im.width() < 1) return false;
    const double dpr = double(im.width()) / r.width();
    const QPoint centre(qBound(0, int(std::floor((local.x() - r.x()) * dpr)), im.width() - 1),
                        qBound(0, int(std::floor((local.y() - r.y()) * dpr)), im.height() - 1));

    double sum[3] = {0, 0, 0};
    int n = 0;
    for (int j = -kBlock / 2; j <= kBlock / 2; ++j)
        for (int i = -kBlock / 2; i <= kBlock / 2; ++i) {
            const int x = centre.x() + i, y = centre.y() + j;
            if (!im.valid(x, y)) continue;
            const QRgb px = im.pixel(x, y);
            sum[0] += qRed(px);
            sum[1] += qGreen(px);
            sum[2] += qBlue(px);
            ++n;
        }
    if (!n) return false;
    for (int k = 0; k < 3; ++k) rgb[k] = float(sum[k] / n);
    if (patch) *patch = im;
    if (patchCentre) *patchCentre = centre;
    return true;
}

void ScreenSampler::arm()
{
    show();
    raise();
    if (armed) return;
    if (!havePermission(true)) {
        showPermissionReason(true);
        return;
    }
    showPermissionReason(false);
    for (QScreen *scr : QGuiApplication::screens()) {
        SamplerOverlay *o = new SamplerOverlay(this, scr);
        overlays.append(o);
        o->show();
    }
    focusOverlay();
    armed = true;
    sampleBtn->setEnabled(false);
    status->setText("Click anywhere to sample a 5x5 average; each click adds a sample.  "
                    "Right-click stops, Esc or Return closes the sampler.");
    emit armedChanged(true);
    aim(QCursor::pos());
}

void ScreenSampler::focusOverlay()
{
    /* Keys (Esc, Return) go to the overlay under the cursor. */
    if (QScreen *here = QGuiApplication::screenAt(QCursor::pos()))
        for (const QPointer<QWidget> &o : overlays)
            if (o && o->geometry() == here->geometry()) {
                o->raise();
                o->activateWindow();
                o->setFocus();
            }
}

void ScreenSampler::disarm()
{
    for (const QPointer<QWidget> &o : overlays) if (o) o->close();
    overlays.clear();
    aimTimer->stop();
    loupe->hide();
    if (!armed) return;
    armed = false;
    sampleBtn->setEnabled(true);
    if (status->text().startsWith("Click anywhere")) status->clear();
    emit armedChanged(false);
}

void ScreenSampler::toggle()
{
    if (armed) disarm();
    else arm();
}

void ScreenSampler::aim(QPoint globalPos)
{
    aimPos = globalPos;
    if (!aimTimer->isActive()) aimTimer->start();
}

void ScreenSampler::pick(QPoint globalPos)
{
    /* Hide the overlay and the magnifier BEFORE the grab -- the overlay tints what it
       covers -- and give the window server a moment to take them off screen. The
       sampler stays armed: takeSample puts them back for the next click. */
    pendingPos = globalPos;
    aimTimer->stop();
    loupe->hide();
    for (const QPointer<QWidget> &o : overlays) if (o) o->hide();
    QTimer::singleShot(80, this, &ScreenSampler::takeSample);
}

void ScreenSampler::takeSample()
{
    float rgb[3];
    if (sampleAt(pendingPos, rgb)) addSample(rgb);
    else status->setText("Could not read the screen there.");
    show();
    /* Still armed (not stopped while the overlay was down): resume aiming. */
    if (!armed) return;
    for (const QPointer<QWidget> &o : overlays) if (o) o->show();
    focusOverlay();
    aim(QCursor::pos());
}

void ScreenSampler::toTableValues(const float rgb[3], float out[6])
{
    const float r = rgb[0] / 255.0f, g = rgb[1] / 255.0f, b = rgb[2] / 255.0f;
    const float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    const float l = 0.5f * (mx + mn);
    const float d = mx - mn;
    float h = 0.0f, sat = 0.0f;
    if (d > 1e-6f) {
        sat = d / (1.0f - std::fabs(2.0f * l - 1.0f));
        if (mx == r)      h = 60.0f * std::fmod((g - b) / d, 6.0f);
        else if (mx == g) h = 60.0f * ((b - r) / d + 2.0f);
        else              h = 60.0f * ((r - g) / d + 4.0f);
        if (h < 0.0f) h += 360.0f;
    }
    out[0] = 100.0f * r; out[1] = 100.0f * g; out[2] = 100.0f * b;
    out[3] = h; out[4] = 100.0f * sat; out[5] = 100.0f * l;
}

void ScreenSampler::addSample(const float rgb[3])
{
    float v[6];
    toTableValues(rgb, v);
    QPixmap chip(12, 12);
    chip.fill(colorOf(rgb));

    const int row = table->rowCount();
    table->insertRow(row);
    QTableWidgetItem *id = new QTableWidgetItem(QIcon(chip), QString::number(++sampleCount));
    table->setItem(row, 0, id);
    for (int k = 0; k < 6; ++k) {
        /* 360 degrees is 0: a rounded hue of 359.6 must not read as 360. */
        int n = int(std::lround(v[k]));
        if (k == 3 && n == 360) n = 0;
        QTableWidgetItem *it = new QTableWidgetItem(QString::number(n));
        it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        table->setItem(row, k + 1, it);
    }
    table->scrollToBottom();

    /* Every reading also goes straight to the clipboard, in Copy's format (header plus
       this one row), so a single sample can be pasted without opening the table. */
    QApplication::clipboard()->setText(headerText() + '\n' + rowText(row) + '\n');
    status->setText(QString("Sample %1 taken and copied to the clipboard.")
                        .arg(sampleCount));
}

QString ScreenSampler::headerText() const
{
    QStringList head;
    for (int c = 0; c < table->columnCount(); ++c)
        head << table->horizontalHeaderItem(c)->text();
    return head.join('\t');
}

QString ScreenSampler::rowText(int row) const
{
    QStringList cells;
    for (int c = 0; c < table->columnCount(); ++c)
        cells << (table->item(row, c) ? table->item(row, c)->text() : QString());
    return cells.join('\t');
}

void ScreenSampler::copyTable()
{
    /* Tab-separated, header first: pastes as columns into a spreadsheet and lines up
       in a plain-text note. */
    QStringList lines;
    lines << headerText();
    for (int r = 0; r < table->rowCount(); ++r) lines << rowText(r);
    QApplication::clipboard()->setText(lines.join('\n') + '\n');
    status->setText(QString("Copied %1 sample%2 to the clipboard.")
                        .arg(table->rowCount()).arg(table->rowCount() == 1 ? "" : "s"));
}

bool ScreenSampler::havePermission(bool request)
{
#ifdef Q_OS_MAC
    if (CGPreflightScreenCaptureAccess()) return true;
    /* Shows the system prompt the first time; after that macOS only remembers the
       answer, and a grant takes effect at the next launch. */
    if (request) CGRequestScreenCaptureAccess();
    return false;
#else
    Q_UNUSED(request)
    return true;
#endif
}

void ScreenSampler::showPermissionReason(bool show)
{
    reason->setVisible(show);
    settingsBtn->setVisible(show);
    if (show) status->setText("Sampling is off until permission is granted.");
}

void ScreenSampler::closeEvent(QCloseEvent *event)
{
    disarm();
    QWidget::closeEvent(event);
}
