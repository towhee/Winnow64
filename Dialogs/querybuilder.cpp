#include "Dialogs/querybuilder.h"

#include <QApplication>
#include <QComboBox>
#include <QCompleter>
#include <QDialogButtonBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QStringListModel>
#include <QToolButton>
#include <QVBoxLayout>

using namespace Query;

/*
    The rows are plain widgets with callbacks rather than Q_OBJECTs: they live and die
    inside one QueryBuilderWidget, which is the only thing that listens to them.
*/

class QueryRuleWidget;

class QueryGroupWidget : public QFrame
{
public:
    QueryGroupWidget(bool isRoot, const QueryBuilderWidget::Suggestions *suggest,
                     std::function<void()> changed, QueryGroupWidget *parentGroup);

    void setNode(const Node &n);
    Node node() const;
    void addRule(const Node &n, QWidget *after = nullptr);
    void addGroup(const Node &n, QWidget *after = nullptr);
    void removeChild(QWidget *w);

private:
    int indexAfter(QWidget *after) const;

    bool isRoot;
    const QueryBuilderWidget::Suggestions *suggest;
    std::function<void()> changed;
    QueryGroupWidget *parentGroup;
    QComboBox *matchBox = nullptr;
    QVBoxLayout *body = nullptr;
    QList<QWidget *> children;          // QueryRuleWidget or QueryGroupWidget, in order
};

class QueryRuleWidget : public QWidget
{
public:
    QueryRuleWidget(QueryGroupWidget *owner, const QueryBuilderWidget::Suggestions *suggest,
                    std::function<void()> changed);

    void setNode(const Node &n);
    Node node() const;

private:
    const Field *currentField() const;
    void fieldChanged();
    void opChanged();

    QueryGroupWidget *owner;
    const QueryBuilderWidget::Suggestions *suggest;
    std::function<void()> changed;
    QComboBox *fieldBox = nullptr;
    QComboBox *opBox = nullptr;
    QLineEdit *v1 = nullptr;
    QLineEdit *v2 = nullptr;
    QLabel *andLabel = nullptr;
    QStringListModel *suggestModel = nullptr;
    bool setting = false;
};

namespace {

QToolButton *smallButton(const QString &text, const QString &tip, QWidget *parent)
{
    // QToolButton, not QPushButton: the global QPushButton min-width would widen the row
    auto *b = new QToolButton(parent);
    b->setText(text);
    b->setToolTip(tip);
    b->setAutoRaise(false);
    return b;
}

/*  "a, b" -> {"a", "b"} for the any-of operators; one value otherwise. */
QStringList valuesFrom(const QString &text, bool list)
{
    if (!list) return {text.trimmed()};
    QStringList out;
    for (const QString &s : text.split(',')) if (!s.trimmed().isEmpty()) out << s.trimmed();
    return out;
}

}   // namespace

/* ---------------------------------------------------------------------------------
   Rule row
   --------------------------------------------------------------------------------- */

QueryRuleWidget::QueryRuleWidget(QueryGroupWidget *owner,
                                 const QueryBuilderWidget::Suggestions *suggest,
                                 std::function<void()> changed)
    : QWidget(owner), owner(owner), suggest(suggest), changed(std::move(changed))
{
    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(4);

    fieldBox = new QComboBox(this);
    for (const Field &f : fields()) fieldBox->addItem(f.label, f.key);
    opBox = new QComboBox(this);
    v1 = new QLineEdit(this);
    andLabel = new QLabel(tr("and"), this);
    v2 = new QLineEdit(this);
    v1->setClearButtonEnabled(true);
    v2->setClearButtonEnabled(true);
    // typeahead: one shared word list, one completer per box (see fieldChanged)
    suggestModel = new QStringListModel(this);
    for (QLineEdit *e : {v1, v2}) {
        auto *c = new QCompleter(suggestModel, e);
        c->setCaseSensitivity(Qt::CaseInsensitive);
        c->setFilterMode(Qt::MatchContains);
        e->setCompleter(c);
    }

    QToolButton *minus = smallButton("-", tr("Remove this rule"), this);
    QToolButton *plus = smallButton("+", tr("Add a rule after this one.\n"
                                            "Alt/Opt+click adds a group of rules."), this);

    row->addWidget(fieldBox);
    row->addWidget(opBox);
    row->addWidget(v1, 1);
    row->addWidget(andLabel);
    row->addWidget(v2, 1);
    row->addWidget(minus);
    row->addWidget(plus);

    connect(fieldBox, &QComboBox::currentIndexChanged, this, [this] { fieldChanged(); });
    connect(opBox, &QComboBox::currentIndexChanged, this, [this] { opChanged(); });
    connect(v1, &QLineEdit::textChanged, this, [this] { if (!setting) this->changed(); });
    connect(v2, &QLineEdit::textChanged, this, [this] { if (!setting) this->changed(); });
    connect(minus, &QToolButton::clicked, this, [this] { this->owner->removeChild(this); });
    connect(plus, &QToolButton::clicked, this, [this] {
        if (QApplication::keyboardModifiers() & Qt::AltModifier) {
            Node g = Node::group(Node::All);
            g.kids << Node::rule("text", Op::Contains, {});
            this->owner->addGroup(g, this);
        }
        else this->owner->addRule(Node::rule(node().field, Op::Contains, {}), this);
    });

    fieldChanged();
}

