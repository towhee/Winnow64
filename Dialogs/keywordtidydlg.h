#ifndef KEYWORDTIDYDLG_H
#define KEYWORDTIDYDLG_H

#include <QDialog>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include "Cache/catalog.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

/*
    What the tidy will do to ONE keyword -- a flat name, or a whole unfiled hierarchical
    path. Empty target with remove = false is a row the user left alone; nothing is
    written for it.
*/
struct KeywordTidyAction
{
    QString from;       // the keyword path as the catalog spells it
    QString target;     // the path it becomes (Append: already joined); empty = removing
    bool remove = false;
};

/*
    TIDY KEYWORDS -- filing a legacy flat vocabulary, and stray branches, into the
    hierarchy.

    THE PROBLEM IT SOLVES. A library that predates hierarchical keywords, or one imported
    from an application that never wrote lr:hierarchicalSubject, carries its keywords as
    bare names: "Nanaimo", not "Location|Canada|BC|Vancouver Island|Nanaimo". Winnow
    reads both -- a flat list is a tree of depth one -- so nothing is broken, but the
    vocabulary tree grows a thousand roots beside the branches they belong in, and the
    same place is two keywords depending on which application wrote it.

    WHAT A FLAT KEYWORD IS HERE. Not "a keyword row with no separator": the index holds a
    row for every ancestor PREFIX, so "Location" exists as the parent of Location|Canada
    whether or not any file says it. A flat keyword is one an image carries as a ROOT
    after leaf consumption -- see Catalog::flatKeywords.

    AND THE UNFILED BRANCHES. A hierarchical path an image carries that the keyword list
    does not hold -- "Birds|Robin" from another application, where the list files Robin
    as "Fauna|Bird|Robin" -- is the same problem one level up, and is reviewed in the same
    table. It is matched on its LEAF like a flat name, with candidates that share more of
    its parent names ranked first, and it is replaced WHOLE: a row is one exact path.
    UNLIKE A FLAT NAME, ONE THAT MATCHES NOTHING DEFAULTS TO LEAVE ALONE: a bare
    "Location" is a container name with nothing to lose, but "Birds|Robin|Juvenile" is
    information.

    IT IS A REVIEW LIST, NOT A BUTTON. On a real library this finds around a thousand
    names, and roughly a fifth of them match SEVERAL branches ("BC" is under both
    Canada|BC and Location|Canada|BC) -- the very ambiguity that path identity exists to
    expose. Applying a guess to a fifth of a library's keywords, in the files themselves,
    is not a thing to do silently, so every row shows what it matched and the choice is
    editable. The deepest match is pre-selected and the row is MARKED FOR REVIEW when
    there was more than one.

    A NAME THAT MATCHES NOTHING DEFAULTS TO REMOVE, and those are the container names --
    "Location", "Fauna", "Category" -- that Lightroom writes when "export containing
    keywords" is on. Under prefix expansion an image tagged Location|Canada is already
    found by a search for "Location", so a bare "Location" beside it is the same fact
    spelled without its parents rather than a second one. Keep is one click away per row.

    CATALOG-WIDE, DELIBERATELY. A flat keyword filed in one folder and left flat in
    another is the same keyword in two places, which is the state this exists to end.
    There is no "this folder only".
*/
class KeywordTidyDlg : public QDialog
{
    Q_OBJECT

public:
    /*  keywords comes from Catalog::tidyKeywords (biggest first); vocabPaths is every path in
        the authored vocabulary, which supplies both the candidate matches and the
        type-ahead for choosing a branch by hand. pathCounts is how many images each
        vocabulary path already holds, folded, which is what makes a review row decidable:
        "Animal" under Fauna|Animal (257 images) or under Fauna|Bird|Arctic Ground
        Squirrel|Animal (1) is not a coin toss once the numbers are on screen. */
    KeywordTidyDlg(const QList<CatalogKeyword> &keywords, const QStringList &vocabPaths,
                   const QHash<QString, int> &pathCounts, QWidget *parent = nullptr);

    /*  The rows the user did not skip. Empty when there is nothing to do, which the
        caller can treat as a cancel. */
    QList<KeywordTidyAction> plan() const;

private:
    void addRow(const CatalogKeyword &k);
    /*  Vocabulary paths whose LEAF is this keyword's leaf, best first. For a flat name
        they are depth 2 or more (a root is not somewhere to move a root); a hierarchical
        path may also land on a root. The pre-selection and the row's combo come from the
        same list, so what the row says it matched is what it offers. */
    QStringList candidatesFor(const QString &path) const;
    void applyFilter();
    /*  The rows a bulk button acts on: the SELECTED rows when anything is selected, and
        every row SHOWN otherwise. Both readings of "do this to these" are live at once
        in a table with a filter box and a selection, and answering only one of them is
        what made the first version of this dialog unreadable -- the buttons acted on the
        filtered set while the selection, the thing the user had just made, did nothing.
        The hint beside the buttons always names which it is. */
    QList<int> targetRows() const;
    void setRows(const QList<int> &rows, int action);
    /*  "Move / Remove / Leave alone act on N rows shown" -- kept live, because it is the
        answer to the question this dialog kept raising. */
    void updateScopeHint();
    void updateSummary();

    QTableWidget *table = nullptr;
    QLineEdit *filterEdit = nullptr;
    QComboBox *showCombo = nullptr;
    QLabel *scopeHint = nullptr;
    QLabel *summary = nullptr;
    QPushButton *applyBtn = nullptr;

    /*  Set while a bulk button rewrites many rows, so the per-item handler does its own
        row's repaint and skips the whole-table summary until the end. Without it,
        "Remove all shown" is a thousand row edits each recounting a thousand rows. */
    bool bulkEdit = false;

    QHash<QString, int> counts;              // folded vocabulary path -> images
    QStringList allPaths;                    // every vocabulary path, for the completer
    QHash<QString, QStringList> byLeafFold;  // folded leaf -> paths with that leaf
    QHash<QString, QString> rootByFold;      // folded root name -> root path
};

#endif // KEYWORDTIDYDLG_H
