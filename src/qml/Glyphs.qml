// SPDX-License-Identifier: MIT
pragma Singleton
pragma ComponentBehavior: Bound
import QtQuick

// Hand-drawn 16x16 line icons (SVG path data), stroked by Icon.qml. Kept in code so the app owns its
// look regardless of the icon theme, and so icons take the ink colour of whatever paper they sit on.
QtObject {
    readonly property string close: "M4 4 L12 12 M12 4 L4 12"
    readonly property string plus: "M8 3 V13 M3 8 H13"
    readonly property string check: "M3.5 8.5 L6.5 11.5 L12.5 4.5"
    readonly property string back: "M9.5 3.5 L5 8 L9.5 12.5"
    readonly property string chevron: "M6 3.5 L10.5 8 L6 12.5"
    readonly property string popOut: "M7 2.5 H4 Q2.5 2.5 2.5 4 V12 Q2.5 13.5 4 13.5 H12 Q13.5 13.5 13.5 12 V9 M9.5 2.5 H13.5 V6.5 M13.5 2.5 L7.5 8.5"
    readonly property string dock: "M7 2.5 H4 Q2.5 2.5 2.5 4 V12 Q2.5 13.5 4 13.5 H12 Q13.5 13.5 13.5 12 V9 M13.5 2.5 L8 8 M8 4 V8 H12"
    readonly property string popped: "M5 11 L11 5 M6.5 5 H11 V9.5"
    readonly property string pin: "M5.5 2.5 H10.5 M6.5 2.5 V6 L4 9 H12 L9.5 6 V2.5 M8 9 V14"
    readonly property string archive: "M2.5 3 H13.5 V6 H2.5 Z M3.5 6 V12 Q3.5 13.5 5 13.5 H11 Q12.5 13.5 12.5 12 V6 M6.5 8.75 H9.5"
    readonly property string unarchive: "M2.5 3 H13.5 V6 H2.5 Z M3.5 6 V12 Q3.5 13.5 5 13.5 H11 Q12.5 13.5 12.5 12 V6 M8 12 V8.5 M6.25 10.25 L8 8.5 L9.75 10.25"
    readonly property string trash: "M2.5 4.5 H13.5 M6 4.5 V2.75 H10 V4.5 M4 4.5 L4.75 13.5 H11.25 L12 4.5"
    // Filled glyphs (three dots).
    readonly property string more: "M2.1 8 A1.4 1.4 0 1 0 4.9 8 A1.4 1.4 0 1 0 2.1 8 Z M6.6 8 A1.4 1.4 0 1 0 9.4 8 A1.4 1.4 0 1 0 6.6 8 Z M11.1 8 A1.4 1.4 0 1 0 13.9 8 A1.4 1.4 0 1 0 11.1 8 Z"
}
