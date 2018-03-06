#ifndef INFOSTRING_H
#define INFOSTRING_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "Dialogs/tokendlg.h"

class InfoString : public QObject
{
    Q_OBJECT
public:
    explicit InfoString(QObject *parent,
                        Metadata *metadata,
                        DataModel *dm,
                        QMap<QString, QString> &infoTemplates,
                        int &currentInfoTemplate);
    void editTemplates();
signals:

public slots:

private:
    Metadata *m;
    DataModel *dm;
    QMap<QString,QString> tokenMap;

    QMap<QString, QString> &infoTemplates;
    int &currentInfoTemplate;

    void initTokenMap();
    bool parseToken(QString &tokenString, int pos, QString &token, int &tokenEnd);
    QString parseTokenString(QString &tokenString, QString &fPath, QModelIndex &idx);
    QString tokenValue(QString &tokenString, QFileInfo &info, QString &fPath, QModelIndex &idx);
};

#endif // INFOSTRING_H
