import QtQuick 2.15
import QtQuick.Controls 2.15

Dialog {
    id: dlg
    x: parent ? Math.round((parent.width - width) / 2) : 0
    y: parent ? Math.round((parent.height - height) / 2) : 0
    modal: true
    focus: true
    padding: 16
    topPadding: 14
    bottomPadding: 14

    Overlay.modal: Rectangle {
        color: ThemePalette.modalScrim
    }

    header: Item {
        implicitHeight: 44
        Text {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            text: dlg.title
            color: ThemePalette.text
            font.pixelSize: 14
            font.weight: Font.DemiBold
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        Rectangle {
            anchors.bottom: parent.bottom
            width: parent.width
            height: 1
            color: ThemePalette.border
        }
    }

    background: Rectangle {
        color: ThemePalette.popover
        border.color: ThemePalette.strongBorder
        border.width: 1
        radius: ThemePalette.dialogRadius
    }
}
