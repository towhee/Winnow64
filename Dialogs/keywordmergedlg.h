#ifndef KEYWORDMERGEDLG_H
#define KEYWORDMERGEDLG_H

#include <QDialog>
#include <QList>
#include <QString>
#include <QStringList>

class QLabel;
class QListWidget;

/*
    ONE PLACE IN THE KEYWORD LIST THE UNFILED BRANCH COULD GO. images is how many
    photographs are already filed under it, which is what tells two same-named branches
    apart when nothing else does -- "Location|New Zealand" with 812 images and
    "Trips|New Zealand" with 4 are not a hard choice once the numbers are on screen.

    createsNodes is what CHOOSING THIS ONE would add to the keyword list: the observed
    branch's own tails, rebuilt under this parent. It differs per target, which is why it
    lives here rather than beside the counts -- picking a different destination changes
    what gets created, and a list that did not follow the selection would be describing
    the other choice.
*/
struct KeywordMergeTarget
{
    QString path;               // the vocabulary path, e.g. "Location|New Zealand"
    int images = 0;             // photographs already filed under it
    QStringList createsNodes;   // keyword-list entries this choice would add
    /*  How many of the moving keywords this destination ALREADY has. It exists so the
        numbers on screen ADD UP: alreadyThere + createsNodes.size() is the size of the
        branch named at the top. Without it the dialog said "19 beneath" and then "adds 6"
        and left the reader to work out what happened to the other thirteen -- which the
        author of this feature read as thirteen missing branches on first use, the
        strongest evidence a dialog can give that it is not explaining itself. */
    int alreadyThere = 0;
};

/*
    "This keyword is not in your list. Put it, and everything under it, where it belongs."

    WHY THIS EXISTS BESIDE THE DRAG. Filing a stray by dragging its photographs onto a
    node (MW::applyKeywordMoves) carries only the dropped keyword's LEAF, so a branch
    dropped that way is FLATTENED -- "New Zealand|North Island|Wellington" arrives as
    "Wellington" and two levels of the user's own structure are gone. Merging a whole
    observed branch into an authored one is the tail-preserving operation, and until now
    the only route to it was KeywordVocab::reparentMerging, which needs a vocabulary node
    on BOTH sides. An unfiled branch has no node to drag; that is what makes it unfiled.

    THE CHOICE IS THE POINT, AND IT IS ONLY OFFERED WHEN THERE IS ONE. A vocabulary grown
    from two applications routinely holds the same leaf twice, so "merge into the keyword
    list" cannot silently pick one. With a single candidate the dialog states it; with
    several it asks, with the image counts that make the answer obvious.

    NOTHING IS CREATED BEFORE THE USER AGREES -- the same contract KeywordDropDlg keeps.
    The nodes named under "This will add" are computed, not inserted; cancelling leaves
    the keyword list exactly as it was, not merely the photographs.

    THE SCOPE IS A BUTTON, NOT A CHECKBOX, and the counts are on the buttons, because that
    is the shape KeywordRetagDlg already established for exactly this question and the two
    are answering it about the same files. "Not now" is the default: Return must not
    rewrite sidecars.
*/
class KeywordMergeDlg : public QDialog
{
    Q_OBJECT

public:
    enum Choice { NotNow, ThisFolder, Everywhere };

    /*  unfiledPath is the branch as the catalog spells it. observedCount is how many
        keywords sit at or beneath it -- the size of what is moving, which the target
        counts do not convey, and which must equal alreadyThere + createsNodes.size() for
        the dialog's numbers to add up (see KeywordMergeTarget). folderCount/catalogCount
        are the photographs each scope would rewrite. */
    KeywordMergeDlg(const QString &unfiledPath, int observedCount,
                    const QList<KeywordMergeTarget> &targets,
                    int folderCount, int catalogCount,
                    bool folderScope, const QString &folderName,
                    QWidget *parent = nullptr);

    Choice choice() const { return result_; }
    QString target() const;

private:
    void showCreatesFor(int row);

    QList<KeywordMergeTarget> targets_;
    QListWidget *list_ = nullptr;       // null when there is only one candidate
    QLabel *creates_ = nullptr;
    Choice result_ = NotNow;
};

#endif // KEYWORDMERGEDLG_H
