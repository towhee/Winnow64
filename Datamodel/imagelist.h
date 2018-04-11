#ifndef IMAGELIST_H
#define IMAGELIST_H

#include <QObject>
#include <QWidget>

class ImageList : public QWidget
{
    Q_OBJECT
public:
    explicit ImageList(QWidget *parent = nullptr);
    bool load(QString &dir, bool includeSubfolders);

protected:
    void keyPressEvent(QKeyEvent *event);

signals:

public slots:
};

#endif // IMAGELIST_H
