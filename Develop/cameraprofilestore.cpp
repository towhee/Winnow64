#include "Develop/cameraprofilestore.h"

#include "ImageFormats/Raw/cameramodel.h"

#include <QtConcurrent/QtConcurrent>
#include <QDir>
#include <QDirIterator>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QMap>
#include <QSet>

CameraProfileStore &CameraProfileStore::instance()
{
    static CameraProfileStore s;
    return s;
}

CameraProfileStore::CameraProfileStore(QObject *parent) : QObject(parent) {}

QStringList CameraProfileStore::roots()
{
    QStringList out;
#ifdef Q_OS_MAC
    out << "/Library/Application Support/Adobe/CameraRaw/CameraProfiles"
        << QDir::homePath() + "/Library/Application Support/Adobe/CameraRaw/CameraProfiles";
#endif
#ifdef Q_OS_WIN
    const QString programData = qEnvironmentVariable("ProgramData", "C:/ProgramData");
    const QString appData = qEnvironmentVariable("APPDATA");
    out << programData + "/Adobe/CameraRaw/CameraProfiles";
    if (!appData.isEmpty()) out << appData + "/Adobe/CameraRaw/CameraProfiles";
#endif
    /* Winnow's own folder, for profiles the user made or was given. Listed LAST so a
       user-supplied profile of the same name as an installed one does not displace it in
       the menu -- both appear, and the duplicate is visible rather than silent. */
    const QString app = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!app.isEmpty()) out << app + "/CameraProfiles";
    return out;
}

QString CameraProfileStore::key(const QString &model)
{
    static const QRegularExpression ws("\\s+");
    return model.simplified().replace(ws, " ").toUpper();
}

void CameraProfileStore::ensureIndex()
{
    {
        QMutexLocker lock(&mutex);
        if (started) return;
        started = true;
    }
    /* Detached: nothing waits on the result, and the panel is told by signal. A failed or
       empty sweep is not an error -- a machine with no profiles installed simply offers
       the built-in entry. */
    (void)QtConcurrent::run([this] { scan(); });
}

bool CameraProfileStore::indexReady() const
{
    QMutexLocker lock(&mutex);
    return ready;
}

void CameraProfileStore::scan()
{
    QHash<QString, QList<Entry>> built;

    for (const QString &root : roots()) {
        if (!QDir(root).exists()) continue;
        QDirIterator it(root, QStringList() << "*.dcp", QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            QString model, name;
            if (!Dcp::peek(path, model, name)) continue;
            /* A profile with no ProfileName is legal; fall back to the file's base name so
               it is still selectable rather than showing an empty menu row. */
            if (name.isEmpty()) name = QFileInfo(path).completeBaseName();
            built[key(model)] << Entry{name, path};
        }
    }

    for (auto it = built.begin(); it != built.end(); ++it)
        std::sort(it->begin(), it->end(),
                  [](const Entry &a, const Entry &b) {
                      return a.name.localeAwareCompare(b.name) < 0;
                  });

    {
        QMutexLocker lock(&mutex);
        index = built;
        ready = true;
    }
    /* Queued: scan() runs on a worker and the panel's slots touch widgets. */
    QMetaObject::invokeMethod(this, "indexChanged", Qt::QueuedConnection);
}

