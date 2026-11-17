// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Templates as T

T.MenuSeparator {
    implicitWidth: 180
    implicitHeight: 9
    topPadding: 4
    bottomPadding: 4
    leftPadding: 8
    rightPadding: 8
    contentItem: Rectangle {
        implicitHeight: 1
        color: Theme.hairline
    }
}
