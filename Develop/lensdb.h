#ifndef LENSDB_H
#define LENSDB_H

/*
    LENS DATABASE: lensfun's lens and camera calibrations, read from its XML files, and
    the lookup that turns one photo's EXIF into the profile Develop/lensprofile.h applies.

    WHERE THE DATA COMES FROM. The XML files in Develop/LensDb/db (lensfun upstream,
    CC BY-SA 3.0) are compiled into the binary under the :/lensfun resource prefix
    (Develop/LensDb/lensfun.qrc). Then every .xml in <AppDataLocation>/lensfun is read
    on top: a camera or lens defined there REPLACES the bundled one with the same
    identity (Lens::id), which is how a user adds a lens or takes a newer database
    without a rebuild. See Develop/LensDb/README.md.

    LOADED ONCE, LAZILY, THREAD-SAFE. The first instance() caller parses everything
    (~5 MB of XML) under std::call_once; after that the database is immutable and every
    query is lock-free. Render workers and the GUI both read it.

    MATCHING (findCamera / findLens) follows lensfun's own rules:
      - A camera matches when its maker and model equal the EXIF ones, case and spacing
        ignored. Winnow's model strings are maker-prefixed (canonicalCameraModel), so the
        model also matches with the maker's first word in front of it.
      - A lens matches by WORDS. Lensfun splits a lens name into runs of digits,
        punctuation and everything else, drops single punctuation marks and a lone "f",
        and ignores order and case: "AF-S NIKKOR 24-70mm f/2.8G ED" is af s nikkor 24 70
        mm 2 8 g ed. The score is the Dice overlap of the two word multisets. The
        candidate must also fit the camera's mount (when the camera is known), cover the
        shot's focal length, and have been calibrated on a sensor no smaller than the
        camera's (lensfun's 0.96 crop-ratio rule).
    The user can override the match (EditParams::lensProfile holds a lens id).

    INTERPOLATION (interpolate) follows lensfun's scheme. Distortion and TCA: a Hermite
    spline over focal length through the two calibrations either side and their
    neighbours. Before the spline, each term is multiplied by its calibration's focal
    length (these terms fall roughly as 1/f); afterwards it is divided by the shot's.
    TCA's constant terms (kr, kb, vr, vb) are not scaled. Vignetting: inverse-distance
    weighting (power 3.5) over (focal normalised to the zoom range, 4/aperture,
    0.1/distance). No vignetting is given when the nearest calibration is further than 1
    in that space.
*/

#include "Develop/lensprofile.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace LensDb {

struct DistPoint { float focal = 0.0f; float t[3] = {0.0f, 0.0f, 0.0f}; };
struct TcaPoint  { float focal = 0.0f; float t[6] = {1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f}; };
struct VigPoint  { float focal = 0.0f, aperture = 0.0f, distance = 1000.0f;
                   float k[3] = {0.0f, 0.0f, 0.0f}; };

struct Lens {
    QString maker;
    QString model;              // the canonical <model> (the one without lang=)
    QStringList altModels;      // <model lang="..."> spellings, matched too
    QStringList mounts;
    float cropFactor = 1.0f;
    float aspect = 1.5f;
    float centerX = 0.0f, centerY = 0.0f;
    float minFocal = 0.0f, maxFocal = 0.0f;
    QString type;               // "" / rectilinear / fisheye / ...

    LensProfile::DistModel distModel = LensProfile::DistModel::None;
    QVector<DistPoint> dist;    // sorted by focal
    LensProfile::TcaModel tcaModel = LensProfile::TcaModel::None;
    QVector<TcaPoint> tca;      // sorted by focal
    QVector<VigPoint> vig;

    /* Stable identity for EditParams::lensProfile: "maker|model|crop". The crop factor
       is part of it because the database calibrates some lenses twice under one name --
       once on full frame, once on APS-C -- and those are different profiles. */
    QString id() const
    {
        return maker + QLatin1Char('|') + model + QLatin1Char('|')
               + QString::number(double(cropFactor), 'g', 6);
    }
    /* What the panel shows: the model, prefixed by the maker when it does not already
       start with it. */
    QString displayName() const;
    bool hasCalibration() const { return !dist.isEmpty() || !tca.isEmpty() || !vig.isEmpty(); }
};

struct Camera {
    QString maker, model, variant, mount;
    float cropFactor = 1.0f;
};

/* The EXIF a lookup needs. focal / aperture 0 = unknown; distance in metres, 0 = unknown
   (vignetting then assumes 1000 m, lensfun's "infinity"). */
struct Shot {
    QString cameraMake, cameraModel, lens;
    float focal = 0.0f, aperture = 0.0f, distance = 0.0f;
};

/* The outcome of resolve(): the lens used, and the profile for this shot, or why not. */
struct Resolved {
    const Lens *lens = nullptr;
    const Camera *camera = nullptr;
    bool manual = false;            // the user picked the lens
    LensProfile::Coeffs coeffs;
    QString reason;                 // empty when ok(); else why not, for the panel
    bool ok() const { return lens && coeffs.any(); }
};

class Database
{
public:
    static const Database &instance();      // loads on first call (blocking)

    const Camera *findCamera(const QString &make, const QString &model) const;
    /* Best lens for the shot, or null. score (optional) receives the match score. */
    const Lens *findLens(const Shot &shot, const Camera *camera, double *score = nullptr) const;
    const Lens *lensById(const QString &id) const;
    /* Lenses that fit the camera's mount (every lens when the camera is unknown),
       sorted by displayName -- the panel's manual picker. Only lenses that carry a
       calibration are listed. */
    QVector<const Lens *> lensesFor(const Camera *camera) const;

    /* Interpolate lens's calibration at the shot. false when the lens has nothing usable
       for this camera (calibrated on a smaller sensor) or at this focal length. */
    bool interpolate(const Lens &lens, float cameraCrop, float focal, float aperture,
                     float distance, LensProfile::Coeffs &out) const;

    int lensCount() const { return lenses.size(); }
    int cameraCount() const { return cameras.size(); }
    /* Where user override XML is read from (may not exist). */
    static QString userDir();

    /* Parse one lensfun XML document into this database (later definitions replace
       earlier ones with the same identity). Public for the tests. */
    void parseXml(const QByteArray &xml);
    void finish();                           // sort calibrations, derive focal ranges

    /* lensfun's word split, exposed for the tests. */
    static QStringList words(const QString &name);

private:
    Database() = default;
    void load();
    bool mountFits(const Lens &lens, const QString &cameraMount) const;

    QVector<Lens> lenses;
    QVector<Camera> cameras;
    QHash<QString, QStringList> mountCompat;     // mount -> compatible mounts
    QHash<QString, int> lensIndex;               // id -> index
    QHash<QString, int> cameraIndex;             // normalised maker|model -> index

    friend struct TestAccess;
};

/* The single call the render and the panel make: find (or take the user's) lens and
   interpolate it for this shot. overrideId empty = automatic. */
Resolved resolve(const Shot &shot, const QString &overrideId = QString());

/* Parse the database on a pool thread now, so the first Develop image does not wait
   ~0.1 s for it. Harmless if something gets there first (std::call_once). */
void preload();

}   // namespace LensDb

#endif // LENSDB_H
