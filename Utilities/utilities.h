#ifndef UTILITIES_H
#define UTILITIES_H

#include <QtWidgets>

class Utilities
{
public:
    Utilities();
    static void setOpacity(QWidget *widget, qreal opacity);
    static QString formatMemory(qulonglong bytes, int precision = 1);
    static QString enquote(QString &s);
    static QString centeredRptHdr(QChar padChar, QString title);

public slots:
    static void hideCursor();

};

#endif // UTILITIES_H
