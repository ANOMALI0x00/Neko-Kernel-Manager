import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

Button {
    id: hdrBtn
    property string symbol: ""

    implicitHeight: 34
    leftPadding: 12
    rightPadding: 12
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    contentItem: RowLayout {
        spacing: 6
        Text {
            visible: hdrBtn.symbol.length > 0
            text: hdrBtn.symbol
            color: !hdrBtn.enabled ? ThemePalette.faint
                 : hdrBtn.checked ? ThemePalette.selected
                 : ThemePalette.text
            font.family: ThemePalette.fontIcon
            font.pixelSize: 15
            verticalAlignment: Text.AlignVCenter
        }
        Text {
            visible: hdrBtn.text.length > 0
            text: hdrBtn.text
            color: !hdrBtn.enabled ? ThemePalette.faint
                 : hdrBtn.checked ? ThemePalette.selected
                 : ThemePalette.text
            font.pixelSize: 12
            font.weight: hdrBtn.checked ? Font.DemiBold : Font.Normal
            verticalAlignment: Text.AlignVCenter
        }
    }

    background: Rectangle {
        color: !hdrBtn.enabled ? "transparent"
             : hdrBtn.checked ? ThemePalette.selectedSubtle
             : hdrBtn.down ? ThemePalette.buttonActive
             : hdrBtn.hovered ? ThemePalette.buttonHover
             : ThemePalette.buttonBg
        border.color: hdrBtn.checked ? ThemePalette.selected
                    : hdrBtn.activeFocus ? ThemePalette.selected
                    : ThemePalette.buttonBorder
        border.width: (hdrBtn.activeFocus || hdrBtn.checked) ? 1.5 : 1
        radius: ThemePalette.cornerRadius
        Behavior on color { ColorAnimation { duration: 120 } }
    }

    ToolTip {
        id: hdrTip
        visible: hdrBtn.hovered && hdrBtn.text.length > 0
        delay: 500
        text: hdrBtn.text
        contentItem: Text {
            text: hdrTip.text
            color: ThemePalette.text
            font.pixelSize: 11
        }
        background: Rectangle {
            color: ThemePalette.popover
            border.color: ThemePalette.strongBorder
            border.width: 1
            radius: 4
        }
    }
}
