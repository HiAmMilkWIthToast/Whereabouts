#pragma once

#include "sessionmodel.h"

#include <QAbstractScrollArea>
#include <QColor>
#include <QHash>
#include <QPixmap>
#include <QPointF>
#include <QString>

// The snapshot as a map: a dark canvas with one labelled tile per item, placed
// where the item actually sits.
//
// Positions read from Explorer are used verbatim, so a folder really does look
// the way it did. Everything else is laid out on a grid by the capture step.
// The view is an abstract canvas, not a screenshot: the whole layout is scaled
// to fit, so a 2560px-wide desktop is readable in a small window.
class LayoutView : public QAbstractScrollArea
{
    Q_OBJECT
public:
    enum class Mode { Map, List };

    explicit LayoutView(QWidget *parent = nullptr);

    void setSession(const Session &session);

    void setMode(Mode mode);
    Mode mode() const { return m_mode; }

    void setKindFilter(const QHash<int, bool> &enabledByKind);
    void setSearch(const QString &text);

    QString selectedPath() const { return m_selectedPath; }
    QList<QString> visiblePaths() const;

    // Where each visible tile is drawn, in viewport pixels. Exists so tests can
    // assert that tiles really land in different places instead of all
    // collapsing onto one spot.
    QHash<QString, QRectF> tileRects() const;

    // Scales the canvas to fit the viewport. Called automatically on resize
    // unless the user has taken over with the zoom controls.
    void fitToView();
    void setZoom(qreal zoom);
    qreal zoom() const { return m_zoom; }

signals:
    void entryActivated(const QString &relativePath);
    void entrySelected(const QString &relativePath);
    void viewChanged();   // selection, zoom or filtering changed anything visible

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Tile {
        QString path;
        QRectF rect;            // canvas coordinates
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
    void updateHover(const QPoint &viewportPos);

    int hitTest(const QPointF &viewportPos) const;

    bool isVisible(const Entry &entry) const;
    QString elided(const QString &text, const QFont &font, qreal width) const;

    Session m_session;
    Mode m_mode = Mode::Map;
    QVector<Tile> m_tiles;
    QHash<int, bool> m_kindFilter;
    QString m_search;
    QString m_selectedPath;
    QString m_hoveredPath;

    qreal m_zoom = 1.0;
    QSizeF m_canvasSize;        // natural size of the laid-out content
    QPointF m_origin;           // canvas offset inside the viewport
    QPointF m_panOffset;        // user pan, added on top of the centring
    QPoint m_dragStart;
    bool m_dragging = false;
    bool m_userZoomed = false;
    QString m_emptyMessage;
};
