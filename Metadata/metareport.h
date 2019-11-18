#ifndef METAREPORT_H
#define METAREPORT_H

#include <QtWidgets>

class MetaReport
{
public:
    static void header(QString title, QTextStream &rpt);
};

#endif // METAREPORT_H
