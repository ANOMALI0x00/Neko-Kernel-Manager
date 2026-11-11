pragma Singleton
import QtQuick 2.15

QtObject {
    id: palette

    // Core surface & foreground tokens (driven by ThemeManager / CSS)
    readonly property color window: theme.windowBg
    readonly property color view: theme.viewBg
    readonly property color surface: theme.cardBg
    readonly property color toolbar: theme.headerbarBg
    readonly property color headerText: theme.headerbarFg
    readonly property color popover: theme.popoverBg
    readonly property color text: theme.windowFg
    readonly property color muted: theme.mutedFg
    readonly property color faint: theme.faintFg

    // Interactive overlays & borders derived dynamically from active foreground/background
    readonly property color border: Qt.alpha(text, 0.12)
    readonly property color strongBorder: Qt.alpha(text, 0.22)
    readonly property color hover: Qt.alpha(text, 0.08)
    readonly property color alternateRow: Qt.alpha(text, 0.02)
    readonly property color active: Qt.rgba(0, 0, 0, 0.25)
    readonly property color modalScrim: Qt.rgba(0, 0, 0, 0.58)
    readonly property color disabledBg: Qt.alpha(text, 0.05)

    // Accent & semantic status tokens
    readonly property color selected: theme.accentBg
    readonly property color selectedText: theme.accentFg
    readonly property color selectedSubtle: Qt.alpha(selected, 0.18)
    readonly property color selectedPillBg: Qt.alpha(selected, 0.22)
    readonly property color selectedRowBorder: Qt.alpha(selectedText, 0.40)
    readonly property color selectedRowPillBg: Qt.alpha(selectedText, 0.20)
    readonly property color selectedRowDivider: Qt.rgba(0, 0, 0, 0.15)
    readonly property color error: theme.errorBg
    readonly property color errorText: theme.errorFg
    readonly property color errorSubtle: Qt.alpha(error, 0.16)
    readonly property color success: theme.successColor
    readonly property color warning: theme.warningColor
    readonly property color warningSubtle: Qt.alpha(warning, 0.18)
    readonly property color installedSubtle: Qt.alpha(text, 0.10)
    readonly property color info: theme.infoColor

    // Controls
    readonly property color buttonBg: theme.buttonBg
    readonly property color buttonHover: theme.buttonHoverBg
    readonly property color buttonActive: theme.buttonActiveBg
    readonly property color buttonBorder: Qt.alpha(text, 0.15)
    readonly property color inputBg: theme.inputBg
    readonly property color inputBorder: Qt.alpha(text, 0.16)
    readonly property color scrollbarThumb: Qt.alpha(text, 0.22)

    // Metrics & Typography
    readonly property real cornerRadius: 6
    readonly property real dialogRadius: 8
    readonly property string fontIcon: "0xProto Nerd Font"
    readonly property string fontMono: "0xProto Nerd Font Mono"
}
