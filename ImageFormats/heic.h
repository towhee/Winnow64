#ifndef HEIC_H
#define HEIC_H

#include <QtWidgets>
#include <QtCore>
#include <QtXmlPatterns>
#include <iostream>
#include <iomanip>

#include "Main/global.h"
#include "Utilities/utilities.h"

class Heic
{
public:
    Heic();
//    static bool getHeic(QFile &file)

private:
    bool nextHeifBox(quint32 &length, QString &type);
    bool getHeifBox(QString &type, quint32 &offset, quint32 &length);
    bool ftypBox(quint32 &offset, quint32 &length);
    bool metaBox(quint32 &offset, quint32 &length);
    bool hdlrBox(quint32 &offset, quint32 &length);
    bool pitmBox(quint32 &offset, quint32 &length);
    bool ilocBox(quint32 &offset, quint32 &length);  //
    bool iinfBox(quint32 &offset, quint32 &length);  // Item Information Box
    bool infeBox(quint32 &offset, quint32 &length);  // Item Info Entry
    bool irefBox(quint32 &offset, quint32 &length);
    bool sitrBox(quint32 &offset, quint32 &length);  // Single Item Type Reference Box
    bool sitrBoxL(quint32 &offset, quint32 &length); // Single Item Type Reference Box Large
    bool iprpBox(quint32 &offset, quint32 &length);
    bool hvcCBox(quint32 &offset, quint32 &length);
    bool ispeBox(quint32 &offset, quint32 &length);
    bool ipmaBox(quint32 &offset, quint32 &length);
    bool mdatBox(quint32 &offset, quint32 &length);
};

#endif // HEIC_H
