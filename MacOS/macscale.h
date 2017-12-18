#ifndef MACSCALE_H
#define MACSCALE_H

#include "MacOS/macScale.h"
#include <QtCore/qglobal.h>
#include <QSize>

//#define Q_MACEXTRAS_EXPORT Q_DECL_EXPORT


//Q_FORWARD_DECLARE_OBJC_CLASS(macBackingScaleFactor);

namespace QtMac {

QSizeF macBackingScaleFactor();
//float macBackingScaleFactor();
//Q_MACEXTRAS_EXPORT float macBackingScaleFactor();

}  // namespace QtMac

#endif // MACSCALE_H
