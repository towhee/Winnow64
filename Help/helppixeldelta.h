#ifndef HELPPIXELDELTA_H
#define HELPPIXELDELTA_H

#include <QDialog>

namespace Ui {
class HelpPixelDelta;
}

class HelpPixelDelta : public QDialog
{
    Q_OBJECT

public:
    explicit HelpPixelDelta(QWidget *parent = nullptr);
    ~HelpPixelDelta();

private:
    Ui::HelpPixelDelta *ui;
};

#endif // HELPPIXELDELTA_H
