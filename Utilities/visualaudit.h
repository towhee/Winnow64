#ifndef VISUALAUDIT_H
#define VISUALAUDIT_H

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>
#include <vector>

/*
    AUDIT KEYWORDS -- find keywording mistakes by comparing every image with the user's
    OWN keyworded images.

    THE QUESTION IS NOT "what is in this picture?" but "does this picture look like the
    other pictures you gave this keyword?". Each image is a SigLIP vector
    (Utilities/inference/imageembedder.h); each keyword in the user's list is learned
    from the images that carry it, against the keyworded images that do not. Measured in
    the Phase 0 prototype (tools/keyword_audit_proto.py) on an 87k-image catalog before
    any of this was written; the rules below are the ones that survived.

    CROSS-FITTED, OR A WRONG TAG VOUCHES FOR ITSELF. Every score for (image, keyword)
    comes from a model that never saw that image labelled -- nor any frame of its burst:

        kNN    similarity-weighted vote of the k nearest images, the image's own burst
               excluded from its neighbours
        ridge  one-vs-rest ridge regression, 5-fold, folds grouped by burst. Closed form:
               the Gram matrices are summed once and each fold solves with the totals
               minus its own share. Replaced the prototype's logistic probe, which found
               the same mistakes (96-98% of its top findings) at 1/130th of the cost.

    BOTH MUST AGREE before anything is reported.

    BURSTS ARE ONE UNIT. Adjacent frames in a folder that are near-duplicates (cosine
    >= burstCos, within burstSecs) are grouped. A burst tagged together and wrongly would
    otherwise confirm itself -- each frame's nearest neighbours are its siblings -- and
    the report would list the same mistake twelve times.

    SCORES BECOME RANKS so a rare keyword and a common one share one threshold:

        Suspect  an EXPLICITLY carried keyword whose score is beaten by more than
                 suspectFpr of the images that do not carry it. Ancestors are implied by
                 their children and never reported themselves. The best-scoring SIBLING
                 the image does not carry is offered as the alternative ("tagged Heron,
                 looks like Egret").
        Missing  an image without the keyword that outscores more than missingTpr of the
                 images that have it. Deepest keyword only: "Fauna|Bird|Heron", not also
                 "Fauna|Bird".

    SUGGESTED KEYWORDS reuse what the audit learned, for ANY image -- one with no
    keywords at all, or one with a few:

        visual   the keyword's ridge model refitted on EVERY keyworded image (no fold
                 held out: an image being suggested for was never a training label) and
                 the score ranked against the cross-fitted scores of the keyword's own
                 carriers. Suggested when it beats missingTpr of them, as Missing does --
                 ridge alone, because a kNN vote needs every vector in memory and the
                 Keywords panel asks one image at a time. Visual keywords only.
        session  keywords most of the image's SESSION carry: other images in the same
                 folder within sessionSecs of its capture time. Not learned and not
                 visual -- it is how an event, a client or a person's name is suggested,
                 which pixels cannot do. A share of at least sessionShare of at least
                 sessionMinImages keyworded neighbours.

    A suggestion never repeats what the image carries (or an ancestor of it), and the
    deepest of a chain wins: "Fauna|Bird|Heron", not also "Fauna|Bird". The model is
    saved after every audit (SuggestModel::save) so the panel can score the current
    image without running one.

    NOT EVERY KEYWORD IS VISUAL. A keyword whose cross-validated AUC is below minAuc
    (people's names across shoots, "Favourite", an event) cannot be judged from pixels
    and is skipped -- reported in the stats so the user can see why.

    PURE: no database, no model, no GUI. The caller supplies vectors and keyword sets and
    gets findings back, which is what tst_visualaudit pins.
*/
namespace VisualAudit {

struct Params
{
    int minPositives = 8;       // a keyword needs this many images to be learned
    double minAuc = 0.80;       // below this (either method) a keyword is not visual
    int k = 20;                 // kNN neighbours
    double tau = 0.05;          // kNN softmax temperature
    double ridgeLambda = 1.0;
    int folds = 5;
    int burstSecs = 60;
    double burstCos = 0.95;
    double suspectFpr = 0.10;
    double missingTpr = 0.60;
    double altTpr = 0.25;       // an alternative must beat this share of its own images
    int sessionSecs = 3 * 3600; // a session: same folder, within this of the capture
    int sessionMinImages = 2;   // keyworded neighbours needed before a session suggests
    double sessionShare = 0.5;  // share of them that must carry the keyword
    int maxSuggestions = 5;     // per image
};

struct Image
{
    QString pathKey;
    QString folder;
    qint64 captured = 0;        // seconds; 0 = unknown
    QStringList explicitPaths;  // what the image carries, ancestors dropped
    QStringList expandedPaths;  // every carried path and all its ancestors
};

struct Finding
{
    enum Kind { Suspect, Missing, Suggested };
    Kind kind = Suspect;
    QString keyword;            // display path, as the vocabulary spells it
    QString alternative;        // Suspect: best sibling, or empty
    QStringList siblings;       // Missing: siblings the image already carries
    QString reason;             // Suggested: why, in words ("looks like" / "session")
    double score = 0;           // 0..1, higher = more confident
    QVector<int> members;       // indices into the input images; >1 = a burst
};

struct KeywordStat
{
    QString keyword;
    int images = 0;
    double aucKnn = 0;
    double aucRidge = 0;
    bool visual = false;
    int suspects = 0;
    int missing = 0;
    QVector<int> exemplars;     // up to 3 carriers that look most like it (image indices)
};

/* One keyword suggested for one image. */
struct Suggestion
{
    QString keyword;            // display path
    double score = 0;           // 0..1: the visual rank, or the session share
    bool visual = false;        // looks like the keyword's images
    int sessionHits = 0;        // session: neighbours carrying it ...
    int sessionOf = 0;          // ... of this many keyworded neighbours
    /* "Looks like your Heron images" / "On 5 of 6 images from this session". */
    QString reason() const;
};

/*
    What the audit learned, kept for suggesting keywords between audits: every VISUAL
    keyword's full-fit ridge weights and the sorted cross-fitted scores of its carriers
    (what a new image's score is ranked against). A few MB for ~2,000 keywords; saved
    beside the index, because it is derived data like the vectors it was learned from.
*/
struct SuggestModel
{
    QString modelTag;           // ImageEmbedder::ModelTag the vectors came from
    QDateTime learned;
    int images = 0;             // keyworded images it was learned from
    int dim = 0;
    double minTpr = 0.6;
    QStringList keywords;       // display paths, one per column
    std::vector<float> W;       // (dim + 1) x keywords.size(), row-major, last row = bias
    std::vector<std::vector<float>> carrierScores;     // per column, ascending

