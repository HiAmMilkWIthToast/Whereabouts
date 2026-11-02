#include "layoutview.h"
#include "entryicon.h"
#include "theme.h"

#include <QFontMetrics>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QSet>
#include <QToolTip>
#include <QWheelEvent>

#include <algorithm>
#include <climits>

namespace {

// Tile metrics at zoom 1. A desktop cell is roughly 75x98 at 100% scaling, and
// the map keeps that proportion so the result reads as the same layout. The
// width is a little wider than a real cell because a desktop icon shows only a
// wrap of its name, while these tiles show one line and need room for it.
constexpr qreal kTileWidth = 106.0;
constexpr qreal kTileHeight = 78.0;
constexpr qreal kTileGap = 8.0;
constexpr qreal kTileRadius = 7.0;
constexpr qreal kIconSize = 20.0;
constexpr qreal kListRowHeight = 30.0;
constexpr qreal kMargin = 18.0;

QString humanSuffix(const Entry &entry)
{
    return entry.suffix.isEmpty() ? QString() : entry.suffix.toUpper();
}

} // namespace

LayoutView::LayoutView(QWidget *parent)
    : QAbstractScrollArea(parent)
{
    setFrameShape(QFrame::NoFrame);
    setObjectName(QStringLiteral("layoutView"));
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    viewport()->setCursor(Qt::ArrowCursor);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    viewport()->setAutoFillBackground(false);
    m_emptyMessage = QStringLiteral("Load a session to see its layout.");
}

void LayoutView::setSession(const Session &session)
{
    m_session = session;
    m_selectedPath.clear();
    m_hoveredPath.clear();
    m_userZoomed = false;
    rebuild();
    fitToView();
    emit viewChanged();
}

void LayoutView::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    rebuild();
    if (m_mode == Mode::Map)
        fitToView();
    else
        viewport()->setCursor(Qt::PointingHandCursor);
    emit viewChanged();
}

void LayoutView::setKindFilter(const QHash<int, bool> &enabledByKind)
{
    m_kindFilter = enabledByKind;
    rebuild();
}

void LayoutView::setSearch(const QString &text)
{
    if (m_search == text)
        return;
    m_search = text;
    rebuild();
}

bool LayoutView::isVisible(const Entry &entry) const
{
    if (!m_kindFilter.isEmpty()
        && m_kindFilter.contains(int(entry.kind))
        && !m_kindFilter.value(int(entry.kind)))
        return false;

    if (!m_search.isEmpty()) {
        const bool nameHit =
            entry.name.contains(m_search, Qt::CaseInsensitive);
        const bool suffixHit =
            entry.suffix.contains(m_search, Qt::CaseInsensitive);
        if (!nameHit && !suffixHit)
            return false;
    }
    return true;
}

void LayoutView::rebuild()
{
    if (m_mode == Mode::Map)
        rebuildMap();
    else
        rebuildList();
}

