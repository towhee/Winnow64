#include "issue.h"

Issue::Issue() {}
Issue::~Issue() {}

QString Issue::toString(bool isOneLine, int newLineOffset)
{
    QString d = timeStamp;                          // time stamp "yyyy-MM-dd hh:mm:ss "
    QString t = TypeDesc.at(type);                  // issue type
    QString m = msg;                                // issue message
    QString s = src;                                // issue source function
    QString r = QString::number(sfRow);             // datamodel proxy row (sfRow)
    QString p = fPath;                              // path
    QString l = "\n";                               // newline separator
    QString o = " ";                                // offset datetime string width
    o = o.repeated(newLineOffset);

    if (isOneLine) {
        d = timeStamp.leftJustified(20);
        t = (t + ": ").leftJustified(9);
        m = m.leftJustified(60);
        s = ("Src: " + s).leftJustified(30);
        r = ("Row: " + r + " ").rightJustified(12);
        p = "  File: " + fPath;
        return d + t + m + s + r + p;
    }
    else {
        d = timeStamp;
        t = (t + ": ").leftJustified(9);
        m = m + "   ";
        s = "Src:     " + s  + " ";
        r = "Row:     " + r + " ";
        p = "File:    " + fPath;
        return d + t + m + l + o + s + l + o + r + l + o + p;
    }
}
