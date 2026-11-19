#ifndef EDITPROGRESS_H
#define EDITPROGRESS_H

#include <QElapsedTimer>
#include <QString>

class Progress;
class QWidget;

/*
    FEEDBACK FOR A METADATA EDIT THAT TAKES MORE THAN A SECOND.

    A rating, colour class, keyword or text-field edit is a synchronous GUI-thread loop
    over the selection, writing each image's sidecar (Metadata::writeXMP) and its catalog
    row. One image is instant; a few thousand on a slow or network drive is many seconds
    with the window frozen and nothing to say why. EditProgress is that loop's status bar
    row ("Saving metadata", MW::progressEditRow): it stays out of sight for a short edit,
    and appears once the edit has run a second or is projected to.

        EditProgress ep("Rating", rows.size());
        for (int i = 0; i < n; ++i) { ...write...; ep.step(i + 1); }
        ep.phase("Updating the filters");       // a slow tail after the loop
        // the destructor clears the row

    IT PAINTS ITSELF. Nothing in these loops turns the event loop, so a row that was only
    updated would sit unpainted until the edit finished. The first show activates the
    status bar's and the window's layouts and repaints the status bar (the row has just
    grown it); later steps repaint the Progress widget alone, at most ten times a second.
    On macOS a repaint does not reach the screen without a Core Animation commit, so each
    paint ends with [CATransaction flush] -- see Popup::paintNow for that story. Neither
    delivers the main window's pending events, so the views still repaint once, after.

    THE STATUS BAR HIDDEN: the same progress goes to G::popup instead, unless a popup is
    already up (an operation such as a keyword move that has its own).

    NESTED EDITS ARE INERT: only the outermost EditProgress reports, so a caller driving
    several passes owns one bar rather than a flicker per pass.

    GUI thread only. install() is called once by MW after the row exists.
*/
class EditProgress
{
public:
    static void install(Progress *progress, int rowId, QWidget *statusBar);

    /*  what names the edit in the tooltip ("Rating", "Colour class", "Keywords").
        total is the number of steps the caller will report. */
    EditProgress(const QString &what, int total);
    ~EditProgress();
    EditProgress(const EditProgress &) = delete;
    EditProgress &operator=(const EditProgress &) = delete;

    void step(int done);                    // done of total images finished
    void phase(const QString &text);        // a named stage after the loop

private:
    enum class Target { None, StatusBar, Popup };
    bool shouldShow(int done) const;
    void show();
    void paint(bool force);
    QString tip() const;

    static Progress *progress;
    static int rowId;
    static QWidget *statusBar;
    static int depth;                       // nesting: only depth 1 reports

    QString what;
    QString phaseText;
    int total = 0;
    int done = 0;
    bool outermost = false;
    Target target = Target::None;
    QElapsedTimer clock;                    // since construction
    QElapsedTimer paintClock;               // paint throttle
};

#endif // EDITPROGRESS_H