const QList<CameraProfileStore::Entry> *
CameraProfileStore::lookupLocked(const QString &cameraModel, QString *matchedKey) const
{
    if (matchedKey) matchedKey->clear();
    if (cameraModel.isEmpty()) return nullptr;

    /*
        THE MODEL, THEN ITS MAKER ALIASES. Winnow's canonical model follows libraw, which
        files the OM bodies under "Olympus"; Adobe ships their profiles under "OM Digital
        Solutions". Neither spelling is wrong, so both are tried -- the canonical one
        first, so a maker that does agree is never sent down the alias path.
    */
    QStringList candidates;
    candidates << cameraModel << cameraModelAliases(cameraModel);

    for (const QString &candidate : std::as_const(candidates)) {
        const QString k = key(candidate);
        if (k.isEmpty()) continue;
        auto exact = index.constFind(k);
        if (exact != index.constEnd()) {
            if (matchedKey) *matchedKey = k;
            return &exact.value();
        }
    }

    /* LONGEST PREFIX, the same rule xyzToCamForModel uses: makers ship variants whose
       names extend a base model ("Sony ILCE-9M2" over "Sony ILCE-9"), and a profile for
       the base is a far better answer than no profile at all. Longest wins so a more
       specific profile is never displaced by a shorter one. Run only after EVERY
       candidate has failed to match exactly, so an alias's exact hit always beats the
       canonical spelling's partial one. */
    const QList<Entry> *best = nullptr;
    int bestLen = 0;
    for (const QString &candidate : std::as_const(candidates)) {
        const QString k = key(candidate);
        if (k.isEmpty()) continue;
        for (auto it = index.constBegin(); it != index.constEnd(); ++it) {
            if (it.key().size() <= bestLen) continue;
            if (k.startsWith(it.key())) {
                best = &it.value();
                bestLen = it.key().size();
                if (matchedKey) *matchedKey = it.key();
            }
        }
    }
    if (!best && matchedKey) matchedKey->clear();
    return best;
}

namespace {

/* The part of a profile a BASE renders with -- everything except the look. Two profiles
   agreeing here render identically once their looks are stripped, which is what makes one
   shared base entry correct rather than an approximation. */
QString baseSignature(const Dcp::Profile &p)
{
    QString s;
    for (int c = 0; c < 2; ++c) {
        const Dcp::Calibration &cal = p.cal[c];
        s += QString::number(cal.illuminant) + ':';
        const Dcp::Matrix3 *m[3] = {&cal.color, &cal.forward, &cal.calibration};
        const bool have[3] = {cal.haveColor, cal.haveForward, cal.haveCalibration};
        for (int k = 0; k < 3; ++k) {
            if (!have[k]) { s += "-|"; continue; }
            for (int i = 0; i < 3; ++i)
                for (int j = 0; j < 3; ++j)
                    s += QString::number(double(m[k]->m[i][j]), 'f', 6) + ' ';
            s += '|';
        }
        s += QString::number(cal.hueSatMap.entries()) + ';';
    }
    for (int i = 0; i < 3; ++i) s += QString::number(double(p.analogBalance[i]), 'f', 6) + ' ';
    return s;
}

/*
    The part of a group's profile names they all end with, cut back to a word boundary.

    "Camera BW v2" / "Camera NT v2" / ... share " v2", which is exactly the distinction
    between Adobe's two generations and so exactly what the two bases must be called apart
    by. The word-boundary rule is what stops "Camera ST" and "Camera NT" contributing a
    shared trailing "T" and producing "Camera BaseT".
*/
QString commonSuffix(const QStringList &names)
{
    if (names.isEmpty()) return QString();
    QString suf = names.first();
    for (const QString &n : names) {
        int i = 0;
        while (i < suf.size() && i < n.size() &&
               suf.at(suf.size() - 1 - i) == n.at(n.size() - 1 - i)) ++i;
        suf = suf.right(i);
    }
    const int sp = suf.indexOf(QLatin1Char(' '));
    return sp < 0 ? QString() : suf.mid(sp);
}

} // namespace

