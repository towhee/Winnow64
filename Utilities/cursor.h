#ifndef CURSOR_H
#define CURSOR_H

#include <QObject>
#include "Main/global.h"
#include <QtWidgets>

class Cursor
{
    Q_OBJECT
public:
    Cursor();
    void hideCursor();
    void showCursor();
};

#endif // CURSOR_H
