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
        Warning,
        Error
    };
    QStringList TypeDesc {"Warning", "Error"};

    enum Category {
        General,
        DM
    };
    QStringList CategoryDesc {"General", "DataModel"};

    enum Format {
        OneRow,
        TwoRow,
        ThreeRow,
        FourRow,
        FiveRow
    };

    Type type;
    Category cat;
    QString src;
    QString msg;
    int sfRow;
    QString fPath;

    QString toString(Format format);

};

Q_DECLARE_METATYPE(QSharedPointer<Issue>)

#endif // ISSUE_H
