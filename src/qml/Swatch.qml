// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

// One paper colour: every tone a note needs, chosen so ink and muted meet WCAG AA on paper and strip.
QtObject {
    property string name
    final property color paper: "transparent" // sheet, top of the gradient
    final property color paperLow: "transparent" // sheet, bottom of the gradient
    final property color strip: "transparent" // adhesive strip behind the header
    final property color tab: "transparent" // index tab / colour dot (saturated)
    final property color faded: "transparent" // tab of a note that is out on the desktop
    final property color ink: "transparent" // body text (>= 9.5:1 on paper)
    final property color muted: "transparent" // placeholders, derived titles (>= 4.5:1)
    final property color selection: "transparent" // text selection background
}
