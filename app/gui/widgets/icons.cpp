#include "gui/widgets/icons.h"

#define _USE_MATH_DEFINES
#include <cmath>

#include <QColor>
#include <QPainter>
#include <QPainterPath>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>

namespace dm::icons {

// All shapes are authored in a 24x24 space, then scaled to `size`.
static void draw(QPainter& p, const QString& n)
{
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(p.pen());
    pen.setWidthF(2.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    auto rr = [&](qreal x, qreal y, qreal w, qreal h, qreal r) {
        p.drawRoundedRect(QRectF(x, y, w, h), r, r);
    };

    if (n == "environment") {           // monitor
        rr(3, 4, 18, 12, 2);
        p.drawLine(QPointF(9, 20), QPointF(15, 20));
        p.drawLine(QPointF(12, 16), QPointF(12, 20));
    } else if (n == "skills") {         // star
        QPolygonF s;
        for (int i = 0; i < 10; ++i) {
            qreal a = -M_PI / 2 + i * M_PI / 5;
            qreal rad = (i % 2 == 0) ? 9 : 3.6;
            s << QPointF(12 + rad * std::cos(a), 12 + rad * std::sin(a));
        }
        p.drawPolygon(s);
    } else if (n == "plugins") {        // plug
        p.drawLine(QPointF(9, 3), QPointF(9, 8));
        p.drawLine(QPointF(15, 3), QPointF(15, 8));
        rr(6, 8, 12, 7, 2);
        p.drawLine(QPointF(12, 15), QPointF(12, 21));
    } else if (n == "packages" || n == "container") {   // box
        QPainterPath path;
        path.moveTo(12, 3); path.lineTo(21, 7.5); path.lineTo(21, 16.5);
        path.lineTo(12, 21); path.lineTo(3, 16.5); path.lineTo(3, 7.5);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(3, 7.5), QPointF(12, 12));
        p.drawLine(QPointF(21, 7.5), QPointF(12, 12));
        p.drawLine(QPointF(12, 12), QPointF(12, 21));
    } else if (n == "env") {            // terminal / braces
        rr(3, 4, 18, 16, 2);
        p.drawPolyline(QPolygonF({QPointF(7, 9), QPointF(10, 12), QPointF(7, 15)}));
        p.drawLine(QPointF(12, 15), QPointF(17, 15));
    } else if (n == "snapshots") {      // layers
        QPolygonF d{QPointF(12, 3), QPointF(21, 8), QPointF(12, 13), QPointF(3, 8)};
        p.drawPolygon(d);
        p.drawPolyline(QPolygonF({QPointF(3, 13), QPointF(12, 18), QPointF(21, 13)}));
    } else if (n == "restore") {        // rotate-ccw
        QRectF a(4, 4, 16, 16);
        p.drawArc(a, 60 * 16, 250 * 16);
        p.drawPolyline(QPolygonF({QPointF(4, 4), QPointF(4, 9), QPointF(9, 9)}));
    } else if (n == "settings") {       // gear
        p.drawEllipse(QPointF(12, 12), 3.2, 3.2);
        for (int i = 0; i < 8; ++i) {
            qreal ang = i * M_PI / 4;
            QPointF a(12 + 6 * std::cos(ang), 12 + 6 * std::sin(ang));
            QPointF b(12 + 9 * std::cos(ang), 12 + 9 * std::sin(ang));
            p.drawLine(a, b);
        }
    } else if (n == "docs") {           // book
        p.drawPolyline(QPolygonF({QPointF(5, 4), QPointF(5, 20), QPointF(12, 18),
                                  QPointF(19, 20), QPointF(19, 4), QPointF(12, 6),
                                  QPointF(5, 4)}));
        p.drawLine(QPointF(12, 6), QPointF(12, 18));
    } else if (n == "about") {          // info
        p.drawEllipse(QPointF(12, 12), 9, 9);
        p.drawPoint(QPointF(12, 8));
        p.drawLine(QPointF(12, 11), QPointF(12, 16));
    } else if (n == "refresh") {        // rotate
        QRectF a(4, 4, 16, 16);
        p.drawArc(a, 40 * 16, 280 * 16);
        p.drawPolyline(QPolygonF({QPointF(20, 3), QPointF(20, 8), QPointF(15, 8)}));
    } else if (n == "copy") {
        rr(8, 8, 12, 12, 2);
        p.drawPolyline(QPolygonF({QPointF(6, 15), QPointF(4, 15), QPointF(4, 4),
                                  QPointF(15, 4), QPointF(15, 6)}));
    } else if (n == "folder") {
        p.drawPolyline(QPolygonF({QPointF(3, 7), QPointF(3, 19), QPointF(21, 19),
                                  QPointF(21, 9), QPointF(11, 9), QPointF(9, 6),
                                  QPointF(3, 6), QPointF(3, 7)}));
    } else if (n == "search") {
        p.drawEllipse(QPointF(11, 11), 6, 6);
        p.drawLine(QPointF(15.5, 15.5), QPointF(20, 20));
    } else if (n == "cpu") {            // runtimes
        rr(6, 6, 12, 12, 2);
        rr(9, 9, 6, 6, 1);
        for (qreal x : {9.0, 12.0, 15.0}) {
            p.drawLine(QPointF(x, 3), QPointF(x, 6));
            p.drawLine(QPointF(x, 18), QPointF(x, 21));
            p.drawLine(QPointF(3, x), QPointF(6, x));
            p.drawLine(QPointF(18, x), QPointF(21, x));
        }
    } else if (n == "database") {       // build / db
        p.drawEllipse(QRectF(4, 3, 16, 6));
        p.drawArc(QRectF(4, 9, 16, 6), 180 * 16, 180 * 16);
        p.drawArc(QRectF(4, 15, 16, 6), 180 * 16, 180 * 16);
        p.drawLine(QPointF(4, 6), QPointF(4, 18));
        p.drawLine(QPointF(20, 6), QPointF(20, 18));
    } else if (n == "wsl") {            // penguin-ish blob
        p.drawEllipse(QPointF(12, 12), 8, 9);
        p.drawEllipse(QPointF(9.5, 10), 1.2, 1.6);
        p.drawEllipse(QPointF(14.5, 10), 1.2, 1.6);
    } else if (n == "stop") {
        p.setBrush(p.pen().color());
        rr(7, 7, 10, 10, 1.5);
    } else if (n == "dot") {
        // painted by pixmap() as a filled circle
    } else {                            // fallback: circle
        p.drawEllipse(QPointF(12, 12), 8, 8);
    }
}

QPixmap pixmap(const QString& name, const QColor& color, int size)
{
    const qreal dpr = 2.0;
    QPixmap pm(int(size * dpr), int(size * dpr));
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.scale(size / 24.0, size / 24.0);
    p.setPen(color);
    p.setBrush(color);
    draw(p, name);
    if (name == "dot") {
        p.setBrush(color);
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(12, 12), 5, 5);
    }
    p.end();
    return pm;
}

QIcon icon(const QString& name, const QColor& color, int size)
{
    return QIcon(pixmap(name, color, size));
}

} // namespace dm::icons
