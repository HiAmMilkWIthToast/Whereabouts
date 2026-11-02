// A warehouse for the handful of things Cairn asks the machine about itself:
// who is using it and what the clock says. Kept in one place so the window does
// not each reach for the OS in its own way.
#pragma once

#include <QString>
#include <QTime>

namespace SystemInfo {

// The name to greet the user by. Windows keeps this separate from the account's
// login name, so it can be "Kille" where the login is "kille" or a domain
// path. Empty when it cannot be determined, which the callers treat as "no
// greeting" rather than inventing one.
QString userName();

// A greeting for the given clock time: "Good morning" before noon, "Good
// afternoon" until 18:00, "Good evening" after that.
QString greetingFor(const QTime &now);

} // namespace SystemInfo