void LayoutView::rebuildMap()
{
    m_tiles.clear();
    if (m_session.entries.isEmpty()) {
        m_emptyMessage = QStringLiteral("This session captured no items.");
        updateScrollRanges();
        viewport()->update();
        return;
    }

    const QSize cell = m_session.cellSize.isValid() ? m_session.cellSize
                                                    : QSize(75, 98);
    const int cellW = qMax(1, cell.width());
    const int cellH = qMax(1, cell.height());

    // A live capture stores the pixel position but not a grid cell, and the cell
    // is deliberately left out of the file to keep it readable. Deriving it here
    // keeps reloaded sessions identical to freshly captured ones; using the
    // pixel position directly is what the capture step does too.
    const auto effectiveCell = [&](const Entry &entry) -> QPoint {
        if (entry.cell.x() >= 0 && entry.cell.y() >= 0)
            return entry.cell;
        if (entry.pos.x() >= 0 && entry.pos.y() >= 0)
            return QPoint(entry.pos.x() / cellW, entry.pos.y() / cellH);
        return QPoint(0, 0);
    };

    QPoint minCell(INT_MAX, INT_MAX);
    QPoint maxCell(INT_MIN, INT_MIN);
    for (const Entry &entry : m_session.entries) {
        const QPoint cellPos = effectiveCell(entry);
        minCell = QPoint(qMin(minCell.x(), cellPos.x()),
                         qMin(minCell.y(), cellPos.y()));
        maxCell = QPoint(qMax(maxCell.x(), cellPos.x()),
                         qMax(maxCell.y(), cellPos.y()));
    }
    if (minCell.x() == INT_MAX)
        minCell = QPoint(0, 0);
    if (maxCell.x() == INT_MIN)
        maxCell = QPoint(0, 0);

    // A desktop usually has one or two icons dragged far from the rest, and
    // honouring the empty space between them would shrink everything to
    // illegibility. Columns and rows that hold nothing are therefore collapsed,
    // keeping order and adjacency; the exact coordinates are still reported by
    // the inspector for any selected item.
    QSet<int> usedCols;
    QSet<int> usedRows;
    for (const Entry &entry : m_session.entries) {
        const QPoint cellPos = effectiveCell(entry);
        usedCols.insert(cellPos.x() - minCell.x());
        usedRows.insert(cellPos.y() - minCell.y());
    }

    QHash<int, int> colRemap;
    QHash<int, int> rowRemap;
    {
        int packed = 0;
        const int last = usedCols.isEmpty() ? 0
                                            : *std::max_element(usedCols.begin(),
                                                                usedCols.end());
        for (int index = 0; index <= last; ++index) {
            if (usedCols.contains(index))
                colRemap.insert(index, packed++);
        }
    }
    {
        int packed = 0;
        const int last = usedRows.isEmpty() ? 0
                                            : *std::max_element(usedRows.begin(),
                                                                usedRows.end());
        for (int index = 0; index <= last; ++index) {
            if (usedRows.contains(index))
                rowRemap.insert(index, packed++);
        }
    }

    const qreal stepX = kTileWidth + kTileGap;
    const qreal stepY = kTileHeight + kTileGap;

    // Explorer positions are the top-left of the icon, not the cell, so they are
    // folded into the same grid the computed layout uses. This keeps mixed
    // sessions (some live, some computed) coherent.
    //
    // Two icons sitting a few pixels apart on a desktop with grid alignment off
    // round into the same cell. Drawing both there put one tile exactly on top
    // of the other, hiding it completely and making it impossible to select. A
    // cell that is already taken now pushes the later item down a row.
    QSet<qint64> occupied;
    int maxCol = 0;
    int maxRow = 0;
    for (const Entry &entry : m_session.entries) {
        if (!isVisible(entry))
            continue;

        const QPoint cellPos = effectiveCell(entry);
        const int col = colRemap.value(cellPos.x() - minCell.x(), 0);
        int row = rowRemap.value(cellPos.y() - minCell.y(), 0);
        while (occupied.contains(qint64(col) * 100000 + row))
            ++row;
        occupied.insert(qint64(col) * 100000 + row);
        maxCol = qMax(maxCol, col);
        maxRow = qMax(maxRow, row);

        const QPointF topLeft(kMargin + col * stepX, kMargin + row * stepY);

        Tile tile;
        tile.path = entry.relativePath;
        tile.rect = QRectF(topLeft, QSizeF(kTileWidth, kTileHeight));
        tile.name = entry.name;
        tile.kind = entry.kind;
        tile.suffix = entry.suffix;
        tile.size = entry.size;
        tile.viaShell = entry.fromShell;
        tile.target = entry.target;
        tile.children = entry.children;
        m_tiles.append(tile);
    }

    if (m_tiles.isEmpty()) {
        m_emptyMessage = m_search.isEmpty()
            ? QStringLiteral("Every item is filtered out.")
            : QStringLiteral("No items match \"%1\".").arg(m_search);
    }

    // Sized from the cells actually used, after any collision nudging, so a
    // tile can never be laid out beyond the scrollable canvas.
    const qreal width = kMargin * 2 + (maxCol + 1) * stepX - kTileGap;
    const qreal height = kMargin * 2 + (maxRow + 1) * stepY - kTileGap;
    // The canvas is virtual: only the scrollbars know about its extent, so the
    // widget itself can stay as small as the window wants.
    m_canvasSize = QSizeF(qMax(width, stepX), qMax(height, stepY));

    updateScrollRanges();
    viewport()->update();
}

