#pragma once

#include "icons.h"
#include "sessionmodel.h"

#include <QColor>

namespace EntryIcon {

inline Icons::Id kindIcon(Kind kind)
{
    switch (kind) {
    case Kind::Folder:     return Icons::Id::Folder;
    case Kind::Archive:    return Icons::Id::Archive;
    case Kind::Executable: return Icons::Id::Executable;
    case Kind::Image:      return Icons::Id::Image;
    case Kind::Code:       return Icons::Id::Code;
    case Kind::Document:   return Icons::Id::Text;
    case Kind::Shortcut:   return Icons::Id::Link;
    case Kind::Audio:
    case Kind::Video:      return Icons::Id::File;
    case Kind::Other:      return Icons::Id::File;
    }
    return Icons::Id::File;
}

inline QColor kindColor(Kind kind)
{
    return QColor(::kindColor(kind));
}

}
