/*
    Every MaskTool survives a sidecar round trip; one past the last does not.

    WHY THIS TEST EXISTS. EditStack's load-time sanitize drops a mask whose tool is past
    the end of the enum (corruption, or a tool from a newer build). Its bound was a
    literal MaskTool::RefineBrush, and when Segmenter was appended after it nobody moved
    the bound -- so every Segmenter mask was dropped on load with an "unknown tool"
    popup, and a preview top-up then wrote the stripped stack back to the sidecar. The
    bound is now kLastMaskTool; this pins that it covers every tool.
*/

#include <QtTest>

#include "Develop/editstack.h"

class TestMaskTools : public QObject
{
    Q_OBJECT

private slots:
    void everyToolSurvivesTheRoundTrip();
    void aToolPastTheLastIsDropped();
};

static EditStack stackWithTool(int tool)
{
    EditStack s;
    s.scopes.append(EditScope());           // Global
    EditScope mask;
    mask.name = QStringLiteral("Mask 1");
    MaskComponent c;
    c.tool = tool;
    mask.components.append(c);
    s.scopes.append(mask);
    return s;
}

void TestMaskTools::everyToolSurvivesTheRoundTrip()
{
    for (int tool = 0; tool <= int(kLastMaskTool); ++tool) {
        EditStack::Status st = EditStack::Status::Absent;
        QStringList issues;
        const EditStack back =
            EditStack::fromBase64(stackWithTool(tool).toBase64(), &st, &issues);
        QCOMPARE(int(st), int(EditStack::Status::Ok));
        QVERIFY2(issues.isEmpty(), qPrintable(QString("tool %1: %2")
                                              .arg(tool).arg(issues.join("; "))));
        QCOMPARE(back.scopes.size(), 2);
        QCOMPARE(back.scopes.at(1).components.size(), 1);
        QCOMPARE(back.scopes.at(1).components.at(0).tool, tool);
    }
    /* Segmenter by name: the tool that was being dropped. */
    QVERIFY(int(kLastMaskTool) >= int(MaskTool::Segmenter));
}

void TestMaskTools::aToolPastTheLastIsDropped()
{
    QStringList issues;
    const EditStack back = EditStack::fromBase64(
        stackWithTool(int(kLastMaskTool) + 1).toBase64(), nullptr, &issues);
    QCOMPARE(back.scopes.at(1).components.size(), 0);
    QVERIFY(!issues.isEmpty() && issues.first().contains("unknown tool"));
}

QTEST_GUILESS_MAIN(TestMaskTools)
#include "tst_masktools.moc"
