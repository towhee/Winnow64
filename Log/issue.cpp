#include "issue.h"

Issue::Issue() {}
Issue::~Issue() {}

/* ---------------------------------------------------------------------------
   ONE-LINE LAYOUT

   Fixed columns first, then the variable parts, so a long message can never push the
   next column out of line:

       col 0   timestamp   "yyyy-MM-dd hh:mm:ss "                    (TimeWidth)
       col 20  type        "Warning: "                               (TypeWidth)
       col 29  source      "Catalog::commit" padded                  (SrcWidth)
       col 63  message     any length, newlines folded to " | "
               then "  Row: n" when the issue is tied to a datamodel row
               then "  Obj: path" when it names a file

   A source longer than SrcWidth - 2 keeps two spaces after it, so only that line
   shifts. collapseKey() below is the part of a line that identifies the issue (type,
   source, message) without the row or path, which change from one repeat to the next.
   MW::prepareReport collapses repeats on it: change the two together.
   --------------------------------------------------------------------------- */

static const QString rowTag = "  Row: ";
static const QString objTag = "  Obj: ";

QString Issue::toString(bool isOneLine, int newLineOffset)
{
    /* Undefined: G::issue got a type string it did not know and folded it into the
       message ("prefix: detail"), so show it as "Other" with the full message. */
    QString t = (type == Type::Undefined) ? QString("Other") : TypeDesc.at(type);
    QString m = msg;
    QString r = sfRow < 0 ? QString("n/a") : QString::number(sfRow);

    if (isOneLine) {
        m.replace("\r\n", " | ").replace('\n', " | ").replace('\r', " | ");
        QString s = src.leftJustified(SrcWidth - 2) + "  ";
        QString line = timeStamp.leftJustified(TimeWidth)
                       + (t + ": ").leftJustified(TypeWidth)
                       + s
                       + m;
        if (sfRow >= 0) line += rowTag + r;
        if (!fPath.isEmpty()) line += objTag + fPath;
        return line;
    }

    QString l = "\n";                               // newline separator
    QString o = QString(" ").repeated(newLineOffset);
    return timeStamp + (t + ": ").leftJustified(TypeWidth) + m + l
           + o + "Src:     " + src + l
           + o + "Row:     " + r + l
           + o + "Obj:    " + fPath;
}

QString Issue::collapseKey(const QString &line)
{
    /* The type, source and message of a one-line issue (see ONE-LINE LAYOUT): from the
       type column to the first "  Row: " or "  Obj: " after the message starts.

       Lines written before 3_beta7 put the message at col 29, padded to 60, and the
       source after it as "Src: ". They still turn up in reports for a few sessions
       after an update, so they keep their old key: the fixed columns 20 to 129. */
    const int msgCol = TimeWidth + TypeWidth + SrcWidth;
    const qsizetype oldSrcAt = line.indexOf("Src: ", TimeWidth);
    if (oldSrcAt >= 0 && oldSrcAt < 140) return line.mid(TimeWidth, 109);
    qsizetype end = line.size();
    qsizetype rowAt = line.indexOf(rowTag, msgCol);
    qsizetype objAt = line.indexOf(objTag, msgCol);
    if (rowAt >= 0) end = rowAt;
    if (objAt >= 0 && objAt < end) end = objAt;
    return line.mid(TimeWidth, end - TimeWidth);
}

/* ---------------------------------------------------------------------------
   REPORT PRIVACY

   WinnowReport.txt goes to the developer by email or onto a PUBLIC GitHub issue, so
   MW::prepareReport passes the whole text through redactReport() before writing it:

   Always
       - The home folder becomes "~", in either separator and any letter case (Windows
         paths are case-insensitive), and only as a whole folder name: with home
         /Users/al, "/Users/alice" is left alone.
       - Device identifiers in a crash report are replaced with "hidden": macOS .ips
         crashReporterKey, deviceIdentifierForVendor, bootSessionUUID, sleepWakeUUID,
         and Windows Report.wer MachineID, ComputerName and UserName. None of them
         helps find a bug, and crashReporterKey and deviceIdentifierForVendor name one
         machine for good.

   hideFolders (the dialog's "Hide folder names", ticked by default)
       - Every path keeps its start and its file name and loses the folders between:
             ~/Photos/2021/Lake/0866.HEIC        ~/…/0866.HEIC
             /Volumes/Card/DCIM/100/A.ARW        /Volumes/…/A.ARW
             C:\Users\al\Pictures\A.JPG          C:\…\A.JPG
             \\nas\photos\2021\A.JPG             \\…\A.JPG
       - A path starts at "~/", a drive letter, a UNC "\\" or "//", or a root folder
         that holds user data (/Users, /home, /Volumes, /media, /mnt, /run, /srv,
         /private, /tmp, /var, /Network). Folder names may contain spaces, so a folder
         runs to the next separator, but never across two spaces (the issue log's
         column gap), a quote, or the start of another path, so two paths on one line
         stay two paths. A URL ("https://...") is not a path.

   File names are kept: the file type is usually what matters, and the dialog and the
   help page say so. Metadata (GPS, creator, copyright, keywords, serial numbers) is
   never written to the logs, so there is nothing of it here to strip.
   --------------------------------------------------------------------------- */

QString Issue::redactReport(QString r, const QString &home, bool hideFolders)
{
    // Home folder -> "~"
    const QString h = QDir::fromNativeSeparators(home);
    if (h.size() > 1) {
        QStringList parts = h.split('/');
        for (QString &p : parts) p = QRegularExpression::escape(p);
        const QRegularExpression homeRe(parts.join(R"([/\\])") + R"((?![\w.-]))",
                                        QRegularExpression::CaseInsensitiveOption |
                                        QRegularExpression::UseUnicodePropertiesOption);
        r.replace(homeRe, "~");
    }

    // Device identifiers (crash reports)
    static const QRegularExpression idRe(
        R"re(("?(?:crashReporterKey|deviceIdentifierForVendor|bootSessionUUID|)re"
        R"re(sleepWakeUUID|MachineID|ComputerName|UserName)"?\s*[:=]\s*"?)[^",\s}]+)re");
    r.replace(idRe, "\\1hidden");

    if (!hideFolders) return r;

    // Folders -> "…"
    static const QString seg =
        R"((?:(?!  | ~[/\\]| [A-Za-z]:[/\\]| [/\\])[^\n/\\"'])+)";
    static const QRegularExpression folderRe(
        R"((~[/\\]|(?<![A-Za-z])[A-Za-z]:[/\\]|\\\\|(?<![:\w/])//|)"
        R"(/(?:Users|home|Volumes|media|mnt|run|srv|private|tmp|var|Network)[/\\]))"
        "((?:" + seg + R"([/\\])+))",
        QRegularExpression::UseUnicodePropertiesOption);
    const QString ellipsis(QChar(0x2026));
    QString out;
    out.reserve(r.size());
    qsizetype from = 0;
    QRegularExpressionMatchIterator it = folderRe.globalMatch(r);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        out += QStringView(r).mid(from, m.capturedStart() - from);
        out += m.captured(1) + ellipsis + m.captured(2).back();
        from = m.capturedEnd();
    }
    out += QStringView(r).mid(from);
    return out;
}
