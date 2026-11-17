// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Shapes

// A 16x16 vector glyph from Glyphs. Static geometry: tessellated once, re-coloured for free.
Shape {
    id: icon

    property string path
    property color color: Theme.chromeInk
    property real strokeWidth: 1.5
    property bool filled: false

    implicitWidth: 16
    implicitHeight: 16
    preferredRendererType: Shape.CurveRenderer

    ShapePath {
        strokeColor: icon.filled ? "transparent" : icon.color
        strokeWidth: icon.filled ? -1 : icon.strokeWidth
        fillColor: icon.filled ? icon.color : "transparent"
        capStyle: ShapePath.RoundCap
        joinStyle: ShapePath.RoundJoin
        PathSvg { path: icon.path }
    }
}