    bool isValid() const { return dim > 0 && !keywords.isEmpty(); }
    bool save(const QString &file) const;
    bool load(const QString &file);

    /*  Visual suggestions for one L2-normalised vector. carriedFold: folded paths the
        image carries, ancestors included; dismissedFold: folded keywords the user said
        no to for this image. */
    QVector<Suggestion> score(const float *vec, const QSet<QString> &carriedFold,
                              const QSet<QString> &dismissedFold) const;
    /*  The second half of score(): raw is one image's keywords.size() ridge scores, bias
        included -- what suggest() gets for a whole block of images from one gemm. */
    QVector<Suggestion> rank(const float *raw, const QSet<QString> &carriedFold,
                             const QSet<QString> &dismissedFold) const;
};

/* An image as the session rule sees it. */
struct SessionImage
{
    QString folder;
    qint64 captured = 0;        // seconds; 0 = unknown, never in a session
    QStringList explicitPaths;  // carried keywords, ancestors dropped
};

/*  Session suggestions for me, from candidates (any images; those outside me's folder
    or time window are ignored, as is me itself if present). vocab: folded -> display of
    the keyword list -- only filed keywords are suggested. */
QVector<Suggestion> sessionSuggestions(const SessionImage &me,
                                       const QVector<SessionImage> &candidates,
                                       const QHash<QString, QString> &vocab,
                                       const QSet<QString> &carriedFold,
                                       const QSet<QString> &dismissedFold,
                                       const Params &params = {});

/*  Visual and session suggestions for one image as ONE list: a keyword both found is
    one suggestion with both reasons; a keyword that is an ancestor of another is
    dropped; best first, at most params.maxSuggestions. */
QVector<Suggestion> mergeSuggestions(const QVector<Suggestion> &visual,
                                     const QVector<Suggestion> &session,
                                     const Params &params = {});

struct Result
{
    bool ok = false;
    bool cancelled = false;
    QVector<Finding> findings;  // most confident first
    QVector<KeywordStat> stats; // learned keywords, most images first
    int bursts = 0;
    SuggestModel model;         // for suggestions; see SuggestModel
};

/* Folded keyword path, as the vocabulary keys it (keywordFold in
   Metadata/keywordpaths.h). */
QString foldKey(const QString &path);

/* Progress: stage text and 0..100. Return false to cancel. */
using ProgressFn = std::function<bool(const QString &stage, int pct)>;

/*  images[i] has the dim-float vector at emb[i * dim]. vocab maps folded path to the
    display path of every keyword in the user's list; keywords outside it (unfiled) are
    not learned. dismissed holds "pathKey|kind|foldedKeyword" (kind "suspect"/"missing"/
    "suggested") for findings the user has already judged; skipped holds folded keywords
    the user excluded from the audit.

    trainCount: only images[0, trainCount) are learned from and audited (-1 = all); the
    rest are there for suggest() to use. inScope, when given, is one flag per image:
    Suspect and Missing are REPORTED only for flagged images -- learning still uses
    every keyworded image, because a keyword learned from a filtered handful would be
    learned badly. */
Result run(const QVector<Image> &images, const std::vector<float> &emb, int dim,
           const QHash<QString, QString> &vocab, const QSet<QString> &dismissed,
           const QSet<QString> &skipped, const Params &params = {},
           const ProgressFn &progress = {}, int trainCount = -1,
           const std::vector<char> *inScope = nullptr);

/*  Suggested findings for images[r], r in rows, from the model run() learned and the
    session rule over ALL images. A keyword already reported as Missing for an image is
    not suggested again (exclude: "imageIndex|foldedKeyword"). Grouped by burst, like
    the audit's findings. */
QVector<Finding> suggest(const QVector<Image> &images, const std::vector<float> &emb,
                         int dim, const QVector<int> &rows, const SuggestModel &model,
                         const QHash<QString, QString> &vocab,
                         const QSet<QString> &dismissed, const QSet<QString> &exclude,
                         const Params &params = {}, const ProgressFn &progress = {});

/* "suspect" / "missing" / "suggested": the verdict store's kind for a finding. */
QString kindName(Finding::Kind kind);

/* The dismissed-set key for one image of a finding. */
QString verdictKey(const QString &pathKey, Finding::Kind kind, const QString &keyword);

}   // namespace VisualAudit

#endif // VISUALAUDIT_H
