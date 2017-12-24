#ifndef QMACFUNCTIONS_P_H
#define QMACFUNCTIONS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "MacOS/macscale.h"

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
#include <QtCore/QDebug>
#include <QtGui/QGuiApplication>
#include <qpa/qplatformnativeinterface.h>

QT_BEGIN_NAMESPACE

inline QPlatformNativeInterface::NativeResourceForIntegrationFunction resolvePlatformFunction(const QByteArray &functionName)
{
    QPlatformNativeInterface *nativeInterface = QGuiApplication::platformNativeInterface();
    QPlatformNativeInterface::NativeResourceForIntegrationFunction function =
        nativeInterface->nativeResourceFunctionForIntegration(functionName);
    if (!function)
         qWarning() << "Qt could not resolve function" << functionName
                    << "from QGuiApplication::platformNativeInterface()->nativeResourceFunctionForIntegration()";
    return function;
}
#endif

QT_END_NAMESPACE

#endif // QMACFUNCTIONS_P_H

