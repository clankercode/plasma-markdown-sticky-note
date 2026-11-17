// SPDX-License-Identifier: MIT
pragma Singleton
pragma ComponentBehavior: Bound
import QtQuick

// Design tokens: paper palette, neutral chrome for menus/popups, type scale, metrics.
QtObject {
    id: theme

    readonly property list<string> colorNames: ["yellow", "pink", "green", "blue", "purple", "orange", "gray"]

    readonly property Swatch yellow: Swatch {
        name: "yellow"; paper: "#fcefa4"; paperLow: "#f8e48c"; strip: "#f3dc7a"; tab: "#eac544"; faded: "#f4e9bf"
        ink: "#3a3112"; muted: "#6b6032"; selection: "#f0d466"
    }
    readonly property Swatch pink: Swatch {
        name: "pink"; paper: "#fadbe2"; paperLow: "#f6cdd7"; strip: "#f0bcc9"; tab: "#e48aa2"; faded: "#f2dcdf"
        ink: "#4a1c2a"; muted: "#754855"; selection: "#eca6b8"
    }
    readonly property Swatch green: Swatch {
        name: "green"; paper: "#deeeca"; paperLow: "#d1e6b6"; strip: "#c5dea4"; tab: "#8cbd67"; faded: "#e1ead0"
        ink: "#20361a"; muted: "#4c6241"; selection: "#a9ce8a"
    }
    readonly property Swatch blue: Swatch {
        name: "blue"; paper: "#d7e7f5"; paperLow: "#c9def0"; strip: "#b9d3eb"; tab: "#6fa0d2"; faded: "#dce6ec"
        ink: "#14304a"; muted: "#415c74"; selection: "#93b9de"
    }
    readonly property Swatch purple: Swatch {
        name: "purple"; paper: "#e6dff5"; paperLow: "#dbd1f0"; strip: "#cfc1ea"; tab: "#a389d5"; faded: "#e5deec"
        ink: "#2d2246"; muted: "#584e70"; selection: "#baa7e0"
    }
    readonly property Swatch orange: Swatch {
        name: "orange"; paper: "#fcdebf"; paperLow: "#f9d1a9"; strip: "#f4c394"; tab: "#ea9a4c"; faded: "#f4dfc9"
        ink: "#46260a"; muted: "#714f30"; selection: "#f0b274"
    }
    readonly property Swatch gray: Swatch {
        name: "gray"; paper: "#ece9e2"; paperLow: "#e1ddd4"; strip: "#d6d1c6"; tab: "#a8a296"; faded: "#e8e5dd"
        ink: "#2b2925"; muted: "#5c5a54"; selection: "#c0bbb1"
    }

    function swatch(name: string): Swatch {
        switch (name) {
        case "pink": return theme.pink
        case "green": return theme.green
        case "blue": return theme.blue
        case "purple": return theme.purple
        case "orange": return theme.orange
        case "gray": return theme.gray
        default: return theme.yellow
        }
    }

    function colorLabel(name: string): string {
        switch (name) {
        case "pink": return qsTr("Pink")
        case "green": return qsTr("Green")
        case "blue": return qsTr("Blue")
        case "purple": return qsTr("Purple")
        case "orange": return qsTr("Orange")
        case "gray": return qsTr("Gray")
        default: return qsTr("Yellow")
        }
    }

    // Links and task boxes in rendered Markdown. Qt's Markdown importer colours links from the
    // *application* palette (QPalette::Link) at parse time; per-item palettes and linkColor are ignored.
    // main.cpp should set this as the application link colour: a ballpoint-pen blue that reads as
    // handwriting on every paper (>= 6.9:1 on all papers, >= 4.6:1 on selections).
    readonly property color penInk: "#1f3c8a"

    // Neutral chrome (menus, popups, tooltips): an off-white index card, not the Plasma theme.
    readonly property color surface: "#fbfaf7"
    readonly property color surfaceAlt: "#efece5"
    readonly property color hairline: "#d9d4ca"
    readonly property color chromeInk: "#2b2925"   // 13.9:1 on surface
    readonly property color chromeMuted: "#69655d" // 5.6:1 on surface
    readonly property color danger: "#a3261b"      // 7.1:1 on surface
    readonly property color shadow: Qt.rgba(0.20, 0.14, 0.04, 0.30)
    readonly property color contactShadow: Qt.rgba(0.20, 0.14, 0.04, 0.22)

    // Type scale: everything follows the system font, a touch larger than UI text for reading comfort.
    // FontMetrics' default font is the application (system) font.
    readonly property FontMetrics systemMetrics: FontMetrics {}
    readonly property real basePt: theme.systemMetrics.font.pointSize > 0 ? theme.systemMetrics.font.pointSize : 10
    readonly property string family: theme.systemMetrics.font.family
    readonly property font bodyFont: Qt.font({ family: theme.family, pointSize: Math.round(theme.basePt * 2.4) / 2 })
    readonly property font titleFont: Qt.font({ family: theme.family, pointSize: Math.round(theme.basePt * 2.5) / 2, weight: Font.DemiBold })
    readonly property font uiFont: Qt.font({ family: theme.family, pointSize: theme.basePt })
    readonly property font uiStrongFont: Qt.font({ family: theme.family, pointSize: theme.basePt, weight: Font.DemiBold })
    readonly property font smallFont: Qt.font({ family: theme.family, pointSize: Math.max(7.5, Math.round(theme.basePt * 1.8) / 2) })

    // Metrics (logical px, 4 px grid).
    readonly property int radius: 7
    readonly property int fold: 18           // dog-ear at the bottom-right corner (also the resize grip)
    readonly property int headerHeight: 42
    readonly property int pad: 14            // text inset from the paper edge
    readonly property int shadowMargin: 14   // transparent window margin that holds the shadow
    // Desktop notes: the window is the note's stored size, so the paper is inset just enough to hold
    // the shadow's actual reach (it falls ~2 px above, ~8 px beside, ~13 px below the sheet).
    readonly property int deskInsetTop: 4
    readonly property int deskInsetSide: 9
    readonly property int deskInsetBottom: 14
    readonly property int tabHeight: 84        // preferred; tabs shrink to fit (min tabMinHeight)
    readonly property int tabMinHeight: 26
    readonly property int tabSpacing: 4
    readonly property int tabStub: 24        // how far a tab sticks out from the page
    readonly property int tabTuck: 8         // how far a tab hides under the page
    readonly property int animFast: 110
    readonly property int animNormal: 150
}
