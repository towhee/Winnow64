#include "winnativeeventfilter.h"

WinNativeEventFilter::WinNativeEventFilter(QObject *parent)
    : QObject{parent}
{
    // install
    //QCoreApplication::instance()->eventDispatche()->installNativeEventFilter(this);
    QCoreApplication::instance()->installNativeEventFilter(this);
}

bool WinNativeEventFilter::nativeEventFilter(const QByteArray &eventType,
                                             void *message, qintptr*)
{
    MSG *msg = static_cast<MSG *>(message);
    /*
    qDebug() << "WindowsNativeEventFilter::nativeEventFilter"
             << "eventType =" << eventType
             << "message =" << msg->message
             << "wParam =" << msg->wParam
             << "lParam =" << msg->lParam
                ;
                */
    if (msg->message == WM_DEVICECHANGE && msg->wParam == DBT_DEVICEARRIVAL) // 537/32768
    {
        //qDebug() << "Device connected";
        emit refreshFileSystem();
    }
    else if (msg->message == WM_DEVICECHANGE && msg->wParam == DBT_DEVICEREMOVECOMPLETE) // 537/32772
    {
        //qDebug() << "Device disconnected";
        emit refreshFileSystem();
    }

    return false;
}

