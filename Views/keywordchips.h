#ifndef KEYWORDCHIPS_H
#define KEYWORDCHIPS_H

#include <QMap>
#include <QString>
#include <QWidget>

class FlowLayout;
class KeywordVocab;
class QLineEdit;
class QLabel;

/*
    THE CHIP ZONE -- the lower half of the Keywords dock, and the only place a keyword is
    put on a photograph.

    ONE CHIP PER KEYWORD THE SELECTION CARRIES, marked by what it is:

      plain   on every selected image
      *       on SOME of them. Clicking the chip promotes it to all; the x removes it
              from all. Without this marker a chip would be a lie the moment more than
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

    IT READS PATHS AND NEVER LEAVES. What the chips show is the keywords the user
    ASSIGNED -- not G::KeywordsAllColumn, which holds every ancestor of every path and
    would put a "Fauna" chip on an image tagged only "Fauna|Bird|Heron". Removing that
    chip would then have to mean something, and there is no good answer to what.
*/
class KeywordChips : public QWidget
{
    Q_OBJECT

public:
    explicit KeywordChips(KeywordVocab *vocab, QWidget *parent = nullptr);

    /*  Rebuild from the selection: path -> how many of the selected images carry it,
        which is what MW::keywordsInSelection returns. selectionSize decides which chips
        are partial. */
    void setSelection(const QMap<QString, int> &counts, int selectionSize);

signals:
    void addRequested(const QString &path);
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

    KeywordVocab *vocab = nullptr;
    FlowLayout *flow = nullptr;
    QWidget *chipArea = nullptr;
    QLineEdit *addEdit = nullptr;
    QLabel *legend = nullptr;

    QMap<QString, int> counts;
    int selectionSize = 0;
};

#endif // KEYWORDCHIPS_H
