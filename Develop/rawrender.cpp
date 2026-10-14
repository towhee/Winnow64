#include "Develop/rawrender.h"
#include "Develop/editparams.h"
#include "Develop/editstack.h"
#include "Main/global.h"
#ifdef Q_OS_MAC
#include "ImageFormats/Raw/applerawdecode.h"
#endif

#include <QByteArray>
#include <QCoreApplication>

namespace RawRender {

QString Choice::label() const
{
    if (!apple) return QCoreApplication::translate("RawRender", "Winnow");
    if (applePin <= 0) return QCoreApplication::translate("RawRender", "Apple Raw (newest)");
    return QCoreApplication::translate("RawRender", "Apple Raw %1").arg(applePin);
}

Choice defaultChoice()
{
    Choice c;
#ifdef Q_OS_MAC
    c.apple = G::decodeRawEngine == G::DecodeRawEngine::appleDecodeRawEngine;
    c.applePin = c.apple ? AppleRawDecode::VersionPin() : 0;
#endif
    return c;
}

Choice resolve(const EditParams &global)
{
    if (global.rawEngine < 0) return defaultChoice();
    Choice c;
#ifdef Q_OS_MAC
    c.apple = global.rawEngine == 1;
    c.applePin = c.apple ? qMax(0, global.appleRawVersion) : 0;
#endif
    return c;
}

Choice fromBlob(const QString &blob)
{
    /*  Only a recipe that CHOSE carries "rawEngine" (paramsToJson writes it when set), so
        a substring probe of the decoded JSON spares parsing every recipe -- this runs per
        edited image as sidecars are read (Metadata::devPreviewKey). */
    if (blob.isEmpty()) return defaultChoice();
    const QByteArray json = QByteArray::fromBase64(blob.toLatin1());
    if (!json.contains("\"rawEngine\"")) return defaultChoice();
    const EditStack s = EditStack::fromBase64(blob);
    if (s.scopes.isEmpty()) return defaultChoice();
    return resolve(s.scopes.first().params);
}

void apply(EditParams &global, const Choice &c)
{
    global.rawEngine = c.apple ? 1 : 0;
    global.appleRawVersion = c.apple ? qMax(0, c.applePin) : 0;
}

int engineKey(const Choice &c)
{
    return int(c.apple ? G::DecodeRawEngine::appleDecodeRawEngine
                       : G::DecodeRawEngine::winnowDecodeRawEngine);
}

QString appleKeyPart(const Choice &c)
{
#ifdef Q_OS_MAC
    if (c.apple)
        return QString("|appleRaw=c3:%1:%2").arg(AppleRawDecode::EffectiveVersionPin(c.applePin))
                                            .arg(AppleRawDecode::SupportFingerprint());
#else
    Q_UNUSED(c)
#endif
    return QString();
}

} // namespace RawRender
