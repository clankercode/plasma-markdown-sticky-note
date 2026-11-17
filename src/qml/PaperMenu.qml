// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

// An index-card menu: off-white, hairline edge, generous rows. Opens as its own popup window so
// it can extend past the edges of small desktop notes.
T.Menu {
    id: control

    implicitWidth: Math.max(implicitBackgroundWidth + leftInset + rightInset,
                            implicitContentWidth + leftPadding + rightPadding)
    implicitHeight: Math.max(implicitBackgroundHeight + topInset + bottomInset,
                             implicitContentHeight + topPadding + bottomPadding)

    popupType: T.Popup.Window
    padding: 5
    margins: 6
    overlap: 4
    font: Theme.uiFont

    delegate: PaperMenuItem {}

    contentItem: ListView {
        implicitHeight: contentHeight
        model: control.contentModel
        interactive: false
        clip: true
        currentIndex: control.currentIndex
    }

    background: Rectangle {
        implicitWidth: 210
        implicitHeight: 32
        radius: 8
        color: Theme.surface
        border.color: Theme.hairline
    }
}
