#ifndef KEYWORDTAGS_H
#define KEYWORDTAGS_H

#include <QMap>
#include <QPointer>
#include <QString>
#include <QWidget>

class FlowLayout;
class GradientHeader;
class KeywordVocab;
class QCompleter;
class QLineEdit;
class QLabel;
struct VocabNode;

/*
    THE TAG ZONE -- the upper zone of the Keywords dock, and the only place a keyword is
    put on a photograph.

    ONE TAG PER KEYWORD THE SELECTION CARRIES, marked by what it is:

      plain   on every selected image
      *       on SOME of them. Right-click > "Add to all selected images" promotes it
              to all; the x removes it from all. Without this marker a tag would be a
              lie the moment more than one image is selected, which is most of the time.
              CLICKING any white (filed) tag shows it in the Keyword list.
      red     on the image but NOT in the vocabulary -- a keyword some other application
              wrote, or one whose vocabulary node was renamed and the files declined.
              Drawn in G::unfiledKeywordColor, the red Filters uses for the same
              keywords. It is never filed into the list from here -- keywords are
              created in the Keyword list only. CLICKING it offers similar list keywords
              in the Add field's popup and picking one REPLACES it (startReplace);
              DROPPING a list keyword on it replaces it too (replaceRequested). Bridge
              calls these "Other Keywords" and gives them their own group; inline is
              better, because the user is looking at this image's keywords and that is
              where the odd one out belongs.

    THE ADD FIELD ONLY ADDS KEYWORDS ALREADY IN THE LIST -- keywords are created in the
    Keyword list, nowhere else (see commitTyped). THE ADD FIELD IS THE FAST PATH. A tree
    is the wrong instrument for the twentieth image: the completer offers matching
    keywords as DISTINCT entries showing their branch, so the two Vancouvers are two
    choices rather than one ambiguous one, and picking either is one keystroke. It
    completes on synonyms too.

    IT READS PATHS AND NEVER LEAVES. What the tags show is the keywords the user
    ASSIGNED -- not G::KeywordsAllColumn, which holds every ancestor of every path and
    would put a "Fauna" tag on an image tagged only "Fauna|Bird|Heron". Removing that
    tag would then have to mean something, and there is no good answer to what.
*/
class KeywordTags : public QWidget
{
    Q_OBJECT

public:
    explicit KeywordTags(KeywordVocab *vocab, QWidget *parent = nullptr);

    /*  Clicked red tag: offer the list keywords that look like it in the Add field's
        popup; picking one REPLACES the red keyword (see `replacing`). Public because MW
        calls it AFTER filtering on the red keyword (see redTagClicked). */
    void startReplace(const QString &unfiledPath);

    /*  Rebuild from the selection: path -> how many of the selected images carry it,
        which is what MW::keywordsInSelection returns. selectionSize decides which tags
        are partial. */
    void setSelection(const QMap<QString, int> &counts, int selectionSize);

signals:
    void addRequested(const QString &path);
    /*  A DROP CARRIES SEVERAL KEYWORDS AND IS ONE USER ACTION. Emitting addRequested per
        path made it N of them: N rewrites of every selected image's sidecar, N bumps of
        each file's ModifyDate and N rebuilds of the whole keyword tree, for one gesture.
        MW::applyKeywordsToSelection already takes a LIST and applies it in one pass --
        the same rule MW::applyKeywordMoves obeys by hoisting its rebuild out of the
        loop. */
    void addManyRequested(const QStringList &paths);
    void removeRequested(const QString &path);
    /*  A white (filed) tag was clicked: reveal and select it in the Keyword list. */
    void showRequested(const QString &path);
    /*  A red tag was clicked. MW filters on it, THEN calls startReplace -- in that order,
        so the filter's selection change cannot cancel the replace it would otherwise
        race with. */
    void redTagClicked(const QString &path);
    /*  Vocabulary keyword(s) dropped on an unfiled tag, or picked for it after clicking
        it: swap unfiledPath for them on the selected images that carry it. */
    void replaceRequested(const QString &unfiledPath, const QStringList &paths);

protected:
    /*  Accepts a keyword dragged down from the vocabulary tree. The other direction --
        dragging images onto a tree node -- is handled by the tree. */
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void rebuild();
    void setDropTarget(QWidget *tag);        // outline the unfiled tag a drop would hit
    void commitTyped();
    void endReplace();
    /*  legendBase plus, while replacing, what the Add field is doing. */
    void updateLegend();
    /*  Vocabulary nodes whose name or a synonym resembles text: equal, or either one
        containing the other. Equal first, then shallowest. */
    QList<const VocabNode *> similarNodes(const QString &text) const;
    /*  Fill the completer's model with nodes, leaf + dimmed branch per row. */
    void fillCompletions(const QList<const VocabNode *> &nodes);
    /*  "Selected image tags" / "Selected images tags" -- the band tracks the selection
        because one image and forty are different questions. */
    QString headerText() const;

    KeywordVocab *vocab = nullptr;
    GradientHeader *header = nullptr;
    FlowLayout *flow = nullptr;
    QWidget *tagArea = nullptr;
    QLineEdit *addEdit = nullptr;
    QLabel *legend = nullptr;
    QCompleter *completer = nullptr;
    QString legendBase;
    /*  The red keyword a click put the Add field into REPLACE mode for; empty = ADD
        mode. Cleared by a pick, by emptying the field, and by a selection change. */
    QString replacing;

    QMap<QString, int> counts;
    int selectionSize = 0;
    QPointer<QWidget> dropTarget;           // a Tag; QPointer because rebuild deletes it
};

#endif // KEYWORDTAGS_H
