#ifndef HISTORYENTRY_H
#define HISTORYENTRY_H

#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QVector>
#include "Develop/editstack.h"

/*
    One step of an image's Develop history: a labelled EditStack snapshot. Split out of
    develophistory.h so the persistent store (historystore.cpp) and the tests can use it
    without the DevelopHistory QObject.
*/
struct HistoryEntry {
    QString   scope;      // display prefix: "Global", "Subject Mask", ...
    QString   action;     // "Exposure", "Add Subject Mask", "Crop", ...
    QString   value;      // "+0.35", "-18"; empty when the action has no value
    QString   mergeKey;   // gesture identity for coalescing; empty = never merge
    EditStack stack;      // full snapshot AFTER the action
    quint64   syncId = 0; // the multi-image edit this step is part of; 0 = this image

    /* JSON form for the persistent store. syncId is written as a STRING: a JSON number
       is a double, which cannot hold every 64-bit id. */
    QJsonObject toJson() const {
        QJsonObject o;
        if (!scope.isEmpty())    o["scope"] = scope;
        o["action"] = action;
        if (!value.isEmpty())    o["value"] = value;
        if (!mergeKey.isEmpty()) o["mergeKey"] = mergeKey;
        if (syncId)              o["syncId"] = QString::number(syncId);
        o["stack"] = stack.toJson();
        return o;
    }
    static HistoryEntry fromJson(const QJsonObject &o) {
        HistoryEntry e;
        e.scope    = o.value("scope").toString();
        e.action   = o.value("action").toString();
        e.value    = o.value("value").toString();
        e.mergeKey = o.value("mergeKey").toString();
        e.syncId   = o.value("syncId").toString().toULongLong();
        e.stack    = EditStack::fromJson(o.value("stack").toObject());
        return e;
    }

    /* The identity of a recipe as the sidecar stores it: "" for an identity stack (the
       sidecar attribute is cleared), else its base64 JSON -- the same rule as
       DevelopProperties::developBlobFor. */
    static QString recipeKey(const EditStack &s) {
        return s.isIdentity() ? QString() : s.toBase64();
    }
    /* A short stamp of recipeKey(s). A saved history carries the stamp of the recipe
       written in the SAME flush, and is trusted only while the sidecar still holds that
       recipe (DevelopHistory::seed). Stamping the recipe rather than comparing the
       current step's snapshot matters: some changes reach the recipe without recording
       a step, and they must not make a perfectly good history look stale. */
    static QString recipeStamp(const EditStack &s) {
        return QString::fromLatin1(QCryptographicHash::hash(recipeKey(s).toUtf8(),
                                   QCryptographicHash::Sha1).toHex());
    }
};

#endif // HISTORYENTRY_H
