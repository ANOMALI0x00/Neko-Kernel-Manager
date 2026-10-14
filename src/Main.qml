import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1 as Platform
import QtCore

ApplicationWindow {
    id: window

    visible: true
    width: 1180
    height: 780
    minimumWidth: 840
    minimumHeight: 620
    title: qsTr("Neko Kernel Manager")
    color: themeColors.window

    property string selectedKernelName: ""
    property bool installedOnly: false

    QtObject {
        id: themeColors

        function mix(a, b, amount) {
            return Qt.rgba(a.r + (b.r - a.r) * amount,
                           a.g + (b.g - a.g) * amount,
                           a.b + (b.b - a.b) * amount, 1.0)
        }

        readonly property color window: theme.windowBg
        readonly property color view: theme.viewBg
        readonly property color surface: theme.cardBg
        readonly property color toolbar: theme.headerbarBg
        readonly property color popover: theme.popoverBg
        readonly property color text: theme.windowFg
        readonly property color muted: mix(theme.windowFg, theme.windowBg, 0.38)
        readonly property color faint: mix(theme.windowFg, theme.windowBg, 0.62)
        readonly property color border: mix(theme.windowBg, theme.windowFg, 0.20)
        readonly property color strongBorder: mix(theme.windowBg, theme.windowFg, 0.34)
        readonly property color hover: mix(theme.cardBg, theme.windowFg, 0.08)
        readonly property color selected: theme.accentBg
        readonly property color selectedText: theme.accentFg
        readonly property color error: theme.errorBg
        readonly property color errorText: theme.errorFg
        readonly property color success: "#3a9b67"
        readonly property color warning: "#d9822b"
        readonly property color info: mix(theme.accentBg, theme.windowBg, 0.12)
    }

    readonly property var filteredKernels: {
        var result = bridge.kernels || []
        var query = kernelSearch.text.trim().toLowerCase()
        if (installedOnly)
            result = result.filter(function(kernel) { return kernel.installed })
        if (query.length > 0) {
            result = result.filter(function(kernel) {
                return (kernel.name || "").toLowerCase().indexOf(query) !== -1
                    || (kernel.version || "").toLowerCase().indexOf(query) !== -1
                    || (kernel.category || "").toLowerCase().indexOf(query) !== -1
                    || (kernel.type || "").toLowerCase().indexOf(query) !== -1
            })
        }
        return result
    }

    readonly property var selectedKernel: {
        var kernels = bridge.kernels || []
        for (var index = 0; index < kernels.length; ++index) {
            if (kernels[index].name === selectedKernelName)
                return kernels[index]
        }
        return null
    }

    readonly property int installedKernelCount: {
        var count = 0
        var kernels = bridge.kernels || []
        for (var index = 0; index < kernels.length; ++index) {
            if (kernels[index].installed)
                ++count
        }
        return count
    }

    function kernelIsRunning(kernel) {
        if (!kernel || !kernel.installed || bridge.activeKernelVersion === "")
            return false
        return bridge.activeKernelVersion === kernel.version
            || bridge.activeKernelVersion.indexOf(kernel.version) !== -1
            || kernel.version.indexOf(bridge.activeKernelVersion) !== -1
            || ((kernel.name || "").indexOf("manual") !== -1
                && bridge.activeKernelVersion.indexOf(kernel.version) !== -1)
    }

    function kernelIsDefault(kernel) {
        if (!kernel || !kernel.installed || bridge.defaultKernel === "")
            return false
        return bridge.defaultKernel === kernel.version
            || bridge.defaultKernel.indexOf(kernel.version) !== -1
            || bridge.defaultKernel.indexOf(kernel.name) !== -1
    }

    function selectFirstVisibleKernel(ensureVisible) {
        if (filteredKernels.length === 0) {
            selectedKernelName = ""
            kernelList.currentIndex = -1
            return
        }

        var selectedIndex = -1
        for (var index = 0; index < filteredKernels.length; ++index) {
            if (filteredKernels[index].name === selectedKernelName) {
                selectedIndex = index
                break
            }
        }

        if (selectedIndex < 0) {
            selectedIndex = 0
            selectedKernelName = filteredKernels[0].name
        }

        kernelList.currentIndex = selectedIndex
        if (ensureVisible)
            Qt.callLater(function() { kernelList.positionViewAtIndex(selectedIndex, ListView.Contain) })
    }

    function refreshKernels() {
        bridge.updateKernels()
        bridge.updateDefaultKernel()
    }

    component ToolbarAction: Button {
        id: toolbarAction
        property string symbol: ""
        implicitWidth: 94
        implicitHeight: 62
        padding: 0
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: text

        contentItem: Column {
            spacing: 3
            Text {
                width: parent.width
                text: toolbarAction.symbol
                color: toolbarAction.enabled ? themeColors.text : themeColors.faint
                font.family: "0xProto Nerd Font"
                font.pixelSize: 20
                horizontalAlignment: Text.AlignHCenter
            }
            Text {
                width: parent.width
                text: toolbarAction.text
                color: toolbarAction.enabled ? themeColors.text : themeColors.faint
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
            }
        }

        background: Rectangle {
            color: toolbarAction.down ? themeColors.border
                  : toolbarAction.hovered ? themeColors.hover : "transparent"
            border.color: toolbarAction.activeFocus ? themeColors.selected : "transparent"
            border.width: toolbarAction.activeFocus ? 2 : 1
        }

        ToolTip.visible: hovered
        ToolTip.delay: 500
        ToolTip.text: text
    }

    component FlatButton: Button {
        id: flatButton
        property bool primary: false
        property bool destructive: false
        implicitHeight: 36
        leftPadding: 14
        rightPadding: 14
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: text

        contentItem: Text {
            text: flatButton.text
            color: !flatButton.enabled ? themeColors.faint
                 : flatButton.primary ? themeColors.selectedText
                 : flatButton.destructive ? themeColors.error
                 : themeColors.text
            font.pixelSize: 13
            font.weight: flatButton.primary ? Font.DemiBold : Font.Normal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: !flatButton.enabled ? themeColors.hover
                 : flatButton.primary ? (flatButton.down ? Qt.darker(themeColors.selected, 1.16)
                                                       : flatButton.hovered ? Qt.lighter(themeColors.selected, 1.08)
                                                                            : themeColors.selected)
                 : flatButton.down ? themeColors.border
                 : flatButton.hovered ? themeColors.hover : themeColors.surface
            border.color: flatButton.activeFocus ? themeColors.selected
                        : flatButton.destructive ? themeColors.error : themeColors.border
            border.width: flatButton.activeFocus ? 2 : 1
            radius: 3
        }
    }

    menuBar: MenuBar {
        Menu {
            title: qsTr("File")
            MenuItem {
                text: qsTr("Export Logs…")
                enabled: bridge.logs.length > 0
                onTriggered: fileDialog.open()
            }
            MenuSeparator { }
            MenuItem { text: qsTr("Close"); onTriggered: window.close() }
        }
        Menu {
            title: qsTr("Edit")
            MenuItem {
                text: qsTr("Show Installed Kernels Only")
                checkable: true
                checked: window.installedOnly
                onToggled: {
                    window.installedOnly = checked
                    window.selectFirstVisibleKernel(true)
                }
            }
            MenuItem {
                text: qsTr("Clear Search")
                enabled: kernelSearch.text.length > 0
                onTriggered: kernelSearch.clear()
            }
        }
        Menu {
            title: qsTr("View")
            MenuItem {
                text: qsTr("Refresh Kernels")
                enabled: !bridge.busy
                onTriggered: window.refreshKernels()
            }
            MenuItem { text: qsTr("Activity Log"); onTriggered: logDialog.open() }
        }
        Menu {
            title: qsTr("Help")
            MenuItem { text: qsTr("About Neko Kernel Manager"); onTriggered: aboutDialog.open() }
        }

        delegate: MenuBarItem {
            id: menuItem
            contentItem: Text {
                text: menuItem.text
                color: themeColors.text
                font.pixelSize: 13
                verticalAlignment: Text.AlignVCenter
            }
            background: Rectangle { color: menuItem.highlighted ? themeColors.hover : "transparent" }
        }
        background: Rectangle {
            color: themeColors.toolbar
            border.color: themeColors.border
            border.width: 1
        }
    }

    Shortcut {
        sequence: "Ctrl+R"
        enabled: !bridge.busy
        onActivated: window.refreshKernels()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 74
            color: themeColors.toolbar

            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: themeColors.border }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 2

                ToolbarAction {
                    symbol: "\uf021"
                    text: qsTr("Refresh")
                    enabled: !bridge.busy
                    onClicked: window.refreshKernels()
                }
                ToolbarAction {
                    symbol: "\uf058"
                    text: qsTr("Installed")
                    checkable: true
                    checked: window.installedOnly
                    onToggled: {
                        window.installedOnly = checked
                        window.selectFirstVisibleKernel(true)
                    }
                }
                ToolbarAction {
                    symbol: "\uf1b3"
                    text: qsTr("DKMS")
                    enabled: !bridge.busy
                    onClicked: {
                        bridge.updateDkmsModules()
                        dkmsDialog.open()
                    }
                }

                ToolbarAction { symbol: "\uf15c"; text: qsTr("Logs"); onClicked: logDialog.open() }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.preferredHeight: 48
                    Layout.leftMargin: 6
                    Layout.rightMargin: 6
                    color: themeColors.border
                }

                ToolbarAction {
                    symbol: "\uf1f8"
                    text: qsTr("Remove Old")
                    enabled: !bridge.busy
                    onClicked: purgeDialog.open()
                }

                Item { Layout.fillWidth: true }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 58
            color: bridge.statusIsError ? themeColors.error : themeColors.info

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 12
                spacing: 12

                Text {
                    text: bridge.statusIsError ? "\uf071" : "\uf05a"
                    color: bridge.statusIsError ? themeColors.errorText : themeColors.selectedText
                    font.family: "0xProto Nerd Font"
                    font.pixelSize: 19
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Text {
                        Layout.fillWidth: true
                        text: bridge.statusIsError
                              ? qsTr("The last kernel operation reported an error")
                              : qsTr("Running kernel: %1").arg(bridge.activeKernelVersion || qsTr("detecting…"))
                        color: bridge.statusIsError ? themeColors.errorText : themeColors.selectedText
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: bridge.statusMessage !== ""
                              ? bridge.statusMessage
                              : qsTr("Select a kernel to inspect its details and available actions.")
                        color: bridge.statusIsError ? themeColors.errorText : themeColors.selectedText
                        opacity: 0.88
                        font.pixelSize: 12
                        elide: Text.ElideRight
                    }
                }

                FlatButton {
                    visible: bridge.statusIsError
                    text: bridge.statusIsError ? qsTr("View Logs") : qsTr("Refresh")
                    onClicked: logDialog.open()
                }
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 10
            Layout.rightMargin: 10
            Layout.topMargin: 8
            Layout.bottomMargin: 8
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1
                    Text {
                        text: qsTr("Linux kernels")
                        color: themeColors.text
                        font.pixelSize: 17
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: qsTr("%1 kernels · %2 installed")
                              .arg(bridge.kernels.length).arg(window.installedKernelCount)
                        color: themeColors.muted
                        font.pixelSize: 12
                    }
                }

                TextField {
                    id: kernelSearch
                    Layout.preferredWidth: Math.min(320, window.width * 0.31)
                    Layout.minimumWidth: 190
                    implicitHeight: 36
                    leftPadding: 10
                    rightPadding: 10
                    placeholderText: qsTr("Search by name, version, or type…")
                    color: themeColors.text
                    placeholderTextColor: themeColors.muted
                    selectionColor: themeColors.selected
                    selectedTextColor: themeColors.selectedText
                    font.pixelSize: 13
                    Accessible.name: qsTr("Search kernels")
                    onTextChanged: window.selectFirstVisibleKernel(true)

                    background: Rectangle {
                        color: themeColors.view
                        border.color: kernelSearch.activeFocus ? themeColors.selected : themeColors.border
                        border.width: kernelSearch.activeFocus ? 2 : 1
                        radius: 3
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: themeColors.view
                border.color: themeColors.border
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 40
                        color: themeColors.surface
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: themeColors.border }

                        Item {
                            id: headerColumns
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10

                            readonly property bool showType: window.width >= 980
                            readonly property bool showSize: window.width >= 900
                            readonly property bool showInstalled: window.width >= 1080
                            readonly property real statusWidth: 112
                            readonly property real gap: 12
                            readonly property real versionWidth: 150
                            readonly property real typeWidth: 118
                            readonly property real sizeWidth: 76
                            readonly property real installedWidth: 112
                            readonly property real trailingWidth: versionWidth
                                + (showType ? gap + typeWidth : 0)
                                + (showSize ? gap + sizeWidth : 0)
                                + (showInstalled ? gap + installedWidth : 0)
                            readonly property real versionX: width - trailingWidth
                            readonly property real kernelX: statusWidth + gap
                            readonly property real kernelWidth: Math.max(80, versionX - gap - kernelX)

                            Text { x: 0; width: headerColumns.statusWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Status"); color: themeColors.muted; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { x: headerColumns.kernelX; width: headerColumns.kernelWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Kernel"); color: themeColors.muted; font.pixelSize: 12; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignLeft; elide: Text.ElideRight }
                            Text { x: headerColumns.versionX; width: headerColumns.versionWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Version"); color: themeColors.muted; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { visible: headerColumns.showType; x: headerColumns.versionX + headerColumns.versionWidth + headerColumns.gap; width: headerColumns.typeWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Type"); color: themeColors.muted; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { visible: headerColumns.showSize; x: headerColumns.versionX + headerColumns.versionWidth + headerColumns.gap + (headerColumns.showType ? headerColumns.typeWidth + headerColumns.gap : 0); width: headerColumns.sizeWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Size"); color: themeColors.muted; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { visible: headerColumns.showInstalled; x: headerColumns.versionX + headerColumns.versionWidth + headerColumns.gap + (headerColumns.showType ? headerColumns.typeWidth + headerColumns.gap : 0) + (headerColumns.showSize ? headerColumns.sizeWidth + headerColumns.gap : 0); width: headerColumns.installedWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Installed"); color: themeColors.muted; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        }
                    }

                    Item {
                        Layout.fillWidth: true
                        Layout.fillHeight: true

                        ListView {
                            id: kernelList
                            anchors.fill: parent
                            clip: true
                            boundsBehavior: Flickable.StopAtBounds
                            model: window.filteredKernels
                            focus: true
                            keyNavigationEnabled: true
                            highlightMoveDuration: 0
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                            onCurrentIndexChanged: {
                                if (currentIndex >= 0 && currentIndex < window.filteredKernels.length)
                                    window.selectedKernelName = window.filteredKernels[currentIndex].name
                            }

                            delegate: ItemDelegate {
                                id: kernelRow
                                width: kernelList.width
                                height: 52
                                hoverEnabled: true
                                focusPolicy: Qt.StrongFocus
                                Accessible.name: qsTr("Kernel %1, version %2").arg(modelData.name).arg(modelData.version)

                                readonly property bool running: window.kernelIsRunning(modelData)
                                readonly property bool defaultKernel: window.kernelIsDefault(modelData)
                                readonly property bool selected: window.selectedKernelName === modelData.name
                                readonly property string stateLabel: running ? qsTr("Running")
                                                                     : defaultKernel ? qsTr("Default")
                                                                     : modelData.installed ? qsTr("Installed")
                                                                                           : qsTr("Available")

                                onClicked: {
                                    window.selectedKernelName = modelData.name
                                    kernelList.currentIndex = index
                                }
                                background: Rectangle {
                                    color: kernelRow.selected ? themeColors.selected
                                         : kernelRow.hovered ? themeColors.hover
                                         : index % 2 === 0 ? themeColors.view : themeColors.surface
                                    border.color: kernelRow.activeFocus ? themeColors.selected : "transparent"
                                    border.width: kernelRow.activeFocus ? 2 : 0
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: parent.width
                                        height: 1
                                        color: kernelRow.selected ? Qt.rgba(1, 1, 1, 0.22) : themeColors.border
                                    }
                                }

                                leftPadding: 10
                                rightPadding: 10
                                topPadding: 0
                                bottomPadding: 0

                                contentItem: Item {
                                    id: rowColumns
                                    implicitHeight: 52

                                    readonly property bool showType: window.width >= 980
                                    readonly property bool showSize: window.width >= 900
                                    readonly property bool showInstalled: window.width >= 1080
                                    readonly property real statusWidth: 112
                                    readonly property real gap: 12
                                    readonly property real versionWidth: 150
                                    readonly property real typeWidth: 118
                                    readonly property real sizeWidth: 76
                                    readonly property real installedWidth: 112
                                    readonly property real trailingWidth: versionWidth
                                        + (showType ? gap + typeWidth : 0)
                                        + (showSize ? gap + sizeWidth : 0)
                                        + (showInstalled ? gap + installedWidth : 0)
                                    readonly property real versionX: width - trailingWidth
                                    readonly property real kernelX: statusWidth + gap
                                    readonly property real kernelWidth: Math.max(80, versionX - gap - kernelX)

                                    Item {
                                        x: 0
                                        width: rowColumns.statusWidth
                                        height: parent.height
                                        Rectangle {
                                            id: statusDot
                                            anchors.left: parent.left
                                            anchors.verticalCenter: parent.verticalCenter
                                            width: 9
                                            height: 9
                                            radius: 5
                                            color: kernelRow.running ? themeColors.warning
                                                 : kernelRow.defaultKernel ? themeColors.success
                                                 : modelData.installed ? themeColors.selected : themeColors.faint
                                            border.color: kernelRow.selected ? themeColors.selectedText : themeColors.strongBorder
                                            border.width: modelData.installed ? 0 : 1
                                        }
                                        Text {
                                            anchors.left: statusDot.right
                                            anchors.leftMargin: 7
                                            anchors.right: parent.right
                                            anchors.verticalCenter: parent.verticalCenter
                                            text: kernelRow.stateLabel
                                            color: kernelRow.selected ? themeColors.selectedText : themeColors.text
                                            font.pixelSize: 12
                                            font.weight: (kernelRow.running || kernelRow.defaultKernel) ? Font.DemiBold : Font.Normal
                                            elide: Text.ElideRight
                                        }
                                    }
                                    Text {
                                        x: rowColumns.kernelX
                                        width: rowColumns.kernelWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.name || qsTr("Unnamed kernel")
                                        color: kernelRow.selected ? themeColors.selectedText : themeColors.text
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        horizontalAlignment: Text.AlignLeft
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        x: rowColumns.versionX
                                        width: rowColumns.versionWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.version || "—"
                                        color: kernelRow.selected ? themeColors.selectedText : themeColors.text
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        visible: rowColumns.showType
                                        x: rowColumns.versionX + rowColumns.versionWidth + rowColumns.gap
                                        width: rowColumns.typeWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.category || modelData.type || "—"
                                        color: kernelRow.selected ? themeColors.selectedText : themeColors.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        visible: rowColumns.showSize
                                        x: rowColumns.versionX + rowColumns.versionWidth + rowColumns.gap + (rowColumns.showType ? rowColumns.typeWidth + rowColumns.gap : 0)
                                        width: rowColumns.sizeWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.size || "—"
                                        color: kernelRow.selected ? themeColors.selectedText : themeColors.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        visible: rowColumns.showInstalled
                                        x: rowColumns.versionX + rowColumns.versionWidth + rowColumns.gap + (rowColumns.showType ? rowColumns.typeWidth + rowColumns.gap : 0) + (rowColumns.showSize ? rowColumns.sizeWidth + rowColumns.gap : 0)
                                        width: rowColumns.installedWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.installDate || (modelData.installed ? qsTr("Installed") : "—")
                                        color: kernelRow.selected ? themeColors.selectedText : themeColors.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(430, parent.width - 48)
                            spacing: 9
                            visible: kernelList.count === 0

                            BusyIndicator {
                                Layout.alignment: Qt.AlignHCenter
                                visible: bridge.busy
                                running: visible
                            }
                            Text {
                                Layout.fillWidth: true
                                text: bridge.busy ? qsTr("Loading kernels…")
                                      : bridge.statusIsError ? qsTr("Kernels could not be loaded")
                                      : kernelSearch.text.trim().length > 0 ? qsTr("No matching kernels")
                                      : window.installedOnly ? qsTr("No installed kernels are visible")
                                                             : qsTr("No kernels were found")
                                color: themeColors.text
                                font.pixelSize: 16
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                Layout.fillWidth: true
                                text: bridge.busy ? qsTr("Querying installed packages and Void repositories.")
                                      : bridge.statusIsError ? (bridge.statusMessage || qsTr("Open the logs for details, then try again."))
                                      : kernelSearch.text.trim().length > 0 ? qsTr("Clear the search or try a shorter version number.")
                                                                          : qsTr("Refresh the list to scan the system again.")
                                color: themeColors.muted
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                            FlatButton {
                                Layout.alignment: Qt.AlignHCenter
                                visible: !bridge.busy
                                text: kernelSearch.text.trim().length > 0 ? qsTr("Clear Search") : qsTr("Retry")
                                primary: bridge.statusIsError
                                onClicked: {
                                    if (kernelSearch.text.trim().length > 0)
                                        kernelSearch.clear()
                                    else
                                        window.refreshKernels()
                                }
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 190
                color: themeColors.surface
                border.color: themeColors.border
                border.width: 1

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    TabBar {
                        id: detailTabs
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        background: Rectangle {
                            color: themeColors.surface
                            Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: themeColors.border }
                        }

                        TabButton {
                            id: detailsTab
                            text: qsTr("Details")
                            width: 100
                            contentItem: Text { text: detailsTab.text; color: detailsTab.checked ? themeColors.text : themeColors.muted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle {
                                color: detailsTab.hovered ? themeColors.hover : "transparent"
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: detailsTab.checked ? 2 : 0; color: themeColors.selected }
                            }
                        }
                        TabButton {
                            id: systemTab
                            text: qsTr("System")
                            width: 100
                            contentItem: Text { text: systemTab.text; color: systemTab.checked ? themeColors.text : themeColors.muted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle {
                                color: systemTab.hovered ? themeColors.hover : "transparent"
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: systemTab.checked ? 2 : 0; color: themeColors.selected }
                            }
                        }
                        TabButton {
                            id: activityTab
                            text: qsTr("Activity")
                            width: 100
                            contentItem: Text { text: activityTab.text; color: activityTab.checked ? themeColors.text : themeColors.muted; font.pixelSize: 12; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle {
                                color: activityTab.hovered ? themeColors.hover : "transparent"
                                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: activityTab.checked ? 2 : 0; color: themeColors.selected }
                            }
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: detailTabs.currentIndex

                        Item {
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 18

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Text {
                                        Layout.fillWidth: true
                                        text: window.selectedKernel ? window.selectedKernel.name : qsTr("Select a kernel")
                                        color: themeColors.text
                                        font.pixelSize: 16
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: window.selectedKernel
                                              ? qsTr("Version %1 · %2 · %3")
                                                .arg(window.selectedKernel.version || "—")
                                                .arg(window.selectedKernel.category || window.selectedKernel.type || qsTr("Unclassified"))
                                                .arg(window.selectedKernel.size || qsTr("Unknown size"))
                                              : qsTr("Choose a row above to inspect its package and boot status.")
                                        color: themeColors.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        visible: window.selectedKernel !== null
                                        text: window.kernelIsRunning(window.selectedKernel)
                                              ? qsTr("This kernel is currently running and cannot be removed.")
                                              : window.kernelIsDefault(window.selectedKernel)
                                                ? qsTr("This kernel is selected as the default boot entry.")
                                                : window.selectedKernel && window.selectedKernel.installed
                                                  ? qsTr("Installed on %1").arg(window.selectedKernel.installDate || qsTr("an unknown date"))
                                                  : qsTr("Available from the configured Void repositories.")
                                        color: window.kernelIsRunning(window.selectedKernel) ? themeColors.warning : themeColors.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }

                                RowLayout {
                                    spacing: 8
                                    FlatButton {
                                        visible: window.selectedKernel && !window.selectedKernel.installed
                                        enabled: visible && !bridge.busy
                                        primary: true
                                        text: qsTr("Install")
                                        onClicked: bridge.installKernel(window.selectedKernel.name)
                                    }
                                    FlatButton {
                                        visible: window.selectedKernel && window.selectedKernel.installed && !window.kernelIsDefault(window.selectedKernel)
                                        enabled: visible && !bridge.busy
                                        text: qsTr("Set as Default")
                                        onClicked: bridge.setDefaultKernel(window.selectedKernel.version)
                                    }
                                    FlatButton {
                                        visible: window.selectedKernel && window.selectedKernel.installed && !window.kernelIsRunning(window.selectedKernel)
                                        enabled: visible && !bridge.busy
                                        destructive: true
                                        text: qsTr("Remove")
                                        onClicked: {
                                            removeDialog.kernelData = window.selectedKernel
                                            removeDialog.open()
                                        }
                                    }
                                }
                            }
                        }

                        Item {
                            GridLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                columns: window.width >= 980 ? 4 : 2
                                columnSpacing: 22
                                rowSpacing: 8
                                Text { text: qsTr("Running kernel"); color: themeColors.muted; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true; text: bridge.activeKernelVersion || qsTr("Detecting…"); color: themeColors.text; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { text: qsTr("Default kernel"); color: themeColors.muted; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true; text: bridge.defaultKernel || qsTr("Not detected"); color: themeColors.text; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text { text: qsTr("Bootloader"); color: themeColors.muted; font.pixelSize: 12 }
                                Text { text: bridge.detectedBootloader || "—"; color: themeColors.text; font.pixelSize: 13 }
                                Text { text: qsTr("CPU level"); color: themeColors.muted; font.pixelSize: 12 }
                                Text { text: bridge.detectedCpuLevel || "—"; color: themeColors.text; font.pixelSize: 13 }
                            }
                        }

                        Item {
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        Layout.fillWidth: true
                                        text: bridge.statusMessage || qsTr("No operation has been reported yet.")
                                        color: bridge.statusIsError ? themeColors.error : themeColors.text
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    FlatButton { text: qsTr("Open Full Log"); onClicked: logDialog.open() }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: themeColors.view
                                    border.color: themeColors.border
                                    border.width: 1
                                    Text {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        text: bridge.logs.length > 0 ? bridge.logs : qsTr("The activity log is empty.")
                                        color: themeColors.muted
                                        font.family: "0xProto Nerd Font Mono"
                                        font.pixelSize: 11
                                        textFormat: Text.PlainText
                                        wrapMode: Text.WrapAnywhere
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: bridge.busy || bridge.progress > 0 ? 38 : 32
            color: themeColors.toolbar
            Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: themeColors.border }
            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                height: 3
                width: parent.width * Math.max(0, Math.min(100, bridge.progress)) / 100
                visible: bridge.busy || bridge.progress > 0
                color: themeColors.selected
                Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            }
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 10
                anchors.rightMargin: 10
                anchors.topMargin: bridge.busy || bridge.progress > 0 ? 4 : 0
                spacing: 8
                Text { text: bridge.busy ? qsTr("Working…") : qsTr("Ready"); color: bridge.busy ? themeColors.selected : themeColors.muted; font.pixelSize: 12; font.weight: Font.DemiBold }
                Text { Layout.fillWidth: true; text: qsTr("%1 kernels shown").arg(window.filteredKernels.length); color: themeColors.muted; font.pixelSize: 12 }
                Text { text: qsTr("Neko Kernel Manager %1").arg(Qt.application.version); color: themeColors.muted; font.pixelSize: 11 }
            }
        }
    }

    Dialog {
        id: removeDialog
        property var kernelData: null
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(450, window.width - 48)
        modal: true
        focus: true
        title: qsTr("Remove kernel")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: {
            if (kernelData)
                bridge.removeKernel(kernelData.name, kernelData.type || "", kernelData.version || "")
        }
        Text {
            width: parent.width
            text: removeDialog.kernelData
                  ? qsTr("Remove %1 (%2)? The running kernel is protected, but this action cannot be undone from this screen.")
                    .arg(removeDialog.kernelData.name).arg(removeDialog.kernelData.version)
                  : ""
            color: themeColors.text
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        background: Rectangle { color: themeColors.popover; border.color: themeColors.border; border.width: 1; radius: 4 }
    }

    Dialog {
        id: purgeDialog
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(450, window.width - 48)
        modal: true
        focus: true
        title: qsTr("Remove old kernels")
        standardButtons: Dialog.Cancel | Dialog.Ok
        onAccepted: bridge.vkpurge()
        Text {
            width: parent.width
            text: qsTr("Neko will run vkpurge to remove obsolete kernel versions and reclaim disk space. The running kernel will not be removed.")
            color: themeColors.text
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
        background: Rectangle { color: themeColors.popover; border.color: themeColors.border; border.width: 1; radius: 4 }
    }

    Dialog {
        id: logDialog
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(760, window.width - 48)
        height: Math.min(500, window.height - 48)
        modal: true
        focus: true
        title: qsTr("System Logs and Operations")
        standardButtons: Dialog.Close

        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                FlatButton { text: qsTr("Export…"); enabled: bridge.logs.length > 0; onClicked: fileDialog.open() }
                FlatButton { text: qsTr("Clear"); enabled: bridge.logs.length > 0; onClicked: bridge.clearLogs() }
            }
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: themeColors.view
                border.color: themeColors.border
                border.width: 1
                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 7
                    clip: true
                    TextArea {
                        text: bridge.logs.length > 0 ? bridge.logs : qsTr("No log entries yet.")
                        readOnly: true
                        color: themeColors.text
                        selectionColor: themeColors.selected
                        selectedTextColor: themeColors.selectedText
                        font.family: "0xProto Nerd Font Mono"
                        font.pixelSize: 12
                        textFormat: TextEdit.RichText
                        wrapMode: TextEdit.Wrap
                        background: null
                    }
                }
            }
        }
        background: Rectangle { color: themeColors.popover; border.color: themeColors.border; border.width: 1; radius: 4 }
    }

    Dialog {
        id: dkmsDialog
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(780, window.width - 48)
        height: Math.min(540, window.height - 48)
        modal: true
        focus: true
        title: qsTr("DKMS Module Management")
        standardButtons: Dialog.Close

        ColumnLayout {
            anchors.fill: parent
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Install or remove out-of-tree modules for the kernels on this system.")
                    color: themeColors.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                FlatButton { text: qsTr("Refresh"); enabled: !bridge.busy; onClicked: bridge.updateDkmsModules() }
                FlatButton { text: qsTr("Autoinstall"); enabled: !bridge.busy; primary: true; onClicked: bridge.autoinstallDkms() }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: themeColors.view
                border.color: themeColors.border
                border.width: 1

                ListView {
                    id: dkmsList
                    anchors.fill: parent
                    clip: true
                    model: bridge.dkmsModules
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        width: dkmsList.width
                        height: 58
                        color: index % 2 === 0 ? themeColors.view : themeColors.surface
                        Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: themeColors.border }
                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            anchors.rightMargin: 10
                            spacing: 12
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2
                                Text { Layout.fillWidth: true; text: modelData.name + " " + modelData.version; color: themeColors.text; font.pixelSize: 13; font.weight: Font.DemiBold; elide: Text.ElideRight }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.isDkmsPackage
                                          ? (modelData.packageInstalled ? qsTr("XBPS package installed; not registered in DKMS") : qsTr("Available from the XBPS repository"))
                                          : qsTr("Kernel %1 · %2").arg(modelData.kernel || "—").arg(modelData.arch || "—")
                                    color: themeColors.muted
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                            Text {
                                Layout.preferredWidth: 90
                                text: modelData.status || qsTr("unknown")
                                color: modelData.status === "installed" ? themeColors.success
                                     : modelData.status === "unregistered" ? themeColors.warning : themeColors.muted
                                font.pixelSize: 12
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }
                            FlatButton {
                                readonly property bool removable: modelData.status === "installed" || modelData.status === "unregistered"
                                text: removable ? qsTr("Remove") : qsTr("Install")
                                destructive: removable
                                primary: !removable
                                enabled: !bridge.busy
                                onClicked: {
                                    if (removable)
                                        bridge.removeDkmsModule(modelData.name, modelData.version, modelData.kernel)
                                    else
                                        bridge.installDkmsModule(modelData.name, modelData.version, modelData.kernel)
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    visible: dkmsList.count === 0
                    spacing: 8
                    BusyIndicator { Layout.alignment: Qt.AlignHCenter; visible: bridge.busy; running: visible }
                    Text { text: bridge.busy ? qsTr("Loading DKMS modules…") : qsTr("No DKMS modules were found"); color: themeColors.text; font.pixelSize: 13 }
                }
            }
        }
        background: Rectangle { color: themeColors.popover; border.color: themeColors.border; border.width: 1; radius: 4 }
    }

    Dialog {
        id: aboutDialog
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(430, window.width - 48)
        modal: true
        focus: true
        title: qsTr("About Neko Kernel Manager")
        standardButtons: Dialog.Close

        ColumnLayout {
            width: parent.width
            spacing: 10
            Text { Layout.fillWidth: true; text: qsTr("Neko Kernel Manager"); color: themeColors.text; font.pixelSize: 18; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignHCenter }
            Text {
                Layout.fillWidth: true
                text: qsTr("A graphical manager for installing, removing, and selecting Linux kernels and DKMS modules on Void Linux.")
                color: themeColors.muted
                font.pixelSize: 13
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }
            Text { Layout.fillWidth: true; text: qsTr("Version %1 · GPL-3.0-only · Qt 6 / QML / C++").arg(Qt.application.version); color: themeColors.muted; font.pixelSize: 11; horizontalAlignment: Text.AlignHCenter }
        }
        background: Rectangle { color: themeColors.popover; border.color: themeColors.border; border.width: 1; radius: 4 }
    }

    Platform.FileDialog {
        id: fileDialog
        title: qsTr("Save Logs")
        fileMode: Platform.FileDialog.SaveFile
        folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        nameFilters: [qsTr("Text files (*.txt)"), qsTr("All files (*)")]
        defaultSuffix: "txt"
        onAccepted: {
            if (fileDialog.file.toString().length > 0) {
                var filePath = fileDialog.file.toString().replace("file://", "")
                bridge.exportLogs(filePath)
            }
        }
    }

    Connections {
        target: bridge
        function onKernelsChanged() { window.selectFirstVisibleKernel(true) }
    }

    Component.onCompleted: {
        bridge.updateKernels()
        bridge.updateDkmsModules()
        bridge.updateDefaultKernel()
    }
}
