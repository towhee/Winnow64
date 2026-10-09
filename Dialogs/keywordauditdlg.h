#ifndef KEYWORDAUDITDLG_H
#define KEYWORDAUDITDLG_H

#include <QDialog>
#include <QHash>
#include <QVector>
#include "Main/keywordauditjob.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QProgressBar;
class QPushButton;
class QTableWidget;
class QTabWidget;
class FindingModel;

/*
    AUDIT KEYWORDS -- the review of what VisualAudit found (Utilities/visualaudit.h).

    A REVIEW LIST, NOT A BUTTON, like KeywordTidyDlg: every finding is a question for the
    user, never an automatic edit. Each one shows the image beside three of the user's
    own best examples of the keyword in question (and of the suggested alternative), so
    "does this look like my Heron pictures?" is answered by looking, not by trusting a
    score.

    DECISIONS ARE KEYS, because a reviewed catalog is hundreds of findings:
        R  Replace the keyword with the suggested one     (Suspect with an alternative)
        D  Remove the keyword                             (Suspect)
        A  Add the keyword                                (Missing)
        C  The keywords are right                         (either)
        U  Undecide          S  Show the image in Winnow
    and the list moves on to the next undecided finding.

    EDITS WAIT FOR Apply; VERDICTS DO NOT. Replace/Remove/Add are collected and written
    in one pass when the user presses Apply (MW::applyKeywordAuditFixes), so a review can
    be abandoned without having touched a file. "Keywords are right" is saved at once
    (Datamodel/keywordauditstore.h) -- it writes no image, and it is what stops the next
    audit asking again.

    NON-MODAL: the user can open an image in Winnow ("Show") and come back.

    The audit itself runs on MW's KeywordAuditJob; this dialog only asks for a run and
    shows its progress and outcome.
*/
class KeywordAuditDlg : public QDialog
{
    Q_OBJECT

public:
    /* One image's keyword change. remove / add are display paths; either may be empty. */
    struct Fix
    {
        QString path;
        QString remove;
        QString add;
    };

    explicit KeywordAuditDlg(QWidget *parent = nullptr);
    ~KeywordAuditDlg() override;

    /* Why the audit cannot run here (no inference backend, no model, no catalog), or
       empty. Shown greyed in place of the Run button's action. */
    void setUnavailableReason(const QString &reason);

    void setRunning(bool running);
    void setProgress(const QString &stage, int done, int total);
    void setOutcome(const KeywordAuditOutcome &outcome);

    /* MW reports how Apply went; the applied findings are marked done. */
    void applied(int written, int failed);

    int pendingCount() const;

signals:
    void runRequested();
    void stopRequested();
    void applyRequested(const QVector<KeywordAuditDlg::Fix> &fixes);
    void showImageRequested(const QString &path);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    enum Decision { Undecided, Replace, Remove, Add, Correct, Applied };

    void buildUi();
    void refilter();
    void showCurrent();
    void decide(Decision d);
    void advance();
    void apply();
    void updateSummary();
    void fillKeywordTable();
    QPixmap thumb(const QString &path, int side);
    QString leaf(const QString &path) const;

    KeywordAuditOutcome outcome;
    QVector<int> decision;                      // per finding
    QHash<QString, QVector<int>> exemplars;     // keyword -> image indices
    QHash<QString, QString> skippedKeywords;
    QVector<int> pendingApply;                  // findings sent with the last Apply

    QTabWidget *tabs = nullptr;
    QLabel *stageLabel = nullptr;
    QProgressBar *progressBar = nullptr;
    QPushButton *runBtn = nullptr;
    QComboBox *kindCombo = nullptr;
    QLineEdit *keywordFilter = nullptr;
    QCheckBox *hideDecided = nullptr;
    QListView *list = nullptr;
    FindingModel *model = nullptr;

    QLabel *bigImage = nullptr;
    QLabel *claimLabel = nullptr;
    QLabel *kwExLabel = nullptr;
    QLabel *kwEx[3] = {};
    QLabel *altExLabel = nullptr;
    QLabel *altEx[3] = {};
    QLabel *pathLabel = nullptr;
    QLabel *tagsLabel = nullptr;
    QPushButton *replaceBtn = nullptr;
    QPushButton *removeBtn = nullptr;
    QPushButton *addBtn = nullptr;
    QPushButton *correctBtn = nullptr;
    QPushButton *undoBtn = nullptr;
    QPushButton *showBtn = nullptr;
    QPushButton *skipBtn = nullptr;

    QTableWidget *kwTable = nullptr;
    QLabel *summaryLabel = nullptr;
    QPushButton *applyBtn = nullptr;
    QString unavailable;
    bool running = false;
};

Q_DECLARE_METATYPE(KeywordAuditDlg::Fix)

#endif // KEYWORDAUDITDLG_H
