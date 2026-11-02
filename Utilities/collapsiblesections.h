#ifndef COLLAPSIBLESECTIONS_H
#define COLLAPSIBLESECTIONS_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QVector>
#include <functional>

class QBoxLayout;
class QGroupBox;
class QLabel;
class QToolButton;
class QWidget;

/*
    Collapsible QGroupBox sections for a dialog that is too tall for a small screen
    (ExportDlg, IngestDlg).

    OPEN, a section is its group box with a chevron (U+25BE) in the title, and the title
    is clickable: hovering it shows a pointing hand and "Click to collapse", with the
    right-click hint below it (the closed title says "Click to expand"). CLOSED, the
    box is hidden and a one-line row stands in for it, inserted just after it in the
    same layout: the chevron (U+25B8) + title as a button, then a grey summary of the
    section's settings from the caller's Summary function. A short screen loses the box,
    not the information. Right-click a title, open or closed, for Expand all / Collapse
    all.

    COLLAPSED BY DEFAULT. Each section's state is saved under <settingsGroup>/<key> in
    G::settings; a key with nothing saved yet reads as collapsed, so a first open shows
    every section closed -- the dialog's buttons cannot end up off a small screen. After
    that each section opens as the user last left it.

    The closed row starts at the box's left edge (the dialog's left margin), and every
    closed title sits left-aligned in a cell as wide as the longest title, so the
    summaries form a column. restore() sizes those cells, so call it once all sections
    are added.

    onChanged runs after a USER toggle (a title click, Expand / Collapse all): the dialog
    re-fits its height and syncs any Expand All / Collapse All buttons there. restore()
    and setOpen(..., save = false) do not call it.

    No _Q_OBJECT: it declares no signals or slots (lambdas connect its buttons), so it
    needs no moc. The title hit test is the band above the box's first child (or its
    layout's top margin) as far as the title text runs -- not the style's label rect,
    which widgetcss shifts with relative offsets.
*/
class CollapsibleSections : public QObject
{
public:
    using Summary = std::function<QString()>;

    CollapsibleSections(QWidget *owner, const QString &settingsGroup);

    /* Make box collapsible. box must already be in lay; its closed row is inserted just
       after it. The box's current title is the section's title. Returns its index. */
    int add(QGroupBox *box, QBoxLayout *lay, const QString &key, Summary summary);
    /* Apply the saved states (collapsed by default) and line up the closed titles. */
    void restore();

    int  count() const { return int(sections.size()); }
    int  indexOf(const QString &key) const;
    bool isOpen(int i) const;
    bool anyOpen() const;
    bool anyClosed() const;
    /* save == false: show it, but do not remember it or call onChanged. */
    void setOpen(int i, bool open, bool save = true);
    void setAllOpen(bool open);
    void updateSummaries();

    std::function<void()> onChanged;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    struct Section {
        QString      key;
        QString      title;
        QGroupBox   *box = nullptr;
        QWidget     *closedRow = nullptr;
        QWidget     *titleCell = nullptr;
        QToolButton *closedBtn = nullptr;
        QLabel      *summaryLab = nullptr;
        Summary      summary;
        bool         open = true;
    };
    void apply(int i);
    void menu(const QPoint &globalPos);
    static bool onTitle(QGroupBox *box, const QPoint &pos);

    QWidget *owner;
    QString settingsGroup;
    QVector<Section> sections;
};

#endif // COLLAPSIBLESECTIONS_H
