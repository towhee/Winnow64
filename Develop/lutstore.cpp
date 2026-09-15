#include "Develop/lutstore.h"

#include "Develop/lutparse.h"

#include <QtConcurrent/QtConcurrent>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QMutexLocker>
#include <QStandardPaths>
#include <algorithm>

namespace {

/*
    A .cube is text, so its size bounds what the parser can build: the largest table
    Lut3d accepts is 128^3 triplets, which even at a terse eight bytes a number is under
    60 MB. Anything past this is not a LUT, and reading it into memory to find that out is
    the denial of service itself.
*/
constexpr qint64 kMaxCubeBytes = 96LL * 1024 * 1024;

/* The extensions swept. PNG only for Hald in this pass -- RawTherapee also accepts TIFF,
   and QImageReader would handle it, but claiming a format nothing has been tested against
   is how a "supported" list stops meaning anything. */
const char *const kCubeExt = "cube";
const char *const kHaldExt = "png";

/* The TITLE line's value, if the head of the file carries one. Display only -- see the
   header on why the title is never an identity. */
QString titleFrom(const QByteArray &head)
{
    for (const QByteArray &raw : head.split('\n')) {
        QByteArray line = raw.trimmed();
        if (!line.toUpper().startsWith("TITLE")) continue;
        line = line.mid(5).trimmed();
        if (line.size() >= 2 && line.startsWith('"') && line.endsWith('"'))
            line = line.mid(1, line.size() - 2);
        return QString::fromUtf8(line);
    }
    return QString();
}

} // namespace

LutStore &LutStore::instance()
{
    static LutStore s;
    return s;
}

LutStore::LutStore(QObject *parent) : QObject(parent) {}

QStringList LutStore::roots()
{
    QStringList out;
    const QString app = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!app.isEmpty()) out << app + "/Looks";
    return out;
}

void LutStore::ensureIndex()
{
    {
        QMutexLocker lock(&mutex);
        if (started) return;
        started = true;
    }
    /* Detached: nothing waits on the result and the panel is told by signal. An empty
       sweep is not an error -- a machine with no looks installed simply offers none. */
    (void)QtConcurrent::run([this] { scan(); });
}

bool LutStore::indexReady() const
{
    QMutexLocker lock(&mutex);
    return ready;
}

void LutStore::scan()
{
    /* Serialised, so a background sweep and an inline one from lut() cannot both run.
       Whichever arrives second finds `ready` already set and returns. */
    QMutexLocker scanLock(&scanMutex);
    {
        QMutexLocker lock(&mutex);
        if (ready) return;
    }

    QList<Entry> built;
    for (const QString &root : roots()) {
        QDir dir(root);
        /* Created rather than merely checked: a folder the user is supposed to drop
           files into has to exist before they can find it. */
        if (!dir.exists()) dir.mkpath(".");
        if (!dir.exists()) continue;

        QDirIterator it(root, {"*.cube", "*.CUBE", "*.png", "*.PNG"}, QDir::Files,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString path = it.next();
            const QFileInfo fi(path);
            const QString ext = fi.suffix().toLower();

            Entry e;
            /* THE KEY: the path under this root, extension dropped, forward slashes. */
            e.key = dir.relativeFilePath(path);
            const int dot = e.key.lastIndexOf(QLatin1Char('.'));
            if (dot > 0) e.key = e.key.left(dot);
            e.label = e.key.section(QLatin1Char('/'), -1);
            e.path = path;

            /* Cheap validation only -- enough to keep a junk file out of the menu without
               parsing every table at sweep time. A .cube's title is worth having now
               because the tooltip wants it and the file is already open. */
            if (ext == QLatin1String(kCubeExt)) {
                QFile f(path);
                if (!f.open(QIODevice::ReadOnly)) continue;
                if (f.size() > kMaxCubeBytes) continue;
                /* Just the head: the size keyword is always near the top, and reading
                   60 MB per file to populate a menu would be absurd. */
                const QByteArray head = f.read(4096);
                const QByteArray upper = head.toUpper();
                if (!upper.contains("LUT_3D_SIZE") && !upper.contains("LUT_1D_SIZE"))
                    continue;
                e.title = titleFrom(head);
            } else if (ext == QLatin1String(kHaldExt)) {
                /* HEADER ONLY. QImageReader::size() does not decode, so a 4096x4096 PNG
                   is rejected for nothing. */
                QImageReader r(path);
                const QSize sz = r.size();
                if (!sz.isValid() || sz.width() != sz.height()) continue;
                if (sz.width() > LutParse::MaxHaldWidth()) continue;
                const int level = int(std::lround(std::cbrt(double(sz.width()))));
                if (level < 2 || level * level * level != sz.width()) continue;
            } else {
                continue;
            }
            built << e;
        }
    }

    std::sort(built.begin(), built.end(), [](const Entry &a, const Entry &b) {
        return a.key.localeAwareCompare(b.key) < 0;
    });

    {
        QMutexLocker lock(&mutex);
        index = built;
        ready = true;
        started = true;
    }
    /* Queued: scan() may be on a worker and the panel's slots touch widgets. */
    QMetaObject::invokeMethod(this, "indexChanged", Qt::QueuedConnection);
}

QList<LutStore::Entry> LutStore::entries()
{
    QMutexLocker lock(&mutex);
    if (!ready) return {};
    return index;
}

std::shared_ptr<const Lut3d::Table> LutStore::lut(const QString &key)
{
    if (key.isEmpty()) return nullptr;

    QString path;
    {
        QMutexLocker lock(&mutex);
        if (ready) {
            auto cached = parsed.constFind(key);
            if (cached != parsed.constEnd()) return cached.value();
            for (const Entry &e : std::as_const(index))
                if (e.key == key) { path = e.path; break; }
            if (path.isEmpty()) return nullptr;       // indexed, and not there
        }
    }

    if (path.isEmpty()) {
        /* Not indexed yet -- see the header. Build it here rather than answer "not
           ready", because this call may be the export path with no panel behind it. */
        scan();
        QMutexLocker lock(&mutex);
        auto cached = parsed.constFind(key);
        if (cached != parsed.constEnd()) return cached.value();
        for (const Entry &e : std::as_const(index))
            if (e.key == key) { path = e.path; break; }
        if (path.isEmpty()) return nullptr;
    }

    /* Parsed OUTSIDE the lock: file I/O, reachable from a render thread while the GUI
       thread is asking for the menu. Two threads racing to parse the same look waste one
       parse and then agree -- cheaper than serialising every reader behind a read. */
    Lut3d::Table t;
    QString err;
    bool ok = false;
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == QLatin1String(kCubeExt)) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) ok = LutParse::ParseCube(f.readAll(), t, &err);
    } else {
        QImageReader r(path);
        const QImage img = r.read();
        ok = LutParse::FromHald(img, t, &err);
    }
    if (!ok || !t.isValid()) return nullptr;

    auto sp = std::make_shared<const Lut3d::Table>(std::move(t));

    QMutexLocker lock(&mutex);
    auto cached = parsed.constFind(key);
    if (cached != parsed.constEnd()) return cached.value();
    parsed.insert(key, sp);
    return sp;
}
