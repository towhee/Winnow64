#ifndef ISSUE_H
#define ISSUE_H

#include <QObject>
#include <QtWidgets>
// #include <QSharedPointer>

class Issue : public QObject
{
    Q_OBJECT
public:
    Issue();
    ~Issue() override;

    // Ordered low-severity → high-severity. G::issueThreshold filters at
    // or below its value (e.g. threshold=Info hides Debug+Comment).
    // Undefined is reserved for unknown type strings — never filtered.
    enum Type {
        Debug,
        Comment,
        Info,
        General,
        Warning,
        Error,
        Undefined
    };
    static const QStringList &typeDescList() {
        static const QStringList list{"Debug", "Comment", "Info", "General", "Warning", "Error", "Undefined"};
        return list;
    }
    QStringList TypeDesc = typeDescList();

    Type type;
    // Category cat;
    QString src;
    QString msg;
    int sfRow;
    QString fPath;
    QString timeStamp;

    // One-line column widths (see ONE-LINE LAYOUT in issue.cpp)
    static constexpr int TimeWidth = 20;
    static constexpr int TypeWidth = 9;
    static constexpr int SrcWidth = 34;

    QString toString(bool isOneLine = true, int newLineOffset = 0);
    // The type + source + message of a one-line issue, for collapsing repeats
    static QString collapseKey(const QString &line);
    // Strip what a WinnowReport.txt should not carry (see REPORT PRIVACY in issue.cpp)
    static QString redactReport(QString text, const QString &home, bool hideFolders);

};

Q_DECLARE_METATYPE(QSharedPointer<Issue>)

#endif // ISSUE_H
