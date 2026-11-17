// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

// Invisible edge/corner strip that hands an interactive resize to the window manager.
MouseArea {
    required property int edges

    acceptedButtons: Qt.LeftButton
    hoverEnabled: true
    cursorShape: {
        const h = (edges & (Qt.LeftEdge | Qt.RightEdge)) !== 0
        const v = (edges & (Qt.TopEdge | Qt.BottomEdge)) !== 0
        if (h && v)
            return (edges === (Qt.TopEdge | Qt.LeftEdge) || edges === (Qt.BottomEdge | Qt.RightEdge))
                    ? Qt.SizeFDiagCursor : Qt.SizeBDiagCursor
        return h ? Qt.SizeHorCursor : Qt.SizeVerCursor
    }
    onPressed: if (Window.window) Window.window.startSystemResize(edges)
}
