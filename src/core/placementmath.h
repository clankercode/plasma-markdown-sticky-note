// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QSize>

#include <algorithm>
#include <limits>

// Pure geometry for desktop-note placement and dragging, kept free of QScreen/QWindow so it can be
// unit-tested against simulated multi-monitor layouts and a simulated (lagging) compositor.
// Coordinates: "global" = the compositor's logical layout (QScreen::geometry()); "position" = a
// note's top-left relative to its output's top-left (the layer-shell margins).
namespace PlacementMath {

// Minimum part of a note that must stay on its output so it can always be grabbed again.
constexpr int MinVisible = 48;

// Index of the output containing `p`, or of the nearest output when `p` falls in a gap of a
// non-rectangular layout (e.g. monitors with different heights / vertical offsets). -1 if none.
inline int outputAt(const QList<QRect> &outputs, QPoint p)
{
    int best = -1;
    qint64 bestDistance = std::numeric_limits<qint64>::max();
    for (int i = 0; i < outputs.size(); ++i) {
        const QRect &g = outputs.at(i);
        if (g.contains(p))
            return i;
        const qint64 dx = p.x() < g.left() ? g.left() - p.x() : (p.x() > g.right() ? p.x() - g.right() : 0);
        const qint64 dy = p.y() < g.top() ? g.top() - p.y() : (p.y() > g.bottom() ? p.y() - g.bottom() : 0);
        const qint64 distance = dx * dx + dy * dy;
        if (distance < bestDistance) {
            bestDistance = distance;
            best = i;
        }
    }
    return best;
}

// Clamps a position (relative to an output of size `area`) so at least MinVisible px of a note of
// size `note` remains on that output in each direction.
inline QPoint clamped(QPoint p, QSize area, QSize note)
{
    p.setX(std::clamp(p.x(), MinVisible - note.width(), std::max(MinVisible - note.width(), area.width() - MinVisible)));
    p.setY(std::clamp(p.y(), MinVisible - note.height(), std::max(MinVisible - note.height(), area.height() - MinVisible)));
    return p;
}

// One drag step. Pointer events are reported in surface-local coordinates relative to where the
// compositor has the surface NOW (`appliedPosition`: the last margins actually committed), which
// lags the margins we last requested. The new target is absolute — the press point stays under the
// pointer — so a late commit can never make moves compound (the 1.0.x "teleport" bug).
inline QPoint dragTarget(QPoint appliedPosition, QPointF localPointer, QPointF pressPoint)
{
    return appliedPosition + (localPointer - pressPoint).toPoint();
}

struct Placement {
    int output = -1;  // index into the outputs list
    QPoint position;  // relative to that output
};

// Where a note ends up after being dropped at `position` on output `from`: on the output under the
// note's centre (nearest output if the centre is in a gap), at the same global spot, clamped.
inline Placement settle(const QList<QRect> &outputs, int from, QPoint position, QSize note)
{
    if (from < 0 || from >= outputs.size())
        return {};
    const QPoint global = outputs.at(from).topLeft() + position;
    int target = outputAt(outputs, global + QPoint(note.width() / 2, note.height() / 2));
    if (target < 0)
        target = from;
    const QRect &g = outputs.at(target);
    return {target, clamped(global - g.topLeft(), g.size(), note)};
}

} // namespace PlacementMath
