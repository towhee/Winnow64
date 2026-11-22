/*
    Unit tests for Undo/undomanager.h and Undo/undocommands.h -- the app-wide Edit > Undo
    stack: record after the fact (the push's redo is skipped), no re-recording while a
    command is applied, macro grouping, and the PathValues helpers.
*/

#include <QtTest>
#include "Undo/undomanager.h"
#include "Undo/undocommands.h"

namespace {

/* A field per path, standing in for a rating or a pick in the model. */
struct Store {
    QHash<QString, QVariant> v;
    int applies = 0;
    void apply(const PathValues &pv) {
        ++applies;
        for (const auto &p : pv) v[p.first] = p.second;
    }
};

/* What an edit does: change the store, then record. */
void edit(UndoManager &m, Store &s, const QString &path, const QVariant &to)
{
    PathValues before{{path, s.v.value(path)}};
    PathValues after{{path, to}};
    s.v[path] = to;
    m.push(new PathValuesCommand(path, before, after,
                                 [&s](const PathValues &pv) { s.apply(pv); }));
}

}   // namespace

class TstUndo : public QObject
{
    Q_OBJECT
private slots:
    void pushDoesNotReapply();
    void undoRedoRestoresValues();
    void newEditTruncatesRedo();
    void pushDiscardedWhileApplying();
    void macroGroupsAndOrders();
    void emptyMacroRecordsNothing();
    void macroOfOneIsThatCommand();
    void actionCommandRunsFunctions();
    void pruneUnchangedDropsNoOps();
    void stepText();
    void refusedStepStaysPut();
};

void TstUndo::pushDoesNotReapply()
{
    UndoManager m;
    Store s;
    edit(m, s, "a", 3);
    QCOMPARE(s.applies, 0);             // the edit already ran; push must not redo it
    QVERIFY(m.canUndo());
    QVERIFY(!m.canRedo());
    QCOMPARE(m.undoText(), QString("a"));
}

void TstUndo::undoRedoRestoresValues()
{
    UndoManager m;
    Store s;
    s.v["a"] = 1;
    edit(m, s, "a", 2);
    edit(m, s, "a", 3);
    m.undo();
    QCOMPARE(s.v["a"].toInt(), 2);
    m.undo();
    QCOMPARE(s.v["a"].toInt(), 1);
    QVERIFY(!m.canUndo());
    m.redo();
    QCOMPARE(s.v["a"].toInt(), 2);
    m.redo();
    QCOMPARE(s.v["a"].toInt(), 3);
    QVERIFY(!m.canRedo());
}

void TstUndo::newEditTruncatesRedo()
{
    UndoManager m;
    Store s;
    edit(m, s, "a", 1);
    edit(m, s, "a", 2);
    m.undo();
    QVERIFY(m.canRedo());
    edit(m, s, "b", 9);
    QVERIFY(!m.canRedo());
}

void TstUndo::pushDiscardedWhileApplying()
{
    /*  The real setters record as they run. Undo re-runs them, so a push made during an
        undo must vanish, or the undo would record itself and break the stack. */
    UndoManager m;
    Store s;
    s.v["a"] = 1;
    PathValues before{{"a", 1}}, after{{"a", 2}};
    s.v["a"] = 2;
    m.push(new PathValuesCommand("a", before, after, [&](const PathValues &pv) {
        QVERIFY(m.isApplying());
        s.apply(pv);
        edit(m, s, "x", 99);            // a setter recording from inside the undo
    }));
    m.undo();
    QVERIFY(!m.isApplying());
    QVERIFY(!m.canUndo());              // nothing new was recorded
    QVERIFY(m.canRedo());
    QCOMPARE(m.redoText(), QString("a"));
}

void TstUndo::macroGroupsAndOrders()
{
    UndoManager m;
    QStringList log;
    m.beginMacro("Group");
    for (const QString &n : {"1", "2", "3"}) {
        m.push(new ActionCommand(n, [&log, n] { log << "u" + n; },
                                 [&log, n] { log << "r" + n; }));
    }
    m.endMacro();
    QVERIFY(log.isEmpty());             // no redo when the group is pushed
    QCOMPARE(m.undoText(), QString("Group"));
    m.undo();
    QCOMPARE(log, QStringList({"u3", "u2", "u1"}));     // newest first
    log.clear();
    m.redo();
    QCOMPARE(log, QStringList({"r1", "r2", "r3"}));     // oldest first
    m.undo();
    QVERIFY(!m.canUndo());              // one step
}

void TstUndo::emptyMacroRecordsNothing()
{
    UndoManager m;
    {
        UndoMacro g(&m, "Nothing");
    }
    QVERIFY(!m.canUndo());
}

void TstUndo::macroOfOneIsThatCommand()
{
    UndoManager m;
    int undos = 0, redos = 0;
    {
        UndoMacro g(&m, "Outer");
        UndoMacro inner(&m, "Inner");   // nested: the outermost names the step
        m.push(new ActionCommand("one", [&] { ++undos; }, [&] { ++redos; }));
    }
    QCOMPARE(redos, 0);
    QCOMPARE(m.undoText(), QString("Outer"));
    m.undo();
    QCOMPARE(undos, 1);
    m.redo();
    QCOMPARE(redos, 1);
}

void TstUndo::actionCommandRunsFunctions()
{
    UndoManager m;
    int deg = 90;                       // a rotation that already happened
    m.push(new ActionCommand("Rotate", [&] { deg -= 90; }, [&] { deg += 90; }));
    m.undo();
    QCOMPARE(deg, 0);
    m.redo();
    QCOMPARE(deg, 90);
}

void TstUndo::pruneUnchangedDropsNoOps()
{
    PathValues before{{"a", "Picked"}, {"b", "Unpicked"}, {"c", 3}};
    PathValues after{{"a", "Picked"}, {"b", "Picked"}, {"c", 4}};
    pruneUnchanged(before, after);
    QCOMPARE(before.size(), 2);
    QCOMPARE(before.at(0).first, QString("b"));
    QCOMPARE(after.at(1).second.toInt(), 4);
}

void TstUndo::stepText()
{
    QCOMPARE(undoStepText("Rating", 1), QString("Rating"));
    QCOMPARE(undoStepText("Rating", 12), QString("Rating (12 images)"));
}

void TstUndo::refusedStepStaysPut()
{
    /*  A file operation's undo that cannot run (a load in progress, the file gone from
        the trash) refuses: nothing was changed, so the stack must not move either. */
    UndoManager m;
    bool allow = false;
    int undos = 0, redos = 0;
    m.push(new CheckedCommand("Delete",
                              [&] { if (!allow) return false; ++undos; return true; },
                              [&] { ++redos; return true; }));
    m.undo();
    QVERIFY(m.canUndo());               // refused: still offered
    QVERIFY(!m.canRedo());
    QCOMPARE(m.undoText(), QString("Delete"));
    QCOMPARE(redos, 0);                 // stepping back over the refusal re-applied nothing
    allow = true;
    m.undo();
    QCOMPARE(undos, 1);
    QVERIFY(!m.canUndo());
    m.redo();
    QCOMPARE(redos, 1);
}

QTEST_GUILESS_MAIN(TstUndo)
#include "tst_undo.moc"
