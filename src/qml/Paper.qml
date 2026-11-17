// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Effects
import QtQuick.Shapes

// A sheet of sticky-note paper: soft two-layer shadow, a faint top-to-bottom tint (the sheet is
// lit from above), a slightly deeper adhesive strip behind the header, and a dog-eared bottom-right
// corner that doubles as the resize grip. Static geometry; nothing animates while idle.
Item {
    id: paper

    property Swatch swatch: Theme.yellow
    property int stripHeight: Theme.headerHeight
    property bool lifted: false   // e.g. while being dragged: a slightly deeper shadow

    // Broad ambient shadow: analytical SDF, one quad, no offscreen blur passes.
    RectangularShadow {
        anchors.fill: sheet
        offset.x: 0
        offset.y: paper.lifted ? 5 : 4
        blur: paper.lifted ? 15 : 12
        spread: -2
        radius: Theme.radius
        color: Theme.shadow
        Behavior on offset.y { NumberAnimation { duration: Theme.animNormal } }
        Behavior on blur { NumberAnimation { duration: Theme.animNormal } }
    }
    // Tight contact shadow so the sheet sits on the desk rather than floating. It follows the sheet's
    // outline, so it is kept out of the dog-eared corner (two scissor clips, no extra passes): there the
    // paper is lifted and only the soft ambient shadow reaches the desk.
    component ContactShadow: RectangularShadow {
        offset.y: 1
        blur: 2
        radius: Theme.radius
        color: Theme.contactShadow
    }
    Item {
        x: -4; y: -4
        width: sheet.width - Theme.fold + 4
        height: sheet.height + 8
        clip: true
        ContactShadow { x: 4; y: 4; width: sheet.width; height: sheet.height }
    }
    Item {
        x: sheet.width - Theme.fold; y: -4
        width: Theme.fold + 4
        height: sheet.height - Theme.fold + 4
        clip: true
        ContactShadow { x: Theme.fold - sheet.width; y: 4; width: sheet.width; height: sheet.height }
    }

    // The sheet: a rounded rectangle with the bottom-right corner folded away.
    Shape {
        id: sheet
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        readonly property real r: Theme.radius
        readonly property real f: Theme.fold

        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: 0; y2: sheet.height
                GradientStop { position: 0; color: paper.swatch.paper }
                GradientStop { position: 1; color: paper.swatch.paperLow }
            }
            startX: sheet.r; startY: 0
            PathLine { x: sheet.width - sheet.r; y: 0 }
            PathArc { x: sheet.width; y: sheet.r; radiusX: sheet.r; radiusY: sheet.r }
            PathLine { x: sheet.width; y: sheet.height - sheet.f }
            PathLine { x: sheet.width - sheet.f; y: sheet.height }
            PathLine { x: sheet.r; y: sheet.height }
            PathArc { x: 0; y: sheet.height - sheet.r; radiusX: sheet.r; radiusY: sheet.r }
            PathLine { x: 0; y: sheet.r }
            PathArc { x: sheet.r; y: 0; radiusX: sheet.r; radiusY: sheet.r }
        }

        // Adhesive strip: the same paper, a shade deeper, fading out at its lower edge.
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: 0; y1: 0; x2: 0; y2: paper.stripHeight
                GradientStop { position: 0; color: paper.swatch.strip }
                GradientStop { position: 0.8; color: paper.swatch.strip }
                GradientStop { position: 1; color: Qt.alpha(paper.swatch.strip, 0) }
            }
            startX: sheet.r; startY: 0
            PathLine { x: sheet.width - sheet.r; y: 0 }
            PathArc { x: sheet.width; y: sheet.r; radiusX: sheet.r; radiusY: sheet.r }
            PathLine { x: sheet.width; y: paper.stripHeight }
            PathLine { x: 0; y: paper.stripHeight }
            PathLine { x: 0; y: sheet.r }
            PathArc { x: sheet.r; y: 0; radiusX: sheet.r; radiusY: sheet.r }
        }

        // The folded-over corner: the paper's back, a little darker, with a soft crease.
        ShapePath {
            strokeWidth: -1
            fillGradient: LinearGradient {
                x1: sheet.width - sheet.f; y1: sheet.height - sheet.f
                x2: sheet.width - sheet.f / 2; y2: sheet.height - sheet.f / 2
                GradientStop { position: 0; color: Qt.darker(paper.swatch.paperLow, 1.12) }
                GradientStop { position: 1; color: Qt.darker(paper.swatch.paperLow, 1.03) }
            }
            startX: sheet.width; startY: sheet.height - sheet.f
            PathLine { x: sheet.width - sheet.f + 2; y: sheet.height - sheet.f + 2 }
            PathLine { x: sheet.width - sheet.f; y: sheet.height }
            PathLine { x: sheet.width; y: sheet.height - sheet.f }
        }
    }
}