const Field *QueryRuleWidget::currentField() const
{
    return field(fieldBox->currentData().toString());
}

void QueryRuleWidget::fieldChanged()
{
    const Field *f = currentField();
    if (!f) return;
    const Op keep = opBox->count() ? Op(opBox->currentData().toInt()) : Op::Contains;
    const bool wasSetting = setting;
    setting = true;
    opBox->clear();
    int keepIdx = 0;
    const QList<Op> ops = opsFor(f->type);
    for (int i = 0; i < ops.size(); ++i) {
        opBox->addItem(opLabel(ops.at(i), f->type), int(ops.at(i)));
        if (ops.at(i) == keep) keepIdx = i;
    }
    opBox->setCurrentIndex(keepIdx);

    /*  TYPEAHEAD from what the Library holds, plus an Enum's own choices. */
    QStringList offer = f->values;
    if (suggest && *suggest) offer += (*suggest)(f->key);
    offer.removeDuplicates();
    /*  ONE COMPLETER PER BOX, made in the constructor; only its word list changes here.
        Replacing the completer instead crashed: QLineEdit::setCompleter DELETES a
        previous completer parented to the line edit, so a second pass through here --
        setNode picks the field and then calls this again -- deleted it twice. */
    suggestModel->setStringList(offer);
    for (QLineEdit *e : {v1, v2})
        e->setPlaceholderText(f->hint.isEmpty() && !f->values.isEmpty()
                                  ? f->values.join(", ") : f->hint);
    setting = wasSetting;
    opChanged();
}

void QueryRuleWidget::opChanged()
{
    const Op op = Op(opBox->currentData().toInt());
    v1->setVisible(!opTakesNoValue(op));
    andLabel->setVisible(opTakesTwoValues(op));
    v2->setVisible(opTakesTwoValues(op));
    const Field *f = currentField();
    if (f && (op == Op::AnyOf || op == Op::NoneOf))
        v1->setToolTip(tr("One value, or several separated by commas."));
    else v1->setToolTip(QString());
    if (!setting) changed();
}

void QueryRuleWidget::setNode(const Node &n)
{
    setting = true;
    const int fi = fieldBox->findData(n.field);
    fieldBox->setCurrentIndex(fi >= 0 ? fi : 0);
    fieldChanged();
    const int oi = opBox->findData(int(n.op));
    if (oi >= 0) opBox->setCurrentIndex(oi);
    opChanged();
    if (n.op == Op::AnyOf || n.op == Op::NoneOf) v1->setText(n.values.join(", "));
    else v1->setText(n.values.value(0));
    v2->setText(n.values.value(1));
    setting = false;
}

Node QueryRuleWidget::node() const
{
    const QString key = fieldBox->currentData().toString();
    const Op op = Op(opBox->currentData().toInt());
    QStringList vals;
    if (opTakesTwoValues(op)) vals << v1->text().trimmed() << v2->text().trimmed();
    else if (!opTakesNoValue(op))
        vals = valuesFrom(v1->text(), op == Op::AnyOf || op == Op::NoneOf);
    return Node::rule(key, op, vals);
}

/* ---------------------------------------------------------------------------------
   Group
   --------------------------------------------------------------------------------- */

