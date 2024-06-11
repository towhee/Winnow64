#include "issue.h"

Issue::Issue() {}
Issue::~Issue() {}

QString Issue::toString(Format format)
{
    QString strType;
    switch (type) {
    case Type::Error: strType = "Error"; break;
    case Type::Warning: strType = "Warning"; break;
    }

    QString d = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss ");
    QString m = strType + ": " + msg + "   ";                // error message
    QString s = "Src: " + src  + " ";                       // error source function
    QString r = "Row: " + QString::number(sfRow) + " ";   // datamodel proxy row (sfRow)
    QString p = fPath;                                              // path
    QString l = "\n";                                       // newline separator
    QString o = " ";                                        // offset datetime string width
    o = o.repeated(d.count());

    switch (format) {
    case Format::OneRow:
        return d + m + s + r + p;
    case Format::TwoRow:

        break;
    case Format::ThreeRow:

        break;
    case Format::FourRow:

        break;
    case Format::FiveRow:

        break;
    }
}
