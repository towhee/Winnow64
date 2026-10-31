#include <QtTest>
#include <QJsonObject>
#include <cmath>
#include "Develop/colormixer.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"

/*
    Color Mixer (Develop/colormixer.h) -- the 8-band HSL mixer behind the Color panel's
    Mix mode. These pin what is easy to break and invisible in review: all-zero is the
    identity (the pipeline skips the stage on it), greys are never moved, a band's slider
    reaches its own colour and not the far side of the wheel, and the 24 fields round
    trip through the sidecar under their published key names.
*/
class tst_colormixer : public QObject
{
    Q_OBJECT

    struct Tables {
        float rot[ColorMixer::kHueN], sat[ColorMixer::kHueN], lum[ColorMixer::kHueN];
    };
    static Tables build(const float hue[8], const float sat[8], const float lum[8])
    {
        Tables t;
        ColorMixer::buildTables(hue, sat, lum, t.rot, t.sat, t.lum);
        return t;
    }
    /* A saturated OKLab colour at band b's centre. */
    static void bandLab(int b, float lab[3])
    {
        float c[8];
        ColorMixer::bandHues(c);
        lab[0] = 0.6f;
        lab[1] = 0.15f * std::cos(c[b]);
        lab[2] = 0.15f * std::sin(c[b]);
    }

private slots:
    void bandCentresAscend()
    {
        float c[8];
        ColorMixer::bandHues(c);
        for (int i = 1; i < 8; ++i) QVERIFY2(c[i] > c[i - 1], qPrintable(QString::number(i)));
    }

    void zeroIsIdentity()
    {
        const float z[8] = {};
        QVERIFY(ColorMixer::isIdentity(z, z, z));
        const Tables t = build(z, z, z);
        for (int b = 0; b < 8; ++b) {
            float lab[3], o[3];
            bandLab(b, lab);
            for (int k = 0; k < 3; ++k) o[k] = lab[k];
            ColorMixer::adjust(t.rot, t.sat, t.lum, o);
            for (int k = 0; k < 3; ++k) QCOMPARE(o[k], lab[k]);
        }
        QVERIFY(EditParams().mixerIsIdentity());
        QVERIFY(EditParams().isIdentity());
    }

    void greysStayPut()
    {
        float all[8];
        for (float &v : all) v = 100.0f;
        const Tables t = build(all, all, all);
        float lab[3] = {0.5f, 0.0f, 0.0f};
        QVERIFY(!ColorMixer::adjust(t.rot, t.sat, t.lum, lab));
        QCOMPARE(lab[0], 0.5f);
    }

    /* Blue saturation -100 greys the blues and leaves the yellows (opposite) alone. */
    void bandIsLocal()
    {
        const float z[8] = {};
        float sat[8] = {};
        sat[5] = -100.0f;                                // Blue
        const Tables t = build(z, sat, z);
        float blue[3], yellow[3];
        bandLab(5, blue);
        bandLab(2, yellow);
        const float yC = std::hypot(yellow[1], yellow[2]);
        ColorMixer::adjust(t.rot, t.sat, t.lum, blue);
        ColorMixer::adjust(t.rot, t.sat, t.lum, yellow);
        QVERIFY(std::hypot(blue[1], blue[2]) < 1e-4f);
        QCOMPARE(std::hypot(yellow[1], yellow[2]), yC);
    }

    /* The same value on every band is a uniform change: the blend sums to one. */
    void uniformIsUniform()
    {
        const float z[8] = {};
        float lum[8];
        for (float &v : lum) v = -40.0f;
        const Tables t = build(z, z, lum);
        for (int k = 0; k < ColorMixer::kHueN; ++k)
            QVERIFY(std::fabs(t.lum[k] - (-0.4f * ColorMixer::kLumMax)) < 1e-5f);
    }

    /* Red Hue + turns red toward orange, - toward magenta. */
    void hueRotatesTowardNeighbours()
    {
        const float z[8] = {};
        float hue[8] = {};
        hue[0] = 100.0f;
        const Tables up = build(hue, z, z);
        hue[0] = -100.0f;
        const Tables down = build(hue, z, z);
        float a[3], b[3];
        bandLab(0, a);
        bandLab(0, b);
        const float h0 = std::atan2(a[2], a[1]);
        ColorMixer::adjust(up.rot, up.sat, up.lum, a);
        ColorMixer::adjust(down.rot, down.sat, down.lum, b);
        QVERIFY(std::atan2(a[2], a[1]) > h0);
        QVERIFY(std::atan2(b[2], b[1]) < h0);
    }

    void sidecarRoundTrip()
    {
        EditParams p;
        p.mixAquaSat = 37.0f;
        p.mixRedHue = -12.0f;
        QVERIFY(!p.isIdentity());
        QVERIFY(!EditParams::groupIsDefault(p, EditParams::Group::Mixer));
        const QJsonObject o = EditStack::paramsToJson(p);
        QCOMPARE(o.value("mixAquaSat").toDouble(), 37.0);
        QVERIFY(!o.contains("mixBlueLum"));             // written only when set
        const EditParams q = EditStack::paramsFromJson(o);
        QCOMPARE(q.mixAquaSat, 37.0f);
        QCOMPARE(q.mixRedHue, -12.0f);
        EditParams r = q;
        EditParams::resetGroup(r, EditParams::Group::Mixer);
        QVERIFY(r.mixerIsIdentity());
    }

    void keyNames()
    {
        QCOMPARE(ColorMixer::key(ColorMixer::Hue, 0), QString("mixRedHue"));
        QCOMPARE(ColorMixer::key(ColorMixer::Lum, 7), QString("mixMagentaLum"));
        EditParams p;
        p.*EditParams::mixerMember(ColorMixer::Sat, 4) = 5.0f;
        QCOMPARE(p.mixAquaSat, 5.0f);
    }
};

QTEST_GUILESS_MAIN(tst_colormixer)
#include "tst_colormixer.moc"
