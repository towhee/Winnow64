#ifndef QUERYBUILDER_H
#define QUERYBUILDER_H

#include <QDialog>
#include <QWidget>
#include <functional>

#include "Utilities/queryexpr.h"

class QComboBox;
class QLabel;
class QLineEdit;
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
    it, and editing it (Return) rebuilds the rows. A parse problem is shown under it and
    the rows keep what could be read.
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

private:
    void rowsChanged();                 // rows -> text
    void textEdited();                  // text -> rows

    QueryGroupWidget *root = nullptr;
    QLineEdit *textEdit = nullptr;
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
*/
class QueryBuilderDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode { AdHoc, SavedQuery };

    QueryBuilderDialog(Mode mode, QWidget *parent = nullptr);

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
};

#endif // QUERYBUILDER_H