QList<CameraProfileStore::Entry> CameraProfileStore::deriveBases(const QList<Entry> &real)
{
    /* Group this camera's CURVE-BEARING profiles by what a base render would use. */
    QMap<QString, QStringList> names;       // signature -> member profile names
    QMap<QString, QString> source;          // signature -> a member's path
    for (const Entry &e : real) {
        if (e.isBase) continue;
        Dcp::Profile p;
        if (!Dcp::parseFile(e.path, p)) continue;
        if (p.toneCurve.empty()) continue;  // no tone curve -> no base worth offering
        const QString sig = baseSignature(p);
        names[sig] << e.name;
        if (!source.contains(sig)) source.insert(sig, e.path);
    }
    if (names.isEmpty()) return {};

    /* Name them. One group is the overwhelmingly common case (413 of 431 installed
       cameras) and is simply "Camera Base"; two generations become "Camera Base" and
       "Camera Base v2". A camera odd enough to yield two groups with the SAME name falls
       back to naming one after a member, so every row stays selectable and distinct --
       a duplicate name would make one of them unreachable, since a profile is stored and
       resolved BY NAME. */
    QList<Entry> out;
    QSet<QString> used;
    for (const Entry &e : real) used.insert(e.name);
    for (auto it = names.begin(); it != names.end(); ++it) {
        QStringList members = it.value();
        members.sort();
        QString name = QStringLiteral("Camera Base") + commonSuffix(members);
        if (used.contains(name)) name += QStringLiteral(" (") + members.first() + QLatin1Char(')');
        if (used.contains(name)) continue;          // still colliding: drop it rather than
        used.insert(name);                          // shadow a real profile
        out << Entry{name, source.value(it.key()), true};
    }
    return out;
}

QList<CameraProfileStore::Entry> CameraProfileStore::forModel(const QString &cameraModel)
{
    QList<Entry> real;
    QString matchedKey;
    {
        QMutexLocker lock(&mutex);
        if (!ready) return {};
        const QList<Entry> *hit = lookupLocked(cameraModel, &matchedKey);
        if (!hit) return {};
        real = *hit;
        auto cached = bases.constFind(matchedKey);
        if (cached != bases.constEnd()) return sorted(real + cached.value());
    }

    /* Derived OUTSIDE the lock -- it parses ~9-18 files. Two threads racing wastes one
       derivation and then agrees on the result, which is cheaper than holding the mutex
       across file I/O that every other reader would queue behind. */
    const QList<Entry> derived = deriveBases(real);

    QMutexLocker lock(&mutex);
    bases.insert(matchedKey, derived);
    return sorted(real + derived);
}

QList<CameraProfileStore::Entry> CameraProfileStore::sorted(QList<Entry> list)
{
    std::sort(list.begin(), list.end(), [](const Entry &a, const Entry &b) {
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return list;
}

std::shared_ptr<const Dcp::Profile> CameraProfileStore::profile(const QString &cameraModel,
                                                               const QString &profileName)
{
    if (profileName.isEmpty()) return nullptr;

    /* forModel takes the mutex itself, so it must be called before this one does. It also
       derives the bases, which is what makes a base name resolvable here at all. */
    const QList<Entry> all = forModel(cameraModel);
    QString path;
    bool isBase = false;
    for (const Entry &e : all)
        if (e.name == profileName) { path = e.path; isBase = e.isBase; break; }
    if (path.isEmpty()) return nullptr;

    /* A base is a DIFFERENT profile from the file it came from, so it cannot share that
       file's cache slot. */
    const QString cacheKey = isBase ? path + QStringLiteral("\n#base") : path;
    {
        QMutexLocker lock(&mutex);
        auto cached = parsed.constFind(cacheKey);
        if (cached != parsed.constEnd()) return cached.value();
    }

    /* Parsed OUTSIDE the lock: half a millisecond of file I/O, and this can be reached
       from a render thread while the GUI thread is asking for the menu. A second thread
       racing to parse the same profile wastes one parse and then agrees on the result --
       cheaper than serialising every reader behind file I/O. */
    Dcp::Profile p;
    if (!Dcp::parseFile(path, p)) return nullptr;
    if (isBase) {
        /* THE STRIP. Everything creative goes; the characterisation stays. Done here, so
           that the render path has no notion of a profile being partly applied -- it is
           handed a profile that simply has no look. */
        p.lookTable = Dcp::Table3D();
        p.toneCurve.clear();
        p.baselineExposureOffset = 0.0f;
        p.name = profileName;
    }
    auto sp = std::make_shared<const Dcp::Profile>(std::move(p));

    QMutexLocker lock(&mutex);
    auto cached = parsed.constFind(cacheKey);
    if (cached != parsed.constEnd()) return cached.value();
    parsed.insert(cacheKey, sp);
    return sp;
}