QueryGroupWidget::QueryGroupWidget(bool isRoot,
                                   const QueryBuilderWidget::Suggestions *suggest,
                                   std::function<void()> changed,
                                   QueryGroupWidget *parentGroup)
    : QFrame(parentGroup), isRoot(isRoot), suggest(suggest), changed(std::move(changed)),
      parentGroup(parentGroup)
{
    /*  A nested group is outlined, so which rules it holds reads at a glance. */
    if (!isRoot) setFrameShape(QFrame::StyledPanel);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(isRoot ? 0 : 6, isRoot ? 0 : 4, isRoot ? 0 : 6, isRoot ? 0 : 6);
    outer->setSpacing(4);

    auto *head = new QHBoxLayout();
    head->setSpacing(4);
    head->addWidget(new QLabel(tr("Match"), this));
    matchBox = new QComboBox(this);
    matchBox->addItem(tr("all"), int(Node::All));
    matchBox->addItem(tr("any"), int(Node::Any));
    matchBox->addItem(tr("none"), int(Node::None));
    matchBox->setToolTip(tr("all: every rule below must hold\n"
                            "any: at least one must hold\n"
                            "none: no rule below may hold"));
    head->addWidget(matchBox);
    head->addWidget(new QLabel(tr("of the following:"), this));
    head->addStretch(1);
    QToolButton *addRuleBtn = smallButton(tr("+ Rule"), tr("Add a rule to this group"), this);
    QToolButton *addGroupBtn = smallButton(tr("+ Group"),
                                           tr("Add a group of rules inside this one"), this);
    head->addWidget(addRuleBtn);
    head->addWidget(addGroupBtn);
    if (!isRoot) {
        QToolButton *remove = smallButton("-", tr("Remove this group and its rules"), this);
        head->addWidget(remove);
        connect(remove, &QToolButton::clicked, this, [this] {
            this->parentGroup->removeChild(this);
        });
    }
    outer->addLayout(head);

    body = new QVBoxLayout();
    body->setContentsMargins(isRoot ? 0 : 12, 0, 0, 0);
    body->setSpacing(4);
    outer->addLayout(body);

    connect(matchBox, &QComboBox::currentIndexChanged, this, [this] { this->changed(); });
    connect(addRuleBtn, &QToolButton::clicked, this, [this] {
        addRule(Node::rule("text", Op::Contains, {}));
    });
    connect(addGroupBtn, &QToolButton::clicked, this, [this] {
        Node g = Node::group(Node::All);
        g.kids << Node::rule("text", Op::Contains, {});
        addGroup(g);
    });
}

int QueryGroupWidget::indexAfter(QWidget *after) const
{
    const int i = after ? children.indexOf(after) : -1;
    return i >= 0 ? i + 1 : children.size();
}

void QueryGroupWidget::addRule(const Node &n, QWidget *after)
{
    auto *w = new QueryRuleWidget(this, suggest, changed);
    w->setNode(n);
    const int at = indexAfter(after);
    children.insert(at, w);
    body->insertWidget(at, w);
    changed();
}

void QueryGroupWidget::addGroup(const Node &n, QWidget *after)
{
    auto *g = new QueryGroupWidget(false, suggest, changed, this);
    g->setNode(n);
    const int at = indexAfter(after);
    children.insert(at, g);
    body->insertWidget(at, g);
    changed();
}

void QueryGroupWidget::removeChild(QWidget *w)
{
    children.removeAll(w);
    body->removeWidget(w);
    w->hide();
    w->deleteLater();
    changed();
}

void QueryGroupWidget::setNode(const Node &n)
{
    /*  HIDDEN AS WELL AS DELETED LATER. A deferred delete posted before the dialog's
        exec() only runs once the dialog closes, so a row that was merely removed from
        the layout stayed on screen, painted at the group's top-left over "Match". */
    for (QWidget *w : std::as_const(children)) {
        body->removeWidget(w);
        w->hide();
        w->deleteLater();
    }
    children.clear();
    const QSignalBlocker b(matchBox);
    matchBox->setCurrentIndex(qMax(0, matchBox->findData(int(n.match))));
    for (const Node &k : n.kids) {
        if (k.kind == Node::Rule) {
            auto *w = new QueryRuleWidget(this, suggest, changed);
            w->setNode(k);
            children << w;
            body->addWidget(w);
        }
        else {
            auto *g = new QueryGroupWidget(false, suggest, changed, this);
            g->setNode(k);
            children << g;
            body->addWidget(g);
        }
    }
}

Node QueryGroupWidget::node() const
{
    Node n = Node::group(Node::Match(matchBox->currentData().toInt()));
    for (QWidget *w : children) {
        if (auto *r = dynamic_cast<QueryRuleWidget *>(w)) n.kids << r->node();
        else if (auto *g = dynamic_cast<QueryGroupWidget *>(w)) n.kids << g->node();
    }
    return n;
}

/* ---------------------------------------------------------------------------------
   QueryBuilderWidget
   --------------------------------------------------------------------------------- */

