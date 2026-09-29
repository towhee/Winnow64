#ifndef QUERYBUILDER_H
#define QUERYBUILDER_H

#include <QDialog>
#include <QWidget>
#include <functional>

#include "Utilities/queryexpr.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QToolButton;
class QVBoxLayout;
class QueryGroupWidget;

/*
    THE QUERY BUILDER: a query (Utilities/queryexpr.h) as rows the user can read and
    change without learning the text grammar. General purpose -- the same widget edits
    the Filters Search row's ad hoc query and a saved Query in the Queries panel.

    A GROUP is "Match [all | any | none] of the following", holding RULES and further
    GROUPS; a RULE is [field] [operator] [value(s)]. Each rule row ends with - (remove)
    and + (add a rule after it; Alt/Opt+click adds a group, Lightroom's gesture), and each
    group header carries "+ Rule" and "+ Group" buttons, so nesting never depends on a
    modifier nobody is told about.

    THE TEXT BELOW THE ROWS IS THE SAME QUERY, live: every change to the rows rewrites
    it, and editing it (Return, or leaving the box) rebuilds the rows. A parse problem is
    shown under it and the rows keep what could be read. Four lines high and scrolling,
    because a real query outgrows one line; Shift+Return is a line break, which the
    grammar reads as a space.

    A VALUE THE LIBRARY DOES NOT HOLD IS RED: when a field offers typeahead (camera,
    keyword, collection ...) and nothing it offers contains what was typed, the value box
    turns red, so a misspelt name is seen before the query finds nothing.
*/
class QueryBuilderWidget : public QWidget
{
    Q_OBJECT

public:
    /*  The values a field's value box offers as it is typed into -- the camera models,
        keywords, collections ... the Library holds. MW supplies it. */
    using Suggestions = std::function<QStringList(const QString &fieldKey)>;

    explicit QueryBuilderWidget(QWidget *parent = nullptr);

    void setSuggestions(Suggestions s);
    const Suggestions &suggestions() const { return suggest; }
    void setExpr(const Query::Expr &e);
    Query::Expr expr() const;

signals:
    void changed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void rowsChanged();                 // rows -> text
    void textEdited();                  // text -> rows

    QueryGroupWidget *root = nullptr;
    QPlainTextEdit *textEdit = nullptr;
    QLabel *errorLabel = nullptr;
    Suggestions suggest;
    bool syncing = false;
};

/*
    The builder in a dialog.

    AdHoc       the Search row's query: Apply writes it back; "Save as Query..." keeps it
                in the Queries panel as well (saveAsQueryRequested) without closing.
    SavedQuery  a Query from the Queries panel: a Name field above the builder, and OK
                disabled until there is a name.

    TEST counts the loaded images the query matches, through a counter MW supplies
    (setCounter) -- the answer to "is this the query I meant?" before it is applied or
    saved. The result is cleared as soon as the query changes, so a stale count is never
    on screen beside a different query. No counter, no button.
*/
class QueryBuilderDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { AdHoc, SavedQuery };
    /*  (matching, total) loaded rows for a query. */
    using Counter = std::function<QPair<int, int>(const Query::Expr &)>;

    QueryBuilderDialog(Mode mode, QWidget *parent = nullptr);
    void setCounter(Counter c);

    QueryBuilderWidget *builder() const { return build; }
    void setName(const QString &name);
    QString name() const;
    void setExpr(const Query::Expr &e);
    Query::Expr expr() const;

signals:
    void saveAsQueryRequested(const Query::Expr &e);

private:
    Mode mode;
    QueryBuilderWidget *build = nullptr;
    QLineEdit *nameEdit = nullptr;
    QPushButton *okBtn = nullptr;
    QPushButton *testBtn = nullptr;
    QLabel *testResult = nullptr;
    Counter counter;
};

#endif // QUERYBUILDER_H
