/*
    VisualAudit -- the scoring behind Audit Keywords (Utilities/visualaudit.h).

    Synthetic embeddings stand in for SigLIP: each keyword is a direction in a 16-d
    space and an image is that direction plus noise, normalised. Three clusters (Heron,
    Egret, Tree) of 40 images each, then the mistakes the audit exists to find are
    planted by hand:

      - a Heron-looking image tagged Egret          -> Suspect Egret, alternative Heron
      - a Heron-looking image tagged only Favourite -> Missing Heron (and NOT Fauna|Bird:
                                                       deepest only)
      - a three-frame BURST of Egret-looking frames tagged Heron -> ONE Suspect finding
        with three members, and no separate "Missing Egret" for the same mistake.
        Without burst exclusion each frame's nearest neighbours are its siblings, which
        carry the same wrong tag, and the mistake confirms itself.

    EACH CASE PLANTS ONLY ITS OWN MISTAKE. Two planted images near the same centroid are
    each other's nearest neighbours, and one mistake then masks the other -- real, but
    not what any single case here is about. Planted images are drawn close to their
    cluster's centre so the result does not depend on a noise draw: every case is run
    over several seeds.

    "Category|Favourite" is spread at random, so no picture predicts it: it must be
    skipped as not visual rather than producing findings.
*/
#include <QtTest>
#include <cmath>
#include <random>
#include <vector>
#include "Utilities/visualaudit.h"

using namespace VisualAudit;

namespace {

constexpr int kDim = 16;
constexpr int kClean = 120;                 // the three clusters, before any plant
const unsigned kSeeds[] = {1, 2, 3, 5, 7, 11, 13, 17};

enum Plant { None = 0, PlantSuspect = 1, PlantMissing = 2, PlantBurst = 4 };

struct Fixture
{
    QVector<Image> images;
    std::vector<float> emb;
    QHash<QString, QString> vocab;
    int plantedSuspect = -1;
    int plantedMissing = -1;
    QVector<int> plantedBurst;
};

QStringList expand(const QStringList &paths)
{
    QStringList out;
    for (const QString &p : paths) {
        QString prefix;
        for (const QString &n : p.split('|')) {
            prefix = prefix.isEmpty() ? n : prefix + '|' + n;
            if (!out.contains(prefix)) out << prefix;
        }
    }
    return out;
}

Fixture build(int plants, unsigned seed)
{
    Fixture fx;
    for (const char *p : {"Fauna", "Fauna|Bird", "Fauna|Bird|Heron", "Fauna|Bird|Egret",
                          "Flora", "Flora|Tree", "Category", "Category|Favourite"})
        fx.vocab.insert(foldKey(p), p);

    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(0.0f, 0.2f);
    std::uniform_real_distribution<float> coin(0.0f, 1.0f);

    /*  dir: which axis the picture looks like. jitter scales the noise (a burst frame
        is a near-copy of its first frame). Every image gets its own folder and capture
        times hours apart, so only the planted burst can group. */
    auto add = [&](int dir, const QStringList &tags, float jitter = 1.0f,
                   const float *copyOf = nullptr, const QString &folder = {},
                   qint64 captured = 0) {
        std::vector<float> v(kDim);
        for (int d = 0; d < kDim; ++d) {
            const float base = copyOf ? copyOf[d] : (d == dir ? 1.0f : 0.0f);
            v[d] = base + jitter * noise(rng);
        }
        double norm = 0;
        for (float x : v) norm += double(x) * x;
        for (float &x : v) x = float(x / std::sqrt(norm));
        fx.emb.insert(fx.emb.end(), v.begin(), v.end());

        const int i = fx.images.size();
        QStringList explicitTags = tags;
        if (coin(rng) < 0.3f && !explicitTags.contains("Category|Favourite"))
            explicitTags << "Category|Favourite";
        Image im;
        im.pathKey = QString("/pics/%1.jpg").arg(i, 4, 10, QChar('0'));
        im.folder = folder.isEmpty() ? QString("/pics/f%1").arg(i) : folder;
        im.captured = captured ? captured : 1'000'000 + qint64(i) * 10'000;
        im.explicitPaths = explicitTags;
        im.expandedPaths = expand(explicitTags);
        fx.images << im;
        return i;
    };

    for (int i = 0; i < 40; ++i) add(0, {"Fauna|Bird|Heron"});
    for (int i = 0; i < 40; ++i) add(1, {"Fauna|Bird|Egret"});
    for (int i = 0; i < 40; ++i) add(2, {"Flora|Tree"});

    if (plants & PlantSuspect) fx.plantedSuspect = add(0, {"Fauna|Bird|Egret"}, 0.3f);
    if (plants & PlantMissing) fx.plantedMissing = add(0, {"Category|Favourite"}, 0.3f);
    if (plants & PlantBurst) {
        const int b0 = add(1, {"Fauna|Bird|Heron"}, 0.3f, nullptr, "/pics/burst",
                           5'000'000);
        const std::vector<float> first(fx.emb.begin() + ptrdiff_t(b0) * kDim,
                                       fx.emb.begin() + ptrdiff_t(b0 + 1) * kDim);
        fx.plantedBurst << b0;
        for (int f = 1; f <= 2; ++f)
            fx.plantedBurst << add(-1, {"Fauna|Bird|Heron"}, 0.02f, first.data(),
                                   "/pics/burst", 5'000'000 + 2 * f);
    }
    return fx;
}

Result audit(const Fixture &fx, const QSet<QString> &dismissed = {},
             const QSet<QString> &skipped = {})
{
    return run(fx.images, fx.emb, kDim, fx.vocab, dismissed, skipped);
}

const Finding *find(const Result &r, Finding::Kind kind, const QString &kw, int member)
{
    for (const Finding &f : r.findings)
        if (f.kind == kind && f.keyword == kw && f.members.contains(member)) return &f;
    return nullptr;
}

}   // namespace