void LayoutView::rebuildList()
{
    m_tiles.clear();
    if (m_session.entries.isEmpty()) {
        m_emptyMessage = QStringLiteral("This session captured no items.");
        updateScrollRanges();
        viewport()->update();
        return;
    }
    viewport()->setCursor(Qt::PointingHandCursor);

    QList<Entry> visible;
    for (const Entry &entry : m_session.entries) {
        if (isVisible(entry))
            visible.append(entry);
    }
    std::stable_sort(visible.begin(), visible.end(),
                     [](const Entry &a, const Entry &b) {
                         if (a.name.compare(b.name, Qt::CaseInsensitive) != 0)
                             return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
                         return a.suffix < b.suffix;
                     });

    if (visible.isEmpty()) {
        m_emptyMessage = m_search.isEmpty()
            ? QStringLiteral("Every item is filtered out.")
            : QStringLiteral("No items match \"%1\".").arg(m_search);
    }

    for (int i = 0; i < visible.size(); ++i) {
        const Entry &entry = visible.at(i);
        Tile tile;
        tile.path = entry.relativePath;
        tile.rect = QRectF(kMargin, kMargin + i * kListRowHeight,
                           qMax(200.0, viewport()->width() - kMargin * 2 - 14),
                           kListRowHeight - 2);
        tile.name = entry.name;
        tile.kind = entry.kind;
        tile.suffix = entry.suffix;
        tile.size = entry.size;
        tile.viaShell = entry.fromShell;
        tile.target = entry.target;
        tile.children = entry.children;
        m_tiles.append(tile);
    }

    m_canvasSize = QSizeF(viewport()->width(),
                          kMargin * 2 + qMax(1, visible.size()) * kListRowHeight);
    updateScrollRanges();
    viewport()->update();
}

void LayoutView::updateScrollRanges()
{
    if (m_mode == Mode::List) {
        const int contentHeight = int(m_canvasSize.height());
        const int visible = viewport()->height();
        verticalScrollBar()->setRange(0, qMax(0, contentHeight - visible));
        verticalScrollBar()->setPageStep(qMax(1, visible));
        verticalScrollBar()->setSingleStep(int(kListRowHeight));
        horizontalScrollBar()->setRange(0, 0);
        m_origin = QPointF(0, -verticalScrollBar()->value());
        return;
    }

    const QSizeF scaled = m_canvasSize * m_zoom;
    const int extraX = qMax(0, int(scaled.width() - viewport()->width()));
    const int extraY = qMax(0, int(scaled.height() - viewport()->height()));

    horizontalScrollBar()->setRange(0, extraX);
    horizontalScrollBar()->setPageStep(qMax(1, viewport()->width()));
    horizontalScrollBar()->setSingleStep(24);
    verticalScrollBar()->setRange(0, extraY);
    verticalScrollBar()->setPageStep(qMax(1, viewport()->height()));
    verticalScrollBar()->setSingleStep(24);

    // Centre the canvas until it is larger than the viewport.
    const qreal offsetX = extraX > 0 ? -horizontalScrollBar()->value()
                                     : (viewport()->width() - scaled.width()) / 2.0;
    const qreal offsetY = extraY > 0 ? -verticalScrollBar()->value()
                                     : (viewport()->height() - scaled.height()) / 2.0;
    m_origin = QPointF(offsetX, offsetY) + QPointF(m_panOffset);
}

void LayoutView::fitToView()
{
    if (m_mode != Mode::Map || m_canvasSize.isEmpty()) {
        viewport()->update();
        return;
    }
    const qreal pad = 10.0;
    const qreal availableW = qMax(60.0, viewport()->width() - pad * 2);
    const qreal availableH = qMax(60.0, viewport()->height() - pad * 2);
    qreal fit = qMin(availableW / m_canvasSize.width(),
                     availableH / m_canvasSize.height());
    // Never magnify past 1:1; a two-item snapshot should not become two huge
    // tiles. The floor keeps names legible: below it the map scrolls instead of
    // shrinking further.
    m_zoom = qBound(0.55, qMin(fit, 1.0), 2.4);
    m_panOffset = QPointF(0, 0);
    m_userZoomed = false;
    updateScrollRanges();
    viewport()->update();
}

