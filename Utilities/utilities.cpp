#include "utilities.h"

Utilities::Utilities(QObject *parent)
{

}

void Utilities::setOpacity(QWidget *widget, qreal opacity)
{
    QGraphicsOpacityEffect * effect = new QGraphicsOpacityEffect(widget);
    effect->setOpacity(opacity);
    widget->setGraphicsEffect(effect);
}

QString Utilities::formatMemory(qulonglong bytes, int precision)
{
    qulonglong x = 1024;
    if (bytes == 0) return "0";
    if (bytes < x) return QString::number(bytes) + " bytes";
    if (bytes < x * 1024) return QString::number((float)bytes / x, 'f', precision) + " KB";
    x *= 1024;
    if (bytes < (x * 1024)) return QString::number((float)bytes / x, 'f', precision) + " MB";
    x *= 1024;
    if (bytes < (x * 1024)) return QString::number((float)bytes / x, 'f', precision) + " GB";
    x *= 1024;
    if (bytes < (x * 1024)) return QString::number((float)bytes / x, 'f', precision) + " TB";
    return "More than TB";
}

QString Utilities::enquote(QString &s)
{
    return QChar('\"') + s + QChar('\"');
}
