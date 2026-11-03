#pragma once

#include "sessionmodel.h"

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace SessionStore {

Session capture(const QString &rootPath);

QString directory();

bool save(const Session &session, QString *error = nullptr);
Session load(const QString &path, bool *ok = nullptr);

QString pathForId(const QString &id);

QList<Session> listAll(const QStringList &paths = {});

QString suggestedName(const QString &rootPath);

QString makeId(const QString &rootPath);

bool isDesktopPath(const QString &path);

QString desktopPath();

}