class TestVisualAudit : public QObject
{
    Q_OBJECT

private slots:
    void plantedMislabelIsSuspectWithAlternative();
    void unkeywordedLookalikeIsMissingDeepestOnly();
    void mislabelledBurstIsOneFinding();
    void randomKeywordIsNotVisual();
    void dismissedAndSkippedAreFiltered();
    void cleanImagesProduceFewFindings();
};

void TestVisualAudit::plantedMislabelIsSuspectWithAlternative()
{
    for (unsigned seed : kSeeds) {
        const Fixture fx = build(PlantSuspect, seed);
        const Result r = audit(fx);
        QVERIFY(r.ok);
        const Finding *f = find(r, Finding::Suspect, "Fauna|Bird|Egret", fx.plantedSuspect);
        QVERIFY2(f, qPrintable(QString("seed %1: the Heron-looking image tagged Egret "
                                       "was not flagged").arg(seed)));
        QCOMPARE(f->alternative, QString("Fauna|Bird|Heron"));
    }
}

void TestVisualAudit::unkeywordedLookalikeIsMissingDeepestOnly()
{
    for (unsigned seed : kSeeds) {
        const Fixture fx = build(PlantMissing, seed);
        const Result r = audit(fx);
        QVERIFY2(find(r, Finding::Missing, "Fauna|Bird|Heron", fx.plantedMissing),
                 qPrintable(QString("seed %1").arg(seed)));
        /* The ancestors look just as missing, but the leaf says it all. */
        QVERIFY(!find(r, Finding::Missing, "Fauna|Bird", fx.plantedMissing));
        QVERIFY(!find(r, Finding::Missing, "Fauna", fx.plantedMissing));
    }
}

void TestVisualAudit::mislabelledBurstIsOneFinding()
{
    for (unsigned seed : kSeeds) {
        const Fixture fx = build(PlantBurst, seed);
        const Result r = audit(fx);
        const Finding *f = find(r, Finding::Suspect, "Fauna|Bird|Heron",
                                fx.plantedBurst[0]);
        QVERIFY2(f, qPrintable(QString("seed %1: the mislabelled burst confirmed itself")
                                   .arg(seed)));
        QCOMPARE(f->members.size(), 3);
        for (int m : fx.plantedBurst) QVERIFY(f->members.contains(m));
        QCOMPARE(f->alternative, QString("Fauna|Bird|Egret"));
        /* One mistake, one finding: not also "Missing Egret" for the same frames. */
        for (int m : fx.plantedBurst)
            QVERIFY(!find(r, Finding::Missing, "Fauna|Bird|Egret", m));
    }
}

void TestVisualAudit::randomKeywordIsNotVisual()
{
    for (unsigned seed : kSeeds) {
        const Result r = audit(build(None, seed));
        bool seen = false;
        for (const KeywordStat &s : r.stats) {
            if (s.keyword == "Category|Favourite") {
                seen = true;
                QVERIFY2(!s.visual, qPrintable(QString("seed %1: AUC %2 / %3").arg(seed)
                                                   .arg(s.aucKnn).arg(s.aucRidge)));
                QCOMPARE(s.suspects + s.missing, 0);
            }
            if (s.keyword == "Fauna|Bird|Heron") QVERIFY(s.visual);
        }
        QVERIFY(seen);
    }
}

void TestVisualAudit::dismissedAndSkippedAreFiltered()
{
    const Fixture fx = build(PlantSuspect, 7);
    const QString key = verdictKey(fx.images[fx.plantedSuspect].pathKey, Finding::Suspect,
                                   "Fauna|Bird|Egret");
    QVERIFY(find(audit(fx), Finding::Suspect, "Fauna|Bird|Egret", fx.plantedSuspect));
    QVERIFY(!find(audit(fx, {key}), Finding::Suspect, "Fauna|Bird|Egret",
                  fx.plantedSuspect));

    const Result skipped = audit(fx, {}, {foldKey("Fauna|Bird|Heron")});
    for (const Finding &f : skipped.findings) {
        QVERIFY(f.keyword != "Fauna|Bird|Heron");
        QVERIFY(f.alternative != "Fauna|Bird|Heron");
    }
}

void TestVisualAudit::cleanImagesProduceFewFindings()
{
/*
    The rank thresholds are relative, so a clean cluster still has a tail -- but it must
    be a tail. Fewer than 10% of the clean images may appear in any finding.
*/
    for (unsigned seed : kSeeds) {
        const Result r = audit(build(None, seed));
        QSet<int> flagged;
        for (const Finding &f : r.findings)
            for (int m : f.members) flagged.insert(m);
        QVERIFY2(flagged.size() < kClean / 10,
                 qPrintable(QString("seed %1: %2 flagged").arg(seed).arg(flagged.size())));
    }
}

QTEST_MAIN(TestVisualAudit)
#include "tst_visualaudit.moc"
