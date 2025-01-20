#ifndef WINNATIVEEVENTFILTER_H
#define WINNATIVEEVENTFILTER_H

#include <QObject>
#include <QCoreApplication>
#include <QAbstractNativeEventFilter>
#include <Windows.h>
#include <Dbt.h>

class WinNativeEventFilter : public QObject, public QAbstractNativeEventFilter
{
    Q_OBJECT
public:
    explicit WinNativeEventFilter(QObject *parent = nullptr);
    bool nativeEventFilter(const QByteArray &eventType, void *message, qintptr*) override;

signals:
    void refreshFileSystem();

};

#endif // WINNATIVEEVENTFILTER_H
