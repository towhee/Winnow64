#include <QtTest>

#include "Develop/editrotate.h"

/*
    A Develop recipe follows the photo through a rotation (Develop/editrotate.h).

    Everything positional is stored normalized in the ORIENTED frame, so Cmd+] used to
    leave every crop, gradient, brush stroke and spot on different content. These pin
    the mapping for each stored form, and the property that matters most: a turn and its
    inverse leave the recipe where it was, and four quarter turns come full circle.
*/

namespace {

QJsonObject obj(const QString &json)
{
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

bool near(double a, double b) { return qAbs(a - b) < 1e-9; }

EditStack sample()
{
    EditStack s;
    s.scopes.append(EditScope());
    EditScope mask;
    mask.name = "Mask 1";
    MaskComponent lin;
    lin.tool = int(MaskTool::LinearGradient);
    lin.paramsJson = R"({"x1":0.2,"y1":0.1,"x2":0.7,"y2":0.4})";
    MaskComponent rad;
    rad.tool = int(MaskTool::RadialGradient);
    rad.paramsJson = R"({"cx":0.3,"cy":0.6,"rx":0.1,"ry":0.25,"angle":30})";
    MaskComponent brush;
    brush.tool = int(MaskTool::Brush);
    brush.paramsJson =
        R"({"size":20,"flow":80,"strokes":[{"size":15,"erase":false,"pts":[0.1,0.2,0.3,0.4]}]})";
    mask.components << lin << rad << brush;
    s.scopes.append(mask);
    FillSpot spot;
    spot.paramsJson = R"({"size":0.05,"feather":40,"kind":"spot","pts":[0.8,0.9]})";
    s.spots << spot;
    s.geometry.cropX = 0.1; s.geometry.cropY = 0.2;
    s.geometry.cropW = 0.3; s.geometry.cropH = 0.4;
    s.geometry.hasWarp = true;
    const double q[8] = {0.05,0.1, 0.9,0.02, 0.95,0.9, 0.1,0.97};
    for (int i = 0; i < 8; ++i) s.geometry.quad[i] = q[i];
    return s;
}

// Every positional value of a stack, flattened, for a tolerant comparison.
QList<double> positions(const EditStack &s)
{
    QList<double> v;
    for (const EditScope &l : s.scopes)
        for (const MaskComponent &m : l.components) {
            const QJsonObject o = obj(m.paramsJson);
            for (const char *k : {"x1","y1","x2","y2","cx","cy","rx","ry","angle"})
                if (o.contains(k)) v << o.value(k).toDouble();
            for (const QJsonValue &st : o.value("strokes").toArray())
                for (const QJsonValue &p : st.toObject().value("pts").toArray())
                    v << p.toDouble();
        }
    for (const FillSpot &f : s.spots)
        for (const QJsonValue &p : obj(f.paramsJson).value("pts").toArray()) v << p.toDouble();
    const Geometry &g = s.geometry;
    v << g.cropX << g.cropY << g.cropW << g.cropH;
    for (double q : g.quad) v << q;
    return v;
}

bool samePositions(const EditStack &a, const EditStack &b)
{
    const QList<double> x = positions(a), y = positions(b);
    if (x.size() != y.size()) return false;
    for (int i = 0; i < x.size(); ++i) if (!near(x[i], y[i])) return false;
    return true;
}

}  // namespace

class tst_editrotate : public QObject
{
    Q_OBJECT

private slots:
    void pointMapsCorners();
    void linearGradientPointsTurn();
    void radialSwapsRadiiOnQuarterTurnsOnly();
    void strokesTurnAndKeepTheirSettings();
    void spotTurns();
    void cropCoversTheSameContent();
    void quadIsRelabelled();
    void turnAndInverseRestore();
    void fourQuarterTurnsComeFullCircle();
    void toneOnlyRecipeReportsNothingMoved();
    void notARotationIsANoOp();
};

void tst_editrotate::pointMapsCorners()
{
    // clockwise: TL -> TR -> BR -> BL -> TL
    QCOMPARE(EditRotate::point(0, 0, 90), QPointF(1, 0));
    QCOMPARE(EditRotate::point(1, 0, 90), QPointF(1, 1));
    QCOMPARE(EditRotate::point(1, 1, 90), QPointF(0, 1));
    QCOMPARE(EditRotate::point(0, 1, 90), QPointF(0, 0));
    // counter-clockwise is the inverse
    QCOMPARE(EditRotate::point(1, 0, 270), QPointF(0, 0));
    QCOMPARE(EditRotate::point(0.25, 0.75, 180), QPointF(0.75, 0.25));
}

void tst_editrotate::linearGradientPointsTurn()
{
    const QJsonObject o = obj(EditRotate::maskParams(int(MaskTool::LinearGradient),
        R"({"x1":0.2,"y1":0.1,"x2":0.7,"y2":0.4})", 90));
    QVERIFY(near(o.value("x1").toDouble(), 0.9));
    QVERIFY(near(o.value("y1").toDouble(), 0.2));
    QVERIFY(near(o.value("x2").toDouble(), 0.6));
    QVERIFY(near(o.value("y2").toDouble(), 0.7));
}

void tst_editrotate::radialSwapsRadiiOnQuarterTurnsOnly()
{
/*
    rx is a fraction of W and ry of H. After a quarter turn W and H swap, and the ellipse
    (a,b) turned 90 degrees is the ellipse (b,a) at the same angle -- so the normalized
    radii simply swap and the angle stays.
*/
    const QString src = R"({"cx":0.3,"cy":0.6,"rx":0.1,"ry":0.25,"angle":30})";
    const QJsonObject q = obj(EditRotate::maskParams(int(MaskTool::RadialGradient), src, 90));
    QVERIFY(near(q.value("cx").toDouble(), 0.4));
    QVERIFY(near(q.value("cy").toDouble(), 0.3));
    QVERIFY(near(q.value("rx").toDouble(), 0.25));
    QVERIFY(near(q.value("ry").toDouble(), 0.1));
    QVERIFY(near(q.value("angle").toDouble(), 30));

    const QJsonObject h = obj(EditRotate::maskParams(int(MaskTool::RadialGradient), src, 180));
    QVERIFY(near(h.value("rx").toDouble(), 0.1));
    QVERIFY(near(h.value("ry").toDouble(), 0.25));
}

void tst_editrotate::strokesTurnAndKeepTheirSettings()
{
    const QString src =
        R"({"size":20,"flow":80,"autoMask":true,"strokes":[{"size":15,"erase":true,"pts":[0.1,0.2,0.3,0.4]}]})";
    for (int tool : {int(MaskTool::Brush), int(MaskTool::Object)}) {
        const QJsonObject o = obj(EditRotate::maskParams(tool, src, 270));
        QCOMPARE(o.value("size").toInt(), 20);
        QCOMPARE(o.value("flow").toInt(), 80);
        QCOMPARE(o.value("autoMask").toBool(), true);
        const QJsonObject st = o.value("strokes").toArray().at(0).toObject();
        QCOMPARE(st.value("size").toInt(), 15);         // % of the long edge: unchanged
        QCOMPARE(st.value("erase").toBool(), true);
        const QJsonArray p = st.value("pts").toArray();
        QVERIFY(near(p.at(0).toDouble(), 0.2));         // (0.1,0.2) -> (0.2, 0.9)
        QVERIFY(near(p.at(1).toDouble(), 0.9));
        QVERIFY(near(p.at(2).toDouble(), 0.4));         // (0.3,0.4) -> (0.4, 0.7)
        QVERIFY(near(p.at(3).toDouble(), 0.7));
    }
}

void tst_editrotate::spotTurns()
{
    const QJsonObject o = obj(EditRotate::spotParams(
        R"({"size":0.05,"kind":"fill","pts":[0.8,0.9],"strokes":[{"size":0.1,"pts":[0.5,0.25]}]})",
        180));
    QVERIFY(near(o.value("pts").toArray().at(0).toDouble(), 0.2));
    QVERIFY(near(o.value("pts").toArray().at(1).toDouble(), 0.1));
    const QJsonArray sp = o.value("strokes").toArray().at(0).toObject().value("pts").toArray();
    QVERIFY(near(sp.at(0).toDouble(), 0.5));
    QVERIFY(near(sp.at(1).toDouble(), 0.75));
    QCOMPARE(o.value("kind").toString(), QString("fill"));
    QVERIFY(near(o.value("size").toDouble(), 0.05));
}

void tst_editrotate::cropCoversTheSameContent()
{
/*
    The rotated crop rectangle must be exactly the image of the old one: its four corners
    are the old corners, turned.
*/
    for (int deg : {90, 180, 270}) {
        Geometry g;
        g.cropX = 0.1; g.cropY = 0.2; g.cropW = 0.3; g.cropH = 0.4;
        const Geometry before = g;
        EditRotate::geometry(g, deg);
        const QRectF after(g.cropX, g.cropY, g.cropW, g.cropH);
        for (const QPointF &c : {QPointF(before.cropX, before.cropY),
                                 QPointF(before.cropX + before.cropW, before.cropY),
                                 QPointF(before.cropX, before.cropY + before.cropH),
                                 QPointF(before.cropX + before.cropW,
                                         before.cropY + before.cropH)}) {
            const QPointF t = EditRotate::point(c.x(), c.y(), deg);
            const bool onCorner =
                (near(t.x(), after.left()) || near(t.x(), after.left() + after.width()))
                && (near(t.y(), after.top()) || near(t.y(), after.top() + after.height()));
            QVERIFY2(onCorner, qPrintable(QString("deg %1").arg(deg)));
        }
    }
}

void tst_editrotate::quadIsRelabelled()
{
/*
    TL,TR,BR,BL order must still mean top-left first after the turn, or the warp would
    flip or twist the picture.
*/
    Geometry g;
    const double q[8] = {0.05,0.1, 0.9,0.02, 0.95,0.9, 0.1,0.97};
    for (int i = 0; i < 8; ++i) g.quad[i] = q[i];
    EditRotate::geometry(g, 90);
    // clockwise: the old BL (0.1,0.97) is the new TL
    const QPointF tl = EditRotate::point(0.1, 0.97, 90);
    QVERIFY(near(g.quad[0], tl.x()) && near(g.quad[1], tl.y()));
    // and it really is the top-left-most corner
    QVERIFY(g.quad[0] < 0.5 && g.quad[1] < 0.5);
    QVERIFY(g.quad[2] > 0.5 && g.quad[3] < 0.5);    // TR
    QVERIFY(g.quad[4] > 0.5 && g.quad[5] > 0.5);    // BR
    QVERIFY(g.quad[6] < 0.5 && g.quad[7] > 0.5);    // BL
}

void tst_editrotate::turnAndInverseRestore()
{
    EditStack s = sample();
    const EditStack original = s;
    QVERIFY(EditRotate::stack(s, 90));
    QVERIFY(!samePositions(s, original));
    EditRotate::stack(s, 270);
    QVERIFY(samePositions(s, original));
    EditRotate::stack(s, 180);
    EditRotate::stack(s, 180);
    QVERIFY(samePositions(s, original));
}

void tst_editrotate::fourQuarterTurnsComeFullCircle()
{
    EditStack s = sample();
    const EditStack original = s;
    for (int i = 0; i < 4; ++i) EditRotate::stack(s, 90);
    QVERIFY(samePositions(s, original));
}

void tst_editrotate::toneOnlyRecipeReportsNothingMoved()
{
/*
    A recipe of tone and colour has nothing positional -- but its developed previews are
    still stale, which is why DevelopProperties::rotateImageEdits drops them regardless
    of this result.
*/
    EditStack s;
    s.scopes.append(EditScope());
    s.scopes[0].params.exposure = 0.5f;
    QVERIFY(!EditRotate::stack(s, 90));
    QVERIFY(s.geometry.cropIsIdentity());
}

void tst_editrotate::notARotationIsANoOp()
{
    EditStack s = sample();
    const EditStack original = s;
    QVERIFY(!EditRotate::stack(s, 45));
    QVERIFY(!EditRotate::stack(s, 0));
    QVERIFY(samePositions(s, original));
    QCOMPARE(EditRotate::maskParams(int(MaskTool::Brush), "not json", 90), QString("not json"));
}

QTEST_MAIN(tst_editrotate)
#include "tst_editrotate.moc"
