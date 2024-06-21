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
    if (sfRow < 0) r = "n/a";

    if (isOneLine) {
        d = timeStamp.leftJustified(20);
        t = (t + ": ").leftJustified(9);
        m = m.leftJustified(60);
        s = ("Src: " + s).leftJustified(40);
        r = "Row: " + r.rightJustified(8) + " ";
        p = "  Obj: " + fPath;
        if (sfRow < 0 && fPath == "")
            return d + t + m + s;
        else
            return d + t + m + s + r + p;
    }
    else {
        d = timeStamp;
        t = (t + ": ").leftJustified(9);
        m = m + "   ";
        s = "Src:     " + s  + " ";
        r = "Row:     " + r + " ";
        p = "Obj:    " + fPath;
        return d + t + m + l + o + s + l + o + r + l + o + p;
    }
}
