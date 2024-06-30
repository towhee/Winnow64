#include "issue.h"

Issue::Issue() {}
Issue::~Issue() {}

QString Issue::toString(bool isOneLine, int newLineOffset)
{
    QString t_delim = ": ";
    if (type == Type::Undefined) t_delim = "";     // Undefined type = custom msg
    QString s_src = "Src: ";
    if (src == "") s_src = "";

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
    if (type == Type::Undefined) {
        t = m;
        m = "";
    }

    if (isOneLine) {
        d = timeStamp.leftJustified(20);
        t = (t + t_delim).leftJustified(9);
        m = m.leftJustified(60);
        s = (s_src + s).leftJustified(40);
        r = "Row: " + r.rightJustified(5) + " ";
        p = "  Obj: " + fPath;
        if (sfRow < 0 && fPath == "")
            return d + t + m + s;
        else
            return d + t + m + s + r + p;
    }
    else {
        d = timeStamp;
        t = (t + t_delim).leftJustified(9);
        m = m + "   ";
        s = "Src:     " + s  + " ";
        r = "Row:     " + r + " ";
        p = "Obj:    " + fPath;
        return d + t + m + l + o + s + l + o + r + l + o + p;
    }
}
