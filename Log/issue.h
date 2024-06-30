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

    enum Type {
        Undefined,
        Warning,
        Error,
        Comment,
        General
    };
    QStringList TypeDesc {"Undefined", "Warning", "Error", "Comment", "General"};

    Type type;
    // Category cat;
    QString src;
    QString msg;
    int sfRow;
    QString fPath;
    QString timeStamp;

    QString toString(bool isOneLine = true, int newLineOffset = 0);

};

Q_DECLARE_METATYPE(QSharedPointer<Issue>)

#endif // ISSUE_H
