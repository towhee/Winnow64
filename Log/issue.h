#ifndef ISSUE_H
#define ISSUE_H

#include <QObject>
#include <QtWidgets>

class Issue
{
    Q_OBJECT
public:
    Issue();

    enum IssueType {
        Warning,
        Error
    };
    QStringList issueTypeDesc {"Warning", "Error"};

    enum IssueCategory {
        General,
        DM
    };
    QStringList issueCategoryDesc {"General", "DataModel"};

    enum Format {
        OneRow,
        TwoRow,
        ThreeRow,
        FourRow,
        FiveRow
    };

    IssueType type;
    IssueCategory category;
    QString functionName;
    int sfRow;
    QString fPath;
    QString msg;

    QString toString(Format format);

};

#endif // ISSUE_H