void LayoutView::setZoom(qreal zoom)
{
    const qreal clamped = qBound(0.35, zoom, 3.0);
    if (qFuzzyCompare(clamped, m_zoom))
        return;
    m_zoom = clamped;
    m_userZoomed = true;
    updateScrollRanges();
    viewport()->update();
}

void LayoutView::resizeEvent(QResizeEvent *event)
{
    QAbstractScrollArea::resizeEvent(event);
    if (m_mode == Mode::List) {
        rebuildList();
        return;
    }
    if (!m_userZoomed)
        fitToView();
    else
        updateScrollRanges();
}

QList<QString> LayoutView::visiblePaths() const
{
    QList<QString> paths;
    paths.reserve(m_tiles.size());
    for (const Tile &tile : m_tiles)
        paths.append(tile.path);
    return paths;
}

QHash<QString, QRectF> LayoutView::tileRects() const
{
    // m_origin already includes the scroll offset (see updateScrollRanges), so
    // it must not be subtracted again here. Doing so moved every reported
    // rectangle by the scroll amount and made these rectangles disagree with
    // what paintEvent actually drew.
    QHash<QString, QRectF> rects;
    rects.reserve(m_tiles.size());
    for (const Tile &tile : m_tiles) {
        const QRectF onScreen(m_origin.x() + tile.rect.x() * m_zoom,
                              m_origin.y() + tile.rect.y() * m_zoom,
                              tile.rect.width() * m_zoom,
                              tile.rect.height() * m_zoom);
        rects.insert(tile.path, onScreen);
    }
    return rects;
}

int LayoutView::hitTest(const QPointF &viewportPos) const
{
    // m_zoom is always positive in practice, but a zero here would turn every
    // coordinate into a NaN and silently break clicking, so guard it.
    if (m_zoom <= 0.0)
        return -1;
    // Tiles are stored in canvas space; convert the pointer instead of moving
    // every tile on every scroll.
    const QPointF canvasPos = (viewportPos - m_origin) / m_zoom;
    for (int i = 0; i < m_tiles.size(); ++i) {
        if (m_tiles.at(i).rect.contains(canvasPos))
            return i;
    }
    return -1;
}

void LayoutView::updateHover(const QPoint &viewportPos)
{
    const int index = hitTest(viewportPos);
    const QString path = index >= 0 ? m_tiles.at(index).path : QString();
    if (path == m_hoveredPath)
        return;
    m_hoveredPath = path;
    viewport()->setCursor(path.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
    viewport()->update();
}

void LayoutView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QAbstractScrollArea::mousePressEvent(event);
        return;
    }
    const int index = hitTest(event->position());
    if (index >= 0) {
        const QString path = m_tiles.at(index).path;
        if (path != m_selectedPath) {
            m_selectedPath = path;
            emit entrySelected(path);
            emit viewChanged();
        }
        viewport()->update();
    } else {
        // Dragging empty canvas pans the map. The pan already in effect is kept
        // as the new baseline, so a second drag continues from where the first
        // left off instead of snapping the view back.
        m_dragging = true;
        m_dragStart = event->pos();
        m_panStartOffset = m_panOffset;
        viewport()->setCursor(Qt::ClosedHandCursor);
    }
}

void LayoutView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton)) {
        const QPoint delta = event->pos() - m_dragStart;
        m_panOffset = m_panStartOffset + QPointF(delta.x(), delta.y());
        updateScrollRanges();
        viewport()->update();
        return;
    }
    if (m_mode == Mode::Map)
        updateHover(event->pos());
}

void LayoutView::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        viewport()->setCursor(m_hoveredPath.isEmpty() ? Qt::ArrowCursor
                                                      : Qt::PointingHandCursor);
        return;
    }
    QAbstractScrollArea::mouseReleaseEvent(event);
}

