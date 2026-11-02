#include "icons.h"

namespace Icons {

namespace {

// Common wrapper for stroked glyphs. Kept as a macro-ish helper so adding a
// glyph stays a single readable line.
#define STROKE(body) \
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 24 24\" fill=\"none\" " \
    "stroke=\"#000\" stroke-width=\"1.8\" stroke-linecap=\"round\" " \
    "stroke-linejoin=\"round\">" body "</svg>"

} // namespace

const char *svgFor(Id id)
{
    switch (id) {
    case Id::Cairn:
        // Stacked stones: the app's mark, and what the name refers to.
        return STROKE("<ellipse cx=\"12\" cy=\"18.5\" rx=\"7\" ry=\"2.6\"/>"
                      "<path d=\"M6.4 15.1h11.2\"/>"
                      "<path d=\"M7.9 12.1h8.2\"/>"
                      "<path d=\"M9.4 9.1h5.2\"/>"
                      "<ellipse cx=\"12\" cy=\"6.6\" rx=\"2.4\" ry=\"1.6\"/>");
    case Id::CairnCompact:
        // The same stones with fewer of them and more air between the ones that
        // are left. At 16px the full mark's 3px gaps fall inside a single pixel
        // and the stones merge into one blob; three stones with wider spacing
        // still read as a stack.
        return STROKE("<ellipse cx=\"12\" cy=\"18.6\" rx=\"7\" ry=\"2.8\"/>"
                      "<path d=\"M7 13.4h10\"/>"
                      "<path d=\"M9.2 8.2h5.6\"/>");
    case Id::Layout:
        return STROKE("<rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"2.5\"/>"
                      "<path d=\"M3 9h18\"/><path d=\"M9 21V9\"/>");
    case Id::Folder:
        return STROKE("<path d=\"M4 20a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2h4l2 3h6a2 2 0 0 1 2 2v9a2 2 0 0 1-2 2z\"/>");
    case Id::File:
        return STROKE("<path d=\"M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7z\"/>"
                      "<path d=\"M14 2v5h5\"/>");
    case Id::Archive:
        return STROKE("<rect x=\"3\" y=\"3\" width=\"18\" height=\"5\" rx=\"1\"/>"
                      "<path d=\"M5 8v11a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2V8\"/>"
                      "<path d=\"M10 12h4\"/>");
    case Id::Executable:
        return STROKE("<rect x=\"2.5\" y=\"4\" width=\"19\" height=\"16\" rx=\"2.5\"/>"
                      "<path d=\"m7 9 3 3-3 3\"/><path d=\"M13 15h4\"/>");
    case Id::Image:
        return STROKE("<rect x=\"3\" y=\"3\" width=\"18\" height=\"18\" rx=\"2.5\"/>"
                      "<circle cx=\"8.5\" cy=\"8.5\" r=\"1.6\"/>"
                      "<path d=\"m21 15-5-5L5 21\"/>");
    case Id::Code:
        return STROKE("<path d=\"m9 18-6-6 6-6\"/><path d=\"m15 6 6 6-6 6\"/>");
    case Id::Text:
        return STROKE("<path d=\"M15 2H6a2 2 0 0 0-2 2v16a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2V7z\"/>"
                      "<path d=\"M14 2v5h5\"/><path d=\"M8 13h8\"/><path d=\"M8 17h5\"/>");
    case Id::Link:
        return STROKE("<path d=\"M10 13a5 5 0 0 0 7 0l3-3a5 5 0 0 0-7-7l-1 1\"/>"
                      "<path d=\"M14 11a5 5 0 0 0-7 0l-3 3a5 5 0 0 0 7 7l1-1\"/>");
    case Id::Search:
        return STROKE("<circle cx=\"11\" cy=\"11\" r=\"7\"/><path d=\"m20 20-4.3-4.3\"/>");
    case Id::Plus:
        return STROKE("<path d=\"M12 5v14\"/><path d=\"M5 12h14\"/>");
    case Id::Info:
        return STROKE("<circle cx=\"12\" cy=\"12\" r=\"9\"/><path d=\"M12 11v5\"/>"
                      "<path d=\"M12 8h.01\"/>");
    case Id::Close:
        return STROKE("<path d=\"M18 6 6 18\"/><path d=\"m6 6 12 12\"/>");
    case Id::Minimize:
        return STROKE("<path d=\"M5 12h14\"/>");
    case Id::Maximize:
        return STROKE("<rect x=\"5\" y=\"5\" width=\"14\" height=\"14\" rx=\"1.5\"/>");
    case Id::Restore:
        return STROKE("<rect x=\"4\" y=\"8\" width=\"12\" height=\"12\" rx=\"1.5\"/>"
                      "<path d=\"M8 8V5a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v10a1 1 0 0 1-1 1h-3\"/>");
    case Id::Grid:
        return STROKE("<rect x=\"3\" y=\"3\" width=\"7.5\" height=\"7.5\" rx=\"1.5\"/>"
                      "<rect x=\"13.5\" y=\"3\" width=\"7.5\" height=\"7.5\" rx=\"1.5\"/>"
                      "<rect x=\"3\" y=\"13.5\" width=\"7.5\" height=\"7.5\" rx=\"1.5\"/>"
                      "<rect x=\"13.5\" y=\"13.5\" width=\"7.5\" height=\"7.5\" rx=\"1.5\"/>");
    }
    return "";
}

QPixmap alphaMask(Id id, int logicalSize, qreal dpr)
{
    const int px = qMax(1, qRound(logicalSize * dpr));
    QPixmap pix(px, px);
    pix.fill(Qt::transparent);

    QSvgRenderer renderer(QByteArray(svgFor(id)));
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);
    renderer.render(&p, QRectF(0, 0, px, px));
    p.end();

    pix.setDevicePixelRatio(dpr);
    return pix;
}

QPixmap render(Id id, int logicalSize, const QColor &color, qreal dpr)
{
    return tint(alphaMask(id, logicalSize, dpr), color);
}

} // namespace Icons
