#pragma once

#include "sessionmodel.h"

#include <QAbstractScrollArea>
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QPointF>
#include <QString>

class LayoutView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    enum class Mode { Map, List };

    enum class Sort { Name, Size, Modified, Kind };

    explicit LayoutView(QWidget *parent = nullptr);

    void setSession(const Session &session);
    void setSession(const Session &session, bool resetView);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setKindFilter(const QHash<int, bool> &enabledByKind);
    void setSearch(const QString &text);

    void setSort(Sort sort, bool descending = false);
    Sort sort() const { return m_sort; }

    QString selectedPath() const { return m_selectedPath; }
    QList<QString> visiblePaths() const;

    QHash<QString, QRectF> tileRects() const;

    void fitToView();
    void setZoom(qreal zoom);
    qreal zoom() const { return m_zoom; }

    bool moveSelection(int dx, int dy);

    bool clearSelection();

    bool activateSelection();

signals:
    void entryActivated(const QString &relativePath);
    void entrySelected(const QString &relativePath);
    void viewChanged();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void scrollContentsBy(int dx, int dy) override;

public:

    void ensureSelectionVisible();

private:

    void dropSelectionIfHidden();
    struct Tile {
        QString path;
        QRectF rect;
        QString name;
        Kind kind = Kind::Other;
        QString suffix;
        qint64 size = 0;
        bool viaShell = false;
        QString target;
        int children = -1;
    };

    void rebuild();
    void rebuildMap();
    void rebuildList();
    void updateScrollRanges();

    void updateOrigin();

    void resetScroll();
    void updateHover(const QPoint &viewportPos);

    qreal canvasScale() const { return m_mode == Mode::Map ? m_zoom : 1.0; }

    int hitTest(const QPointF &viewportPos) const;

    bool isVisible(const Entry &entry) const;
    QString elided(const QString &text, const QFont &font, qreal width) const;

    Session m_session;
    Mode m_mode = Mode::Map;
    QVector<Tile> m_tiles;
    QHash<int, bool> m_kindFilter;
    QString m_search;
    Sort m_sort = Sort::Name;
    bool m_sortDescending = false;
    QString m_selectedPath;
    QString m_hoveredPath;

    qreal m_zoom = 1.0;
    QSizeF m_canvasSize;
    QPointF m_origin;
    QPointF m_panOffset;
    QPointF m_panStartOffset;
    QPoint m_dragStart;
    bool m_dragging = false;
    bool m_userZoomed = false;
    QString m_emptyMessage;
};
