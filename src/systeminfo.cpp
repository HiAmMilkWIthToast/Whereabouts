#include "systeminfo.h"

#include <QTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <lmcons.h>   // UNLEN
#endif

namespace SystemInfo {

QString userName()
{
#ifdef Q_OS_WIN
    // GetUserNameW rather than GetUserNameExW(NameDisplay). The display name is
    // nicer when an account sets one, but on an account that has not, Windows
    // answers with a fragment of the full name instead of failing: this machine
    // returned "c j" for the user whose profile is C:\Users\Kille. The login
    // name is the one that matches the profile folder and is never truncated.
    wchar_t buffer[UNLEN + 1];
    DWORD length = UNLEN + 1;
    if (GetUserNameW(buffer, &length) && length > 1)
        return QString::fromWCharArray(buffer, int(length) - 1);  // length counts the null
    return QString();
#else
    // Windows is the only platform Cairn ships for, so there is no other way to
    // ask yet. Returning empty keeps the greeting optional rather than wrong.
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

} // namespace SystemInfo
