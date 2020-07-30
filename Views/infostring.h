#ifndef INFOSTRING_H
#define INFOSTRING_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include "Dialogs/tokendlg.h"

class InfoString : public QWidget
{
    Q_OBJECT
public:
    explicit InfoString(QWidget *parent, DataModel *dm);
    void editTemplates();
//    void setCurrentInfoTemplate(QString &currentInfoTemplate);
    QString getCurrentInfoTemplate();
    QString parseTokenString(QString &tokenString, QString &fPath, QModelIndex &idx);

    QStringList tokens;
    QMap<QString, QString> infoTemplates;
    QString currentInfoTemplate;

protected:

private:
    ImageMetadata m;
    DataModel *dm;
    QMap<QString,QString> exampleMap;

    void initTokenList();
    void initExampleMap();
    bool parseToken(QString &tokenString, int pos, QString &token, int &tokenEnd);
    QString tokenValue(QString &tokenString, QFileInfo &info, QString &fPath, QModelIndex &idx);
    int getIndex();
    QString getCurrentInfoTemplate(int index);
};

#endif // INFOSTRING_H
