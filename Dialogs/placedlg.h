#ifndef PLACEDLG_H
#define PLACEDLG_H

#include <QDialog>
#include "Utilities/geo.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

/*
    THE NEW / EDIT PLACE DIALOG (the Places panel's +, and Edit Shape...). NON-MODAL, a
    tool window: the shape is drawn on the map behind it, so the map must keep taking
    clicks while it is open. MW owns one and reuses it (MW::openPlaceDlg).

    It holds the name, the type (ellipse or polygon) and a caption that says how to
    draw the chosen type and what is still missing. Done is disabled until there is a
    name and a finished shape -- the caption says which is missing, rather than a
    popup after the click. The shape itself lives in MapView's edit mode; MW tells the
    dialog how far it has got (setShapeState).
*/
class PlaceDlg : public QDialog
{
    Q_OBJECT
public:
    explicit PlaceDlg(QWidget *parent = nullptr);

    /* A new place (id 0), or an existing one's name and shape. */
    void start(qint64 id, const QString &name, Geo::Place::Shape shape);
    qint64 placeId() const { return id; }
    QString name() const;
    Geo::Place::Shape shape() const;
    /* Where the shape on the map has got to: complete, and for a polygon its corners. */
    void setShapeState(bool complete, int corners);

signals:
    /* The type was changed: MW restarts the shape on the map. */
    void shapeChosen(Geo::Place::Shape shape);
    void removeCornerRequested();

private:
    void updateCaption();

    qint64 id = 0;
    bool complete = false;
    int corners = 0;
    QLineEdit *nameEdit;
    QRadioButton *ellipseBtn;
    QRadioButton *polygonBtn;
    QLabel *caption;
    QLabel *status;
    QPushButton *removeCornerBtn;
    QPushButton *doneBtn;
    QPushButton *cancelBtn;
};

#endif // PLACEDLG_H
