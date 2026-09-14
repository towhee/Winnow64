#include "Develop/cameraprofilestore.h"

#include <QtConcurrent/QtConcurrent>
#include <QDir>
#include <QDirIterator>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>

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
CameraProfileStore::lookupLocked(const QString &cameraModel) const
{
    const QString k = key(cameraModel);
    if (k.isEmpty()) return nullptr;

    auto exact = index.constFind(k);
    if (exact != index.constEnd()) return &exact.value();

    /* LONGEST PREFIX, the same rule xyzToCamForModel uses: makers ship variants whose
       names extend a base model ("Sony ILCE-9M2" over "Sony ILCE-9"), and a profile for
       the base is a far better answer than no profile at all. Longest wins so a more
       specific profile is never displaced by a shorter one. */
    const QList<Entry> *best = nullptr;
    int bestLen = 0;
    for (auto it = index.constBegin(); it != index.constEnd(); ++it) {
        if (it.key().size() <= bestLen) continue;
        if (k.startsWith(it.key())) { best = &it.value(); bestLen = it.key().size(); }
    }
    return best;
}

QList<CameraProfileStore::Entry> CameraProfileStore::forModel(const QString &cameraModel) const
{
    QMutexLocker lock(&mutex);
    if (!ready) return {};
    const QList<Entry> *hit = lookupLocked(cameraModel);
    return hit ? *hit : QList<Entry>();
}

std::shared_ptr<const Dcp::Profile> CameraProfileStore::profile(const QString &cameraModel,
                                                               const QString &profileName)
{
    if (profileName.isEmpty()) return nullptr;

    QString path;
    {
        QMutexLocker lock(&mutex);
        if (!ready) return nullptr;
        const QList<Entry> *hit = lookupLocked(cameraModel);
        if (!hit) return nullptr;
        for (const Entry &e : *hit)
            if (e.name == profileName) { path = e.path; break; }
        if (path.isEmpty()) return nullptr;

        auto cached = parsed.constFind(path);
        if (cached != parsed.constEnd()) return cached.value();
    }

    /* Parsed OUTSIDE the lock: half a millisecond of file I/O, and this can be reached
       from a render thread while the GUI thread is asking for the menu. A second thread
       racing to parse the same profile wastes one parse and then agrees on the result --
       cheaper than serialising every reader behind file I/O. */
    Dcp::Profile p;
    if (!Dcp::parseFile(path, p)) return nullptr;
    auto sp = std::make_shared<const Dcp::Profile>(std::move(p));

    QMutexLocker lock(&mutex);
    auto cached = parsed.constFind(path);
    if (cached != parsed.constEnd()) return cached.value();
    parsed.insert(path, sp);
    return sp;
}
