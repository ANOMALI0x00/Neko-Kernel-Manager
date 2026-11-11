import QtQuick 2.15
import QtQuick.Controls 2.15

Button {
    id: gtkBtn
    property bool suggested: false
    property bool destructive: false

    implicitHeight: 34
    leftPadding: 16
    rightPadding: 16
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    contentItem: Text {
        text: gtkBtn.text
        color: !gtkBtn.enabled ? ThemePalette.faint
             : gtkBtn.suggested ? ThemePalette.selectedText
             : gtkBtn.destructive ? ThemePalette.errorText
             : ThemePalette.text
        font.pixelSize: 12
        font.weight: (gtkBtn.suggested || gtkBtn.destructive) ? Font.DemiBold : Font.Normal
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        color: !gtkBtn.enabled ? ThemePalette.disabledBg
             : gtkBtn.suggested ? (gtkBtn.down ? Qt.darker(ThemePalette.selected, 1.15)
                                               : gtkBtn.hovered ? Qt.lighter(ThemePalette.selected, 1.10)
                                                                : ThemePalette.selected)
             : gtkBtn.destructive ? (gtkBtn.down ? Qt.darker(ThemePalette.error, 1.15)
                                                 : gtkBtn.hovered ? Qt.lighter(ThemePalette.error, 1.10)
                                                                  : ThemePalette.error)
             : gtkBtn.down ? ThemePalette.buttonActive
             : gtkBtn.hovered ? ThemePalette.buttonHover
             : ThemePalette.buttonBg
        border.color: gtkBtn.activeFocus ? ThemePalette.selected
                    : (gtkBtn.suggested || gtkBtn.destructive) ? "transparent"
                    : ThemePalette.buttonBorder
        border.width: gtkBtn.activeFocus ? 2 : 1
        radius: ThemePalette.cornerRadius
        Behavior on color { ColorAnimation { duration: 120 } }
    }
}