QueryBuilderWidget::QueryBuilderWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(6);

    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    auto *holder = new QWidget(scroll);
    auto *holderLayout = new QVBoxLayout(holder);
    holderLayout->setContentsMargins(0, 0, 0, 0);
    root = new QueryGroupWidget(true, &suggest, [this] { rowsChanged(); }, nullptr);
    root->setParent(holder);
    holderLayout->addWidget(root);
    holderLayout->addStretch(1);
    scroll->setWidget(holder);
    layout->addWidget(scroll, 1);

    layout->addWidget(new QLabel(tr("As text (edit and press Return to rebuild the rules):"),
                                 this));
    textEdit = new QLineEdit(this);
    textEdit->setToolTip(tr(
        "The same query, as the Search row reads it.\n\n"
        "heron OR eagle        either word\n"
        "\"great blue\"          a phrase\n"
        "-heron                must not appear\n"
        "rating:>=3            rating at least 3 (also < <= > = and 3..5)\n"
        "label:red,yellow      any of these; -label:red none of them\n"
        "keyword:\"Fauna|Bird\"  has this keyword or one beneath it\n"
        "title:=Heron  title:her*  title:*ron  title:*  -title:*\n"
        "captured:2024-05      a year, month or day; < > and a..b too\n"
        "(a OR b) c            brackets group; OR and NOT in capitals"));
    layout->addWidget(textEdit);
    errorLabel = new QLabel(this);
    errorLabel->setWordWrap(true);
    errorLabel->setStyleSheet("color: #d06060;");
    errorLabel->setVisible(false);
    layout->addWidget(errorLabel);

    connect(textEdit, &QLineEdit::returnPressed, this, &QueryBuilderWidget::textEdited);
    connect(textEdit, &QLineEdit::editingFinished, this, &QueryBuilderWidget::textEdited);

    setExpr(Expr());
}

void QueryBuilderWidget::setSuggestions(Suggestions s)
{
    suggest = std::move(s);
    setExpr(expr());                    // rebuild the rows so their typeahead has it
}

void QueryBuilderWidget::setExpr(const Expr &e)
{
    syncing = true;
    Node n = e.root;
    if (n.kind == Node::Rule) {
        Node g = Node::group(Node::All);
        g.kids << n;
        n = g;
    }
    if (n.kids.isEmpty()) n.kids << Node::rule("text", Op::Contains, {});
    root->setNode(n);
    textEdit->setText(e.toText());
    errorLabel->setText(e.error());
    errorLabel->setVisible(!e.error().isEmpty());
    syncing = false;
}

Expr QueryBuilderWidget::expr() const
{
    Expr e;
    e.root = root->node();
    /*  A rule left blank -- the empty row a new query starts with -- is not a rule. */
    std::function<void(Node &)> prune = [&prune](Node &n) {
        QList<Node> keep;
        for (Node &k : n.kids) {
            if (k.kind == Node::Rule) {
                const bool blank = !opTakesNoValue(k.op)
                                   && (k.values.isEmpty() || k.values.first().isEmpty());
                if (!blank) keep << k;
            }
            else { prune(k); keep << k; }
        }
        n.kids = keep;
    };
    prune(e.root);
    return e;
}

void QueryBuilderWidget::rowsChanged()
{
    if (syncing) return;
    syncing = true;
    textEdit->setText(expr().toText());
    errorLabel->setVisible(false);
    syncing = false;
    emit changed();
}

void QueryBuilderWidget::textEdited()
{
    if (syncing) return;
    const QString text = textEdit->text();
    if (text == expr().toText()) return;        // nothing new: keep the rows as they are
    setExpr(Expr::parse(text));
    emit changed();
}

/* ---------------------------------------------------------------------------------
   QueryBuilderDialog
   --------------------------------------------------------------------------------- */

QueryBuilderDialog::QueryBuilderDialog(Mode mode, QWidget *parent)
    : QDialog(parent), mode(mode)
{
    setWindowTitle(tr("Query Builder"));
    auto *layout = new QVBoxLayout(this);

    if (mode == SavedQuery) {
        auto *nameRow = new QHBoxLayout();
        nameRow->addWidget(new QLabel(tr("Name:"), this));
        nameEdit = new QLineEdit(this);
        nameRow->addWidget(nameEdit, 1);
        layout->addLayout(nameRow);
    }

    build = new QueryBuilderWidget(this);
    layout->addWidget(build, 1);

    auto *buttons = new QDialogButtonBox(this);
    okBtn = buttons->addButton(mode == SavedQuery ? tr("OK") : tr("Apply"),
                               QDialogButtonBox::AcceptRole);
    buttons->addButton(QDialogButtonBox::Cancel);
    if (mode == AdHoc) {
        QPushButton *save = buttons->addButton(tr("Save as Query..."),
                                               QDialogButtonBox::ActionRole);
        save->setToolTip(tr("Keep this query in the Queries panel as well."));
        connect(save, &QPushButton::clicked, this, [this] {
            emit saveAsQueryRequested(build->expr());
        });
    }
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    if (nameEdit) {
        auto sync = [this] { okBtn->setEnabled(!nameEdit->text().trimmed().isEmpty()); };
        connect(nameEdit, &QLineEdit::textChanged, this, sync);
        sync();
    }
    resize(760, 460);
}

void QueryBuilderDialog::setName(const QString &name)
{
    if (nameEdit) nameEdit->setText(name);
}

QString QueryBuilderDialog::name() const
{
    return nameEdit ? nameEdit->text().trimmed() : QString();
}

void QueryBuilderDialog::setExpr(const Expr &e) { build->setExpr(e); }

Expr QueryBuilderDialog::expr() const { return build->expr(); }
