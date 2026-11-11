import QtQuick 2.15
import QtQuick.Controls 2.15

ScrollBar {
    id: sb
    policy: ScrollBar.AsNeeded
    implicitWidth: 8
    implicitHeight: 8
    padding: 2

    contentItem: Rectangle {
        implicitWidth: 4
        implicitHeight: 4
        radius: 2
        color: sb.pressed ? ThemePalette.selected
             : sb.hovered ? ThemePalette.muted
             : ThemePalette.scrollbarThumb
    }

    background: Rectangle {
        color: "transparent"
    }
}
