#ifndef INFOSTRING_H
#define INFOSTRING_H

#include <QtWidgets>
#include "Metadata/metadata.h"
#include "Datamodel/datamodel.h"
#include <functional>
#include "Dialogs/loupeinfodlg.h"
#include "Embellish/Properties/embelproperties.h"

class InfoString : public QWidget
{
    Q_OBJECT
public:
    explicit InfoString(QWidget *parent, DataModel *dm, QSettings *setting);
    void change(std::function<void()> updateInfoCallback = nullptr);
    QString getCurrentInfoTemplate();
    QString parseTokenString(QString &tokenString, QString &fPath, QModelIndex &idx);
    QString parseTokenString(QString &tokenString, QString &fPath);
    void add(QMap<QString,QVariant> items);

    QStringList tokens;
    QMap<QString, QString> infoTemplates;
    QString loupeInfoTemplate;

private:
    ImageMetadata m;
    DataModel *dm;
    QSettings *setting;
    QMap<QString,QString> exampleMap;
    QMap<QString,QString> item;
    QMap<QString,QString> usingTokenMap;

    void initTokenList();
    void initExampleMap();
    bool parseToken(QString &tokenString, int pos, QString &token, int &tokenEnd);
    QString tokenValue(QString &tokenString, QFileInfo &info, QString &fPath, QModelIndex &idx);
    QString tokenValue(QString &tokenString, QFileInfo &info, QString &fPath, ImageMetadata &m);
    int getCurrentInfoTemplateIndex();
    QString getCurrentInfoTemplate(int index);
    QString uniqueTokenName(QString name);
    void usingToken();
};

#endif // INFOSTRING_H
