#include "systeminfo.h"

#include <QTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <lmcons.h>
#endif

namespace SystemInfo {

QString userName()
{
#ifdef Q_OS_WIN

    wchar_t buffer[UNLEN + 1];
    DWORD length = UNLEN + 1;
    if (GetUserNameW(buffer, &length) && length > 1)
        return QString::fromWCharArray(buffer, int(length) - 1);
    return QString();
#else

    return QString();
#endif
}

QString greetingFor(const QTime &now)
{
    if (!now.isValid())
        return QStringLiteral("Hello");
    const int hour = now.hour();
    if (hour < 12)
        return QStringLiteral("Good morning");
    if (hour < 18)
        return QStringLiteral("Good afternoon");
    return QStringLiteral("Good evening");
}

}