void LayoutView::mouseDoubleClickEvent(QMouseEvent *event)
{
    const int index = hitTest(event->position());
    if (index >= 0)
        emit entryActivated(m_tiles.at(index).path);
}

void LayoutView::leaveEvent(QEvent *event)
{
    QAbstractScrollArea::leaveEvent(event);
    if (!m_hoveredPath.isEmpty()) {
        m_hoveredPath.clear();
        viewport()->update();
    }
}

void LayoutView::wheelEvent(QWheelEvent *event)
{
    if (m_mode == Mode::Map && (event->modifiers() & Qt::ControlModifier)) {
        const qreal factor = event->angleDelta().y() > 0 ? 1.1 : 1.0 / 1.1;
        setZoom(m_zoom * factor);
        event->accept();
        return;
    }
    QAbstractScrollArea::wheelEvent(event);
}

QString LayoutView::elided(const QString &text, const QFont &font, qreal width) const
{
    // Right-elide: the start of a file name is what identifies it, and the
    // extension is repeated in the tile's metadata line below.
    return QFontMetrics(font).elidedText(text, Qt::ElideRight, int(width));
}

void LayoutView::paintEvent(QPaintEvent *)
{
    QPainter p(viewport());
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(viewport()->rect(), QColor(QStringLiteral("#0b0d10")));

    // Draw the empty state whenever there is nothing to show. This used to be
    // gated on the session also being empty, so filtering every item out left a
    // blank canvas with no explanation of why.
    if (m_tiles.isEmpty()) {
        p.setPen(QColor(QStringLiteral("#4f555e")));
        QFont f = font();
        f.setPointSizeF(9.5);
        p.setFont(f);
        p.drawText(viewport()->rect(), Qt::AlignCenter, m_emptyMessage);
        return;
    }

    p.save();
    if (m_mode == Mode::Map) {
        p.translate(m_origin);
        p.scale(m_zoom, m_zoom);

        // A faint grid, so relative positions are legible even when the layout
        // is sparse. It is a backdrop, not information.
        if (m_zoom > 0.4) {
            const qreal stepX = (kTileWidth + kTileGap);
            const qreal stepY = (kTileHeight + kTileGap);
            p.setPen(QPen(QColor(255, 255, 255, 6), 1.0));
            for (qreal x = kMargin; x < m_canvasSize.width(); x += stepX)
                p.drawLine(QPointF(x, kMargin),
                           QPointF(x, m_canvasSize.height() - kMargin));
            for (qreal y = kMargin; y < m_canvasSize.height(); y += stepY)
                p.drawLine(QPointF(kMargin, y),
                           QPointF(m_canvasSize.width() - kMargin, y));
        }
    } else {
        p.translate(0, m_origin.y());
    }

    QFont nameFont = font();
    nameFont.setPointSizeF(qMax(7.0, 8.8));
    QFont metaFont = font();
    metaFont.setPointSizeF(qMax(6.5, 7.6));

    const bool map = m_mode == Mode::Map;
    const QRectF clipRect = map ? QRectF(QPointF(0, 0), m_canvasSize).adjusted(-2, -2, 2, 2)
                                : QRectF(0, 0, m_canvasSize.width(), m_canvasSize.height());
    p.setClipRect(clipRect);

    for (const Tile &tile : m_tiles) {
        const bool selected = tile.path == m_selectedPath;
        const bool hovered = tile.path == m_hoveredPath;
        const QColor kindTint = EntryIcon::kindColor(tile.kind);

        if (map) {
            QColor fill(QStringLiteral("#14171c"));
            QColor border(QStringLiteral("#232830"));
            if (hovered) {
                fill = QColor(QStringLiteral("#191d23"));
                border = QColor(QStringLiteral("#333a44"));
            }
            if (selected) {
                fill = QColor(QStringLiteral("#1b2027"));
                border = Theme::accent();
            }

            p.setPen(QPen(border, selected ? 1.4 : 1.0));
            p.setBrush(fill);
            p.drawRoundedRect(tile.rect.adjusted(0.5, 0.5, -0.5, -0.5),
                              kTileRadius, kTileRadius);

            // Kind stripe: a 2px bar at the left edge, which is enough to read
            // the type at a glance without colouring the whole tile.
            QPainterPath stripe;
            stripe.addRoundedRect(QRectF(tile.rect.left() + 0.5,
                                         tile.rect.top() + 7.0, 2.0,
                                         tile.rect.height() - 14.0),
                                  1.0, 1.0);
            p.fillPath(stripe, kindTint);

            const QRectF iconRect(tile.rect.left() + 12.0,
                                  tile.rect.top() + 11.0, kIconSize, kIconSize);
            const QPixmap icon = Icons::render(EntryIcon::kindIcon(tile.kind),
                                               int(kIconSize),
                                               selected ? QColor(Qt::white) : kindTint,
                                               devicePixelRatioF());
            p.drawPixmap(iconRect, icon, QRectF(icon.rect()));

            if (tile.children >= 0 && tile.kind == Kind::Folder) {
                p.setPen(QColor(QStringLiteral("#5d646d")));
                p.setFont(metaFont);
                p.drawText(QRectF(tile.rect.right() - 34, tile.rect.top() + 8, 26, 12),
                           Qt::AlignRight | Qt::AlignVCenter,
                           QString::number(tile.children));
            }

            const qreal textLeft = tile.rect.left() + 11.0;
            const qreal textWidth = tile.rect.width() - 19.0;

            p.setPen(selected ? QColor(Qt::white) : QColor(QStringLiteral("#d3d8df")));
            p.setFont(nameFont);
            p.drawText(QRectF(textLeft, tile.rect.top() + 35.0, textWidth, 15.0),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       elided(tile.name, nameFont, textWidth));

            QString meta = tile.suffix == QLatin1String("dir")
                ? QStringLiteral("folder")
                : tile.suffix.toUpper();
            if (tile.size > 0)
                meta += QStringLiteral("  ·  ") + Theme::formatBytes(tile.size);

            p.setPen(QColor(QStringLiteral("#666d76")));
            p.setFont(metaFont);
            p.drawText(QRectF(textLeft, tile.rect.top() + 52.0, textWidth, 13.0),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       elided(meta, metaFont, textWidth));

            if (tile.viaShell) {
                // A dot marking a position that came from the real desktop
                // rather than the computed grid.
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(QStringLiteral("#5f7a63")));
                p.drawEllipse(QPointF(tile.rect.right() - 12.0,
                                      tile.rect.bottom() - 12.0), 2.0, 2.0);
            }
        } else {
            // List mode: one dense row per item.
            QRectF row = tile.rect.adjusted(0, 1, 0, -1);
            if (selected) {
                p.setPen(QPen(Theme::accent(), 1.0));
                p.setBrush(QColor(QStringLiteral("#1a1f26")));
                p.drawRoundedRect(row, 5, 5);
            } else if (hovered) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(QStringLiteral("#15181d")));
                p.drawRoundedRect(row, 5, 5);
            }

            const QRectF iconRect(row.left() + 8, row.center().y() - 7.0, 14.0, 14.0);
            const QPixmap icon = Icons::render(EntryIcon::kindIcon(tile.kind), 14,
                                               kindTint, devicePixelRatioF());
            p.drawPixmap(iconRect, icon, QRectF(icon.rect()));

            const qreal nameLeft = iconRect.right() + 9.0;
            const qreal metaWidth = 96.0;
            const qreal nameWidth = qMax(40.0, row.right() - nameLeft - metaWidth - 10.0);

            p.setPen(selected ? QColor(Qt::white) : QColor(QStringLiteral("#c7ccd4")));
            p.setFont(nameFont);
            p.drawText(QRectF(nameLeft, row.top(), nameWidth, row.height()),
                       Qt::AlignLeft | Qt::AlignVCenter,
                       elided(tile.name, nameFont, nameWidth));

            p.setPen(QColor(QStringLiteral("#666d76")));
            p.setFont(metaFont);
            p.drawText(QRectF(row.right() - metaWidth - 6, row.top(), metaWidth,
                              row.height()),
                       Qt::AlignRight | Qt::AlignVCenter,
                       elided(Theme::formatBytes(tile.size), metaFont, metaWidth));
        }
    }

    p.restore();
}
