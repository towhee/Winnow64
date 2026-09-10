#ifndef KEYWORDTAGS_H
#define KEYWORDTAGS_H

#include <QMap>
#include <QString>
#include <QWidget>

class FlowLayout;
class GradientHeader;
class KeywordVocab;
class QLineEdit;
class QLabel;

/*
    THE TAG ZONE -- the upper zone of the Keywords dock, and the only place a keyword is
    put on a photograph.

    ONE TAG PER KEYWORD THE SELECTION CARRIES, marked by what it is:

      plain   on every selected image
      *       on SOME of them. Clicking the tag promotes it to all; the x removes it
              from all. Without this marker a tag would be a lie the moment more than
              one image is selected, which is most of the time.
      ?       on the image but NOT in the vocabulary -- a keyword some other application
              wrote, or one whose vocabulary node was renamed and the files declined.
              It offers to file itself into the tree. Bridge calls these "Other
              Keywords" and gives them their own group; inline is better, because the
              user is looking at this image's keywords and that is where the odd one out
              belongs.

    THE ADD FIELD IS THE FAST PATH. A tree is the wrong instrument for the twentieth
    image: the completer offers matching keywords as DISTINCT entries showing their
    branch, so the two Vancouvers are two choices rather than one ambiguous one, and
    picking either is one keystroke. It completes on synonyms too.

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
    /*  An unfiled keyword the user wants in the vocabulary. The dock adds it to the tree
        rather than this widget doing it, because where it lands is a tree question. */
    void fileRequested(const QString &path);

protected:
    /*  Accepts a keyword dragged down from the vocabulary tree. The other direction --
        dragging images onto a tree node -- is handled by the tree. */
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void rebuild();
    void commitTyped();
    /*  "Selected image tags" / "Selected images tags" -- the band tracks the selection
        because one image and forty are different questions. */
    QString headerText() const;

    KeywordVocab *vocab = nullptr;
    GradientHeader *header = nullptr;
    FlowLayout *flow = nullptr;
    QWidget *tagArea = nullptr;
    QLineEdit *addEdit = nullptr;
    QLabel *legend = nullptr;

    QMap<QString, int> counts;
    int selectionSize = 0;
};

#endif // KEYWORDTAGS_H
