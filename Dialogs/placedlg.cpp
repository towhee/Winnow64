#include "Dialogs/placedlg.h"
#include "Main/global.h"

#include <QButtonGroup>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

PlaceDlg::PlaceDlg(QWidget *parent) : QDialog(parent)
{
    if (G::isLogger) G::log("PlaceDlg::PlaceDlg");
    setObjectName("PlaceDlg");
    setWindowTitle(tr("New Place"));
    /* A tool window, not a modal dialog: the map behind it is where the shape is
       drawn. See the header. */
    setWindowFlag(Qt::Tool);
    setModal(false);
    setMinimumWidth(380);

    auto *layout = new QVBoxLayout(this);
    layout->setSpacing(8);

    auto *nameRow = new QHBoxLayout;
    nameRow->addWidget(new QLabel(tr("Name:"), this));
    nameEdit = new QLineEdit(this);
    nameEdit->setPlaceholderText(tr("e.g. Home, Banff, Lake Cowichan"));
    nameRow->addWidget(nameEdit, 1);
    layout->addLayout(nameRow);

    auto *typeRow = new QHBoxLayout;
    typeRow->addWidget(new QLabel(tr("Type:"), this));
    ellipseBtn = new QRadioButton(tr("Ellipse"), this);
    polygonBtn = new QRadioButton(tr("Polygon"), this);
    ellipseBtn->setToolTip(tr("An oval you move, size and turn with its handles"));
    polygonBtn->setToolTip(tr("A shape you outline by clicking its corners on the map"));
    auto *group = new QButtonGroup(this);
    group->addButton(ellipseBtn);
    group->addButton(polygonBtn);
    ellipseBtn->setChecked(true);
    typeRow->addWidget(ellipseBtn);
    typeRow->addWidget(polygonBtn);
    typeRow->addStretch(1);
    layout->addLayout(typeRow);

    // the directions: how to draw the chosen type
    caption = new QLabel(this);
    caption->setWordWrap(true);
    caption->setStyleSheet(QString("color: %1;").arg(G::disabledColor.name()));
    layout->addWidget(caption);

    // what is still missing before Done can be clicked
    status = new QLabel(this);
    status->setWordWrap(true);
    layout->addWidget(status);

    auto *btnRow = new QHBoxLayout;
    removeCornerBtn = new QPushButton(tr("Remove Last Corner"), this);
    removeCornerBtn->setToolTip(tr("Take away the last corner you clicked "
                                   "(Delete or Backspace on the map)"));
    removeCornerBtn->setAutoDefault(false);
    btnRow->addWidget(removeCornerBtn);
    btnRow->addStretch(1);
    cancelBtn = new QPushButton(tr("Cancel"), this);
    cancelBtn->setAutoDefault(false);
    doneBtn = new QPushButton(tr("Done"), this);
    doneBtn->setDefault(true);
    btnRow->addWidget(cancelBtn);
    btnRow->addWidget(doneBtn);
    layout->addLayout(btnRow);

    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    connect(doneBtn, &QPushButton::clicked, this, [this] {
        if (doneBtn->isEnabled()) accept();
    });
    connect(removeCornerBtn, &QPushButton::clicked,
            this, &PlaceDlg::removeCornerRequested);
    connect(nameEdit, &QLineEdit::textChanged, this, &PlaceDlg::updateCaption);
    connect(ellipseBtn, &QRadioButton::toggled, this, [this](bool on) {
        updateCaption();
        emit shapeChosen(on ? Geo::Place::Ellipse : Geo::Place::Polygon);
    });

    updateCaption();
}

void PlaceDlg::start(qint64 placeId, const QString &placeName, Geo::Place::Shape s)
{
    id = placeId;
    complete = false;
    corners = 0;
    setWindowTitle(placeId ? tr("Edit Place") : tr("New Place"));
    {
        QSignalBlocker b1(ellipseBtn), b2(polygonBtn);
        ellipseBtn->setChecked(s == Geo::Place::Ellipse);
        polygonBtn->setChecked(s == Geo::Place::Polygon);
    }
    nameEdit->setText(placeName);
    nameEdit->setFocus();
    nameEdit->selectAll();
    updateCaption();
}

QString PlaceDlg::name() const
{
    return nameEdit->text().simplified();
}

Geo::Place::Shape PlaceDlg::shape() const
{
    return polygonBtn->isChecked() ? Geo::Place::Polygon : Geo::Place::Ellipse;
}

void PlaceDlg::setShapeState(bool isComplete, int cornerCount)
{
    complete = isComplete;
    corners = cornerCount;
    updateCaption();
}

void PlaceDlg::updateCaption()
{
    const bool poly = shape() == Geo::Place::Polygon;
    caption->setText(poly
        ? tr("Click the map to add corners. Double-click, or click the first (green) "
             "corner, to close the shape. Delete or Backspace removes the last corner. "
             "Drag the map to pan and scroll to zoom. Once the shape is closed, drag a "
             "corner to move it.")
        : tr("Drag the centre to move the ellipse, the blue handles to size it (hold "
             "Shift to keep its shape) and the green knob to turn it. Drag the map to "
             "pan and scroll to zoom.\n\nAn image is in the place when its GPS location "
             "is inside the shape."));
    removeCornerBtn->setVisible(poly);
    removeCornerBtn->setEnabled(poly && corners > 0);

    QStringList missing;
    if (name().isEmpty()) missing << tr("give the place a name");
    if (!complete) {
        if (!poly) missing << tr("place the ellipse on the map");
        else if (corners < 3)
            missing << tr("click at least %1 more corner(s) on the map, then close the "
                          "shape").arg(3 - corners);
        else missing << tr("close the shape (double-click, or click the first corner)");
    }
    doneBtn->setEnabled(missing.isEmpty());
    doneBtn->setToolTip(missing.isEmpty() ? tr("Save the place")
                                          : tr("To finish: %1.").arg(missing.join("; ")));
    status->setText(missing.isEmpty() ? tr("Ready: click Done to save the place.")
                                      : tr("To finish: %1.").arg(missing.join("; ")));
}
