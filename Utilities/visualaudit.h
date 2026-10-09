#ifndef VISUALAUDIT_H
#define VISUALAUDIT_H

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
    enum Kind { Suspect, Missing };
    Kind kind = Suspect;
    QString keyword;            // display path, as the vocabulary spells it
    QString alternative;        // Suspect: best sibling, or empty
    QStringList siblings;       // Missing: siblings the image already carries
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

struct Result
{
    bool ok = false;
    bool cancelled = false;
    QVector<Finding> findings;  // most confident first
    QVector<KeywordStat> stats; // learned keywords, most images first
    int bursts = 0;
};

/* Folded keyword path, as the vocabulary keys it (keywordFold in
   Metadata/keywordpaths.h). */
QString foldKey(const QString &path);

/* Progress: stage text and 0..100. Return false to cancel. */
using ProgressFn = std::function<bool(const QString &stage, int pct)>;

/*  images[i] has the dim-float vector at emb[i * dim]. vocab maps folded path to the
    display path of every keyword in the user's list; keywords outside it (unfiled) are
    not learned. dismissed holds "pathKey|kind|foldedKeyword" (kind "suspect"/"missing")
    for findings the user has already judged; skipped holds folded keywords the user
    excluded from the audit. */
Result run(const QVector<Image> &images, const std::vector<float> &emb, int dim,
           const QHash<QString, QString> &vocab, const QSet<QString> &dismissed,
           const QSet<QString> &skipped, const Params &params = {},
           const ProgressFn &progress = {});

/* The dismissed-set key for one image of a finding. */
QString verdictKey(const QString &pathKey, Finding::Kind kind, const QString &keyword);

}   // namespace VisualAudit

#endif // VISUALAUDIT_H
