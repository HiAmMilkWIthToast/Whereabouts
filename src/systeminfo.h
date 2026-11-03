
#pragma once

#include <QString>
#include <QTime>

namespace SystemInfo {

QString userName();

QString greetingFor(const QTime &now);

}
