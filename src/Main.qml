import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1 as Platform
import QtCore
import "ui"

ApplicationWindow {
    id: window

    visible: true
    width: 1180
    height: 780
    minimumWidth: 860
    minimumHeight: 620
    title: qsTr("Neko Kernel Manager")
    color: ThemePalette.window

    property string selectedKernelName: ""
    property bool installedOnly: false

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
                    || (kernel.repo || "").toLowerCase().indexOf(query) !== -1
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
            if (kernels[index].installed && kernels[index].hasFiles && kernels[index].type !== "vkpurge")
                ++count
        }
        return count
    }

    function versionsMatch(a, b) {
        if (!a || !b) return false
        if (a === b) return true
        function checkPrefix(full, sub) {
            if (!full.startsWith(sub)) return false
            if (full.length === sub.length) return true
            var next = full.charAt(sub.length)
            return !/[0-9a-zA-Z]/.test(next)
        }
        return checkPrefix(a, b) || checkPrefix(b, a)
    }

    function kernelIsRunning(kernel) {
        if (!kernel || !kernel.installed || !bridge.activeKernelVersion)
            return false
        var active = bridge.activeKernelVersion.trim()
        if (versionsMatch(active, kernel.version))
            return true
        var cleanName = (kernel.name || "").replace(/^(linux-?|kernel-)/, "")
        if (cleanName.length > 0 && versionsMatch(active, cleanName))
            return true
        return false
    }

    function kernelIsDefault(kernel) {
        if (!kernel || !kernel.installed || !bridge.defaultKernel)
            return false
        var def = bridge.defaultKernel.trim()
        if (def === kernel.name || versionsMatch(def, kernel.version))
            return true
        if (def.indexOf(kernel.version) !== -1 || def.indexOf(kernel.name) !== -1)
            return true
        return false
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

    Shortcut {
        sequence: "Ctrl+R"
        enabled: !bridge.busy
        onActivated: window.refreshKernels()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ====================================================================
        // GTK3 HEADERBAR (Matecito Dark)
        // ====================================================================
        Rectangle {
            id: headerBar
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: ThemePalette.toolbar

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: ThemePalette.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // Left buttons
                NekoHeaderButton {
                    symbol: "\uf021"
                    text: qsTr("Refresh")
                    enabled: !bridge.busy
                    onClicked: window.refreshKernels()
                }

                NekoHeaderButton {
                    symbol: "\uf058"
                    text: qsTr("Installed")
                    checkable: true
                    checked: window.installedOnly
                    onToggled: {
                        window.installedOnly = checked
                        window.selectFirstVisibleKernel(true)
                    }
                }

                Item { Layout.fillWidth: true }

                // Right buttons
                NekoHeaderButton {
                    symbol: "\uf023"
                    text: qsTr("Unlock")
                    visible: !bridge.authenticated
                    enabled: !bridge.busy
                    onClicked: bridge.startPrivilegedSession()
                }

                NekoHeaderButton {
                    symbol: "\uf1b3"
                    text: qsTr("DKMS")
                    enabled: !bridge.busy
                    onClicked: {
                        bridge.updateDkmsModules()
                        dkmsDialog.open()
                    }
                }

                NekoHeaderButton {
                    symbol: "\uf1f8"
                    text: qsTr("Remove Old")
                    enabled: !bridge.busy
                    onClicked: purgeDialog.open()
                }

                NekoHeaderButton {
                    symbol: "\uf15c"
                    text: qsTr("Logs")
                    onClicked: logDialog.open()
                }

                NekoHeaderButton {
                    symbol: "\uf05a"
                    text: qsTr("About")
                    onClicked: aboutDialog.open()
                }
            }
        }

        // ====================================================================
        // GTK3 INFOBAR (Status & Active Kernel Banner)
        // ====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            color: ThemePalette.surface

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: ThemePalette.border
            }

            // Left vertical accent stripe
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                color: bridge.statusIsError ? ThemePalette.error : ThemePalette.selected
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 14
                spacing: 12

                Text {
                    text: bridge.statusIsError ? "\uf071" : "\uf058"
                    color: bridge.statusIsError ? ThemePalette.error : ThemePalette.selected
                    font.family: ThemePalette.fontIcon
                    font.pixelSize: 18
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 1

                    Text {
                        Layout.fillWidth: true
                        text: bridge.statusIsError
                              ? qsTr("The last kernel operation reported an error")
                              : qsTr("Running kernel: %1").arg(bridge.activeKernelVersion || qsTr("Detecting…"))
                        color: ThemePalette.text
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: bridge.statusMessage !== ""
                              ? bridge.statusMessage
                              : qsTr("Select a kernel below to inspect its details and actions.")
                        color: ThemePalette.muted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                NekoButton {
                    visible: bridge.statusIsError
                    text: qsTr("View Logs")
                    destructive: true
                    onClicked: logDialog.open()
                }
            }
        }

        // ====================================================================
        // MAIN WORKSPACE (Search + Kernel List + Details)
        // ====================================================================
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 12
            Layout.rightMargin: 12
            Layout.topMargin: 10
            Layout.bottomMargin: 10
            spacing: 10

            // Search & Filter header
            RowLayout {
                Layout.fillWidth: true
                spacing: 12

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 2
                    Text {
                        text: qsTr("Kernels")
                        color: ThemePalette.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: qsTr("%1 kernels available · %2 installed")
                              .arg(bridge.kernels.length).arg(window.installedKernelCount)
                        color: ThemePalette.muted
                        font.pixelSize: 11
                    }
                }

                // GTK3 SearchEntry
                Rectangle {
                    Layout.preferredWidth: Math.min(320, window.width * 0.32)
                    Layout.minimumWidth: 200
                    implicitHeight: 34
                    color: ThemePalette.inputBg
                    border.color: kernelSearch.activeFocus ? ThemePalette.selected : ThemePalette.inputBorder
                    border.width: kernelSearch.activeFocus ? 1.5 : 1
                    radius: ThemePalette.cornerRadius

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: "\uf002"
                            font.family: ThemePalette.fontIcon
                            font.pixelSize: 13
                            color: ThemePalette.muted
                        }

                        TextInput {
                            id: kernelSearch
                            Layout.fillWidth: true
                            color: ThemePalette.text
                            font.pixelSize: 12
                            clip: true
                            selectByMouse: true
                            selectionColor: ThemePalette.selected
                            selectedTextColor: ThemePalette.selectedText
                            verticalAlignment: Text.AlignVCenter
                            onTextChanged: window.selectFirstVisibleKernel(true)

                            Text {
                                anchors.fill: parent
                                text: qsTr("Search by name or version…")
                                color: ThemePalette.faint
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                visible: !kernelSearch.text && !kernelSearch.activeFocus
                            }
                        }

                        Text {
                            visible: kernelSearch.text.length > 0
                            text: "\uf00d"
                            font.family: ThemePalette.fontIcon
                            font.pixelSize: 12
                            color: ThemePalette.muted
                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: kernelSearch.clear()
                            }
                        }
                    }
                }
            }

            // GTK3 ListBox Frame
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: ThemePalette.view
                border.color: ThemePalette.border
                border.width: 1
                radius: ThemePalette.cornerRadius
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Column Headers
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        color: ThemePalette.surface
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: ThemePalette.border
                        }

                        Item {
                            id: headerColumns
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14

                            readonly property bool showType: window.width >= 960
                            readonly property bool showSize: window.width >= 880
                            readonly property bool showInstalled: window.width >= 1040
                            readonly property real statusWidth: 120
                            readonly property real gap: 12
                            readonly property real versionWidth: 150
                            readonly property real typeWidth: 110
                            readonly property real sizeWidth: 76
                            readonly property real installedWidth: 110
                            readonly property real trailingWidth: versionWidth
                                + (showType ? gap + typeWidth : 0)
                                + (showSize ? gap + sizeWidth : 0)
                                + (showInstalled ? gap + installedWidth : 0)
                            readonly property real versionX: width - trailingWidth
                            readonly property real kernelX: statusWidth + gap
                            readonly property real kernelWidth: Math.max(80, versionX - gap - kernelX)

                            Text { x: 0; width: headerColumns.statusWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Status"); color: ThemePalette.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { x: headerColumns.kernelX; width: headerColumns.kernelWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Kernel"); color: ThemePalette.muted; font.pixelSize: 11; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignLeft; elide: Text.ElideRight }
                            Text { x: headerColumns.versionX; width: headerColumns.versionWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Version"); color: ThemePalette.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { visible: headerColumns.showType; x: headerColumns.versionX + headerColumns.versionWidth + headerColumns.gap; width: headerColumns.typeWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Type"); color: ThemePalette.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { visible: headerColumns.showSize; x: headerColumns.versionX + headerColumns.versionWidth + headerColumns.gap + (headerColumns.showType ? headerColumns.typeWidth + headerColumns.gap : 0); width: headerColumns.sizeWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Size"); color: ThemePalette.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { visible: headerColumns.showInstalled; x: headerColumns.versionX + headerColumns.versionWidth + headerColumns.gap + (headerColumns.showType ? headerColumns.typeWidth + headerColumns.gap : 0) + (headerColumns.showSize ? headerColumns.sizeWidth + headerColumns.gap : 0); width: headerColumns.installedWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Installed"); color: ThemePalette.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                        }
                    }

                    // Kernel ListView
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
                            ScrollBar.vertical: NekoScrollBar {}
                            onCurrentIndexChanged: {
                                if (currentIndex >= 0 && currentIndex < window.filteredKernels.length)
                                    window.selectedKernelName = window.filteredKernels[currentIndex].name
                            }

                            delegate: ItemDelegate {
                                id: kernelRow
                                width: kernelList.width
                                height: 50
                                hoverEnabled: true
                                focusPolicy: Qt.StrongFocus
                                Accessible.name: qsTr("Kernel %1, version %2").arg(modelData.name).arg(modelData.version)

                                readonly property bool running: window.kernelIsRunning(modelData)
                                readonly property bool defaultKernel: window.kernelIsDefault(modelData)
                                readonly property bool isSelected: window.selectedKernelName === modelData.name

                                onClicked: {
                                    window.selectedKernelName = modelData.name
                                    kernelList.currentIndex = index
                                }

                                background: Rectangle {
                                    color: kernelRow.isSelected ? ThemePalette.selected
                                         : kernelRow.hovered ? ThemePalette.hover
                                         : (index % 2 === 0 ? "transparent" : ThemePalette.alternateRow)
                                    border.color: kernelRow.activeFocus ? ThemePalette.selected : "transparent"
                                    border.width: kernelRow.activeFocus ? 1.5 : 0

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: parent.width
                                        height: 1
                                        color: kernelRow.isSelected ? ThemePalette.selectedRowDivider : ThemePalette.border
                                    }
                                }

                                leftPadding: 14
                                rightPadding: 14
                                topPadding: 0
                                bottomPadding: 0

                                contentItem: Item {
                                    id: rowColumns
                                    implicitHeight: 50

                                    readonly property bool showType: window.width >= 960
                                    readonly property bool showSize: window.width >= 880
                                    readonly property bool showInstalled: window.width >= 1040
                                    readonly property real statusWidth: 120
                                    readonly property real gap: 12
                                    readonly property real versionWidth: 150
                                    readonly property real typeWidth: 110
                                    readonly property real sizeWidth: 76
                                    readonly property real installedWidth: 110
                                    readonly property real trailingWidth: versionWidth
                                        + (showType ? gap + typeWidth : 0)
                                        + (showSize ? gap + sizeWidth : 0)
                                        + (showInstalled ? gap + installedWidth : 0)
                                    readonly property real versionX: width - trailingWidth
                                    readonly property real kernelX: statusWidth + gap
                                    readonly property real kernelWidth: Math.max(80, versionX - gap - kernelX)

                                    // Status Badge (GTK3 Pill Style)
                                    RowLayout {
                                        x: 0
                                        width: rowColumns.statusWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        spacing: 6

                                        Rectangle {
                                            id: statusPill
                                            readonly property bool isOrphan: modelData.type === "vkpurge"
                                            readonly property bool isManual: modelData.type === "manual"
                                            readonly property bool isNoFiles: modelData.installed && !modelData.hasFiles && !isOrphan
                                            implicitHeight: 22
                                            implicitWidth: statusText.implicitWidth + 16
                                            radius: 11
                                            color: kernelRow.isSelected ? ThemePalette.selectedRowPillBg
                                                 : kernelRow.running ? ThemePalette.warningSubtle
                                                 : kernelRow.defaultKernel ? ThemePalette.selectedPillBg
                                                 : isOrphan ? ThemePalette.errorSubtle
                                                 : modelData.installed ? ThemePalette.installedSubtle
                                                 : "transparent"
                                            border.color: kernelRow.isSelected ? ThemePalette.selectedRowBorder
                                                        : kernelRow.running ? ThemePalette.warning
                                                        : kernelRow.defaultKernel ? ThemePalette.selected
                                                        : isOrphan ? ThemePalette.error
                                                        : modelData.installed ? ThemePalette.strongBorder
                                                        : "transparent"
                                            border.width: 1

                                            RowLayout {
                                                anchors.centerIn: parent
                                                spacing: 5
                                                Rectangle {
                                                    width: 6
                                                    height: 6
                                                    radius: 3
                                                    color: kernelRow.isSelected ? ThemePalette.selectedText
                                                         : kernelRow.running ? ThemePalette.warning
                                                         : kernelRow.defaultKernel ? ThemePalette.selected
                                                         : statusPill.isOrphan ? ThemePalette.error
                                                         : statusPill.isNoFiles ? ThemePalette.muted
                                                         : modelData.installed ? ThemePalette.text
                                                         : ThemePalette.faint
                                                }
                                                Text {
                                                    id: statusText
                                                    text: kernelRow.running ? qsTr("Running")
                                                         : kernelRow.defaultKernel ? qsTr("Default")
                                                         : statusPill.isOrphan ? qsTr("Orphan")
                                                         : statusPill.isManual ? qsTr("Manual")
                                                         : statusPill.isNoFiles ? qsTr("Meta")
                                                         : modelData.installed ? qsTr("Installed")
                                                         : qsTr("Available")
                                                    color: kernelRow.isSelected ? ThemePalette.selectedText
                                                         : kernelRow.running ? ThemePalette.warning
                                                         : kernelRow.defaultKernel ? ThemePalette.selected
                                                         : statusPill.isOrphan ? ThemePalette.error
                                                         : statusPill.isNoFiles ? ThemePalette.muted
                                                         : modelData.installed ? ThemePalette.text
                                                         : ThemePalette.muted
                                                    font.pixelSize: 11
                                                    font.weight: (kernelRow.running || kernelRow.defaultKernel || statusPill.isOrphan) ? Font.DemiBold : Font.Normal
                                                }
                                            }
                                        }
                                    }

                                    // Kernel Name
                                    Text {
                                        x: rowColumns.kernelX
                                        width: rowColumns.kernelWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.name || qsTr("Unnamed kernel")
                                        color: kernelRow.isSelected ? ThemePalette.selectedText : ThemePalette.text
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        horizontalAlignment: Text.AlignLeft
                                        elide: Text.ElideRight
                                    }

                                    // Version
                                    Text {
                                        x: rowColumns.versionX
                                        width: rowColumns.versionWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.version || "—"
                                        color: kernelRow.isSelected ? ThemePalette.selectedText : ThemePalette.text
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }

                                    // Type / Category
                                    Text {
                                        visible: rowColumns.showType
                                        x: rowColumns.versionX + rowColumns.versionWidth + rowColumns.gap
                                        width: rowColumns.typeWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.category || modelData.type || "—"
                                        color: kernelRow.isSelected ? ThemePalette.selectedText : ThemePalette.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }

                                    // Size
                                    Text {
                                        visible: rowColumns.showSize
                                        x: rowColumns.versionX + rowColumns.versionWidth + rowColumns.gap + (rowColumns.showType ? rowColumns.typeWidth + rowColumns.gap : 0)
                                        width: rowColumns.sizeWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.size || "—"
                                        color: kernelRow.isSelected ? ThemePalette.selectedText : ThemePalette.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }

                                    // Installed Date
                                    Text {
                                        visible: rowColumns.showInstalled
                                        x: rowColumns.versionX + rowColumns.versionWidth + rowColumns.gap + (rowColumns.showType ? rowColumns.typeWidth + rowColumns.gap : 0) + (rowColumns.showSize ? rowColumns.sizeWidth + rowColumns.gap : 0)
                                        width: rowColumns.installedWidth
                                        anchors.verticalCenter: parent.verticalCenter
                                        text: modelData.installDate || (modelData.installed ? qsTr("Installed") : "—")
                                        color: kernelRow.isSelected ? ThemePalette.selectedText : ThemePalette.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }
                            }
                        }

                        // Empty State Placeholder
                        ColumnLayout {
                            anchors.centerIn: parent
                            width: Math.min(430, parent.width - 48)
                            spacing: 10
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
                                      : window.installedOnly ? qsTr("No installed kernels found")
                                      : qsTr("No kernels were found")
                                color: ThemePalette.text
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                            Text {
                                Layout.fillWidth: true
                                text: bridge.busy ? qsTr("Querying installed packages and Void repositories.")
                                      : bridge.statusIsError ? (bridge.statusMessage || qsTr("Open the activity log for details."))
                                      : kernelSearch.text.trim().length > 0 ? qsTr("Clear the search or try a different term.")
                                                                           : qsTr("Refresh the list to scan the system again.")
                                color: ThemePalette.muted
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                            NekoButton {
                                Layout.alignment: Qt.AlignHCenter
                                visible: !bridge.busy
                                text: kernelSearch.text.trim().length > 0 ? qsTr("Clear Search") : qsTr("Retry")
                                suggested: bridge.statusIsError
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

            // ================================================================
            // GTK3 NOTEBOOK (Detail & Action Panel)
            // ================================================================
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 185
                color: ThemePalette.surface
                border.color: ThemePalette.border
                border.width: 1
                radius: ThemePalette.cornerRadius

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // GTK3 Tabs
                    TabBar {
                        id: detailTabs
                        objectName: "detailTabs"
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        background: Rectangle {
                            color: ThemePalette.surface
                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: ThemePalette.border
                            }
                        }

                        TabButton {
                            id: detailsTab
                            text: qsTr("Details")
                            width: 105
                            contentItem: Text {
                                text: detailsTab.text
                                color: detailsTab.checked ? ThemePalette.text : ThemePalette.muted
                                font.pixelSize: 12
                                font.weight: detailsTab.checked ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: detailsTab.hovered ? ThemePalette.hover : "transparent"
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: detailsTab.checked ? 2.5 : 0
                                    color: ThemePalette.selected
                                }
                            }
                        }

                        TabButton {
                            id: systemTab
                            text: qsTr("System")
                            width: 105
                            contentItem: Text {
                                text: systemTab.text
                                color: systemTab.checked ? ThemePalette.text : ThemePalette.muted
                                font.pixelSize: 12
                                font.weight: systemTab.checked ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: systemTab.hovered ? ThemePalette.hover : "transparent"
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: systemTab.checked ? 2.5 : 0
                                    color: ThemePalette.selected
                                }
                            }
                        }

                        TabButton {
                            id: activityTab
                            text: qsTr("Activity")
                            width: 105
                            contentItem: Text {
                                text: activityTab.text
                                color: activityTab.checked ? ThemePalette.text : ThemePalette.muted
                                font.pixelSize: 12
                                font.weight: activityTab.checked ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: activityTab.hovered ? ThemePalette.hover : "transparent"
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: activityTab.checked ? 2.5 : 0
                                    color: ThemePalette.selected
                                }
                            }
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: detailTabs.currentIndex

                        // Page 1: Details & Actions
                        Item {
                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 18

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 5

                                    Text {
                                        Layout.fillWidth: true
                                        text: window.selectedKernel ? window.selectedKernel.name : qsTr("Select a kernel")
                                        color: ThemePalette.text
                                        font.pixelSize: 16
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        text: window.selectedKernel
                                              ? qsTr("Version %1 · %2 · %3 · %4")
                                                .arg(window.selectedKernel.version || "—")
                                                .arg(window.selectedKernel.category || window.selectedKernel.type || qsTr("Unclassified"))
                                                .arg(window.selectedKernel.size || qsTr("Unknown size"))
                                                .arg(window.selectedKernel.repo || qsTr("System"))
                                              : qsTr("Select a kernel from the list to manage it.")
                                        color: ThemePalette.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }

                                    Text {
                                        Layout.fillWidth: true
                                        visible: window.selectedKernel !== null
                                        text: window.kernelIsRunning(window.selectedKernel)
                                              ? qsTr("This kernel is currently running and cannot be removed.")
                                              : window.kernelIsDefault(window.selectedKernel)
                                                ? qsTr("This kernel is selected as default boot entry (Repo: %1)").arg(window.selectedKernel.repo || qsTr("System"))
                                                : window.selectedKernel && window.selectedKernel.type === "vkpurge"
                                                  ? qsTr("Obsolete kernel left behind after updates (tracked by vkpurge). Recommended to remove.")
                                                  : window.selectedKernel && window.selectedKernel.type === "manual"
                                                    ? qsTr("Locally compiled or manually installed kernel in /boot.")
                                                    : window.selectedKernel && window.selectedKernel.installed && !window.selectedKernel.hasFiles
                                                      ? qsTr("Metapackage without bootable kernel image in /boot.")
                                                      : window.selectedKernel && window.selectedKernel.installed
                                                        ? qsTr("Installed on %1 · Repo: %2").arg(window.selectedKernel.installDate || qsTr("an unknown date")).arg(window.selectedKernel.repo || qsTr("System"))
                                                        : qsTr("Available from repository: %1").arg(window.selectedKernel ? (window.selectedKernel.repo || qsTr("System")) : qsTr("Void repositories"))
                                        color: window.kernelIsRunning(window.selectedKernel) ? ThemePalette.warning
                                             : window.kernelIsDefault(window.selectedKernel) ? ThemePalette.selected
                                             : (window.selectedKernel && window.selectedKernel.type === "vkpurge") ? ThemePalette.error
                                             : ThemePalette.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }

                                RowLayout {
                                    spacing: 10

                                    NekoButton {
                                        visible: window.selectedKernel && !window.selectedKernel.installed
                                        enabled: visible && !bridge.busy
                                        suggested: true
                                        text: qsTr("Install")
                                        onClicked: bridge.installKernel(window.selectedKernel.name)
                                    }

                                    NekoButton {
                                        visible: window.selectedKernel && window.selectedKernel.installed && window.selectedKernel.hasFiles && window.selectedKernel.type !== "vkpurge" && !window.kernelIsDefault(window.selectedKernel)
                                        enabled: visible && !bridge.busy
                                        text: qsTr("Set as Default")
                                        onClicked: bridge.setDefaultKernel(window.selectedKernel.version)
                                    }

                                    NekoButton {
                                        visible: window.selectedKernel && window.selectedKernel.installed && window.selectedKernel.hasFiles && window.selectedKernel.type !== "vkpurge"
                                        enabled: visible && !bridge.busy
                                        text: qsTr("Reconfigure")
                                        onClicked: bridge.reconfigureKernel(window.selectedKernel.name, window.selectedKernel.type || "", window.selectedKernel.version || "")
                                    }

                                    NekoButton {
                                        visible: window.selectedKernel && window.selectedKernel.installed && !window.kernelIsRunning(window.selectedKernel)
                                        enabled: visible && !bridge.busy && (window.installedKernelCount > 1 || (window.selectedKernel && (window.selectedKernel.type === "vkpurge" || !window.selectedKernel.hasFiles)))
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

                        // Page 2: System Information
                        Item {
                            GridLayout {
                                anchors.fill: parent
                                anchors.margins: 14
                                columns: window.width >= 980 ? 4 : 2
                                columnSpacing: 22
                                rowSpacing: 10

                                Text { text: qsTr("Running kernel:"); color: ThemePalette.muted; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true; text: bridge.activeKernelVersion || qsTr("Detecting…"); color: ThemePalette.text; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }

                                Text { text: qsTr("Default kernel:"); color: ThemePalette.muted; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true; text: bridge.defaultKernel || qsTr("Not detected"); color: ThemePalette.text; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }

                                Text { text: qsTr("Bootloader:"); color: ThemePalette.muted; font.pixelSize: 12 }
                                Text { text: bridge.detectedBootloader || "—"; color: ThemePalette.text; font.pixelSize: 12 }

                                Text { text: qsTr("CPU instruction level:"); color: ThemePalette.muted; font.pixelSize: 12 }
                                Text { text: bridge.detectedCpuLevel || "—"; color: ThemePalette.text; font.pixelSize: 12 }
                            }
                        }

                        // Page 3: Activity Log Preview
                        Item {
                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text {
                                        Layout.fillWidth: true
                                        text: bridge.statusMessage || qsTr("No operation reported yet.")
                                        color: bridge.statusIsError ? ThemePalette.error : ThemePalette.text
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    NekoButton {
                                        text: qsTr("Open Full Log")
                                        onClicked: logDialog.open()
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: ThemePalette.inputBg
                                    border.color: ThemePalette.border
                                    border.width: 1
                                    radius: ThemePalette.cornerRadius
                                    clip: true

                                    ScrollView {
                                        id: tabLogScroll
                                        anchors.fill: parent
                                        clip: true
                                        ScrollBar.vertical: NekoScrollBar {}
                                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                        TextArea {
                                            width: tabLogScroll.availableWidth
                                            text: bridge.logs.length > 0 ? bridge.logs : qsTr("The activity log is empty.")
                                            readOnly: true
                                            selectByMouse: true
                                            color: ThemePalette.muted
                                            selectionColor: ThemePalette.selected
                                            selectedTextColor: ThemePalette.selectedText
                                            font.family: ThemePalette.fontMono
                                            font.pixelSize: 11
                                            textFormat: TextEdit.RichText
                                            wrapMode: TextEdit.Wrap
                                            padding: 8
                                            topInset: 0
                                            bottomInset: 0
                                            background: null
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ====================================================================
        // GTK3 STATUSBAR / PROGRESS
        // ====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: bridge.busy || bridge.progress > 0 ? 34 : 28
            color: ThemePalette.toolbar

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: ThemePalette.border
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                height: 2.5
                width: parent.width * Math.max(0, Math.min(100, bridge.progress)) / 100
                visible: bridge.busy || bridge.progress > 0
                color: ThemePalette.selected
                Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: bridge.busy ? qsTr("Working…") : qsTr("Ready")
                    color: bridge.busy ? ThemePalette.selected : ThemePalette.muted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1 kernels displayed").arg(window.filteredKernels.length)
                    color: ThemePalette.muted
                    font.pixelSize: 11
                }
                Text {
                    text: qsTr("Neko Kernel Manager %1").arg(Qt.application.version)
                    color: ThemePalette.faint
                    font.pixelSize: 10
                }
            }
        }
    }

    // ========================================================================
    // MODAL DIALOGS (Matecito Dark GTK3 Style)
    // ========================================================================

    // Remove Confirmation Dialog
    NekoDialog {
        id: removeDialog
        property var kernelData: null
        width: Math.min(480, window.width - 48)
        title: qsTr("Remove kernel")
        onClosed: kernelData = null

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: {
                    if (!removeDialog.kernelData) return ""
                    var k = removeDialog.kernelData
                    if (k.type === "vkpurge") {
                        return qsTr("Are you sure you want to purge obsolete kernel files for %1?\n\nThis kernel is an unreferenced leftover tracked by vkpurge. It will be cleanly removed from /boot.")
                            .arg(k.version)
                    }
                    if (k.type === "manual") {
                        return qsTr("Are you sure you want to remove the manual kernel %1?\n\nCorresponding files in /boot and /lib/modules will be deleted and the GRUB boot menu will be updated.")
                            .arg(k.version)
                    }
                    return qsTr("Are you sure you want to remove %1 (%2)?\n\nThe currently running kernel is protected, but this action cannot be undone.")
                        .arg(k.name).arg(k.version)
                }
                color: ThemePalette.text
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: ThemePalette.border
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                NekoButton {
                    text: qsTr("Cancel")
                    onClicked: removeDialog.close()
                }

                NekoButton {
                    text: qsTr("Remove")
                    destructive: true
                    enabled: !bridge.busy
                    onClicked: {
                        var k = removeDialog.kernelData
                        removeDialog.close()
                        if (k) {
                            bridge.removeKernel(k.name, k.type || "", k.version || "")
                        }
                    }
                }
            }
        }
    }

    // Purge Dialog
    NekoDialog {
        id: purgeDialog
        property string vkList: ""
        width: Math.min(500, window.width - 48)
        title: qsTr("Purge old kernels (vkpurge)")
        onAboutToShow: {
            vkList = bridge.getVkpurgeList()
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.fillWidth: true
                text: purgeDialog.vkList.length > 0
                      ? qsTr("Neko will run 'vkpurge rm all' to remove obsolete kernel files and reclaim disk space.\n\nThe following obsolete kernel releases were detected and will be purged:")
                      : qsTr("Neko will run 'vkpurge rm all' to clean obsolete kernel files.\nNo unreferenced kernels are currently reported by vkpurge.")
                color: ThemePalette.text
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(90, Math.max(40, purgeListText.contentHeight + 16))
                visible: purgeDialog.vkList.length > 0
                color: ThemePalette.inputBg
                border.color: ThemePalette.inputBorder
                radius: 4
                clip: true

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true
                    ScrollBar.vertical: NekoScrollBar {}
                    Text {
                        id: purgeListText
                        text: purgeDialog.vkList
                        color: ThemePalette.warning
                        font.family: ThemePalette.fontMono
                        font.pixelSize: 12
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The currently active and installed kernels will remain safe and untouched.")
                color: ThemePalette.muted
                font.pixelSize: 11
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: ThemePalette.border
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                NekoButton {
                    text: qsTr("Cancel")
                    onClicked: purgeDialog.close()
                }

                NekoButton {
                    text: qsTr("Purge")
                    destructive: true
                    enabled: !bridge.busy && purgeDialog.vkList.length > 0
                    onClicked: {
                        purgeDialog.close()
                        bridge.vkpurge()
                    }
                }
            }
        }
    }

    // Activity Log Dialog
    NekoDialog {
        id: logDialog
        objectName: "logDialog"
        width: Math.min(780, window.width - 48)
        height: Math.min(520, window.height - 48)
        title: qsTr("Activity Log")

        contentItem: ColumnLayout {
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                NekoButton {
                    text: qsTr("Export…")
                    enabled: bridge.logs.length > 0
                    onClicked: fileDialog.open()
                }
                NekoButton {
                    text: qsTr("Clear")
                    enabled: bridge.logs.length > 0
                    onClicked: bridge.clearLogs()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: ThemePalette.inputBg
                border.color: ThemePalette.border
                border.width: 1
                radius: ThemePalette.cornerRadius
                clip: true

                ScrollView {
                    id: logScroll
                    anchors.fill: parent
                    clip: true
                    ScrollBar.vertical: NekoScrollBar {}
                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                    TextArea {
                        width: logScroll.availableWidth
                        text: bridge.logs.length > 0 ? bridge.logs : qsTr("No log entries recorded.")
                        readOnly: true
                        selectByMouse: true
                        color: ThemePalette.text
                        selectionColor: ThemePalette.selected
                        selectedTextColor: ThemePalette.selectedText
                        font.family: ThemePalette.fontMono
                        font.pixelSize: 11
                        textFormat: TextEdit.RichText
                        wrapMode: TextEdit.Wrap
                        padding: 10
                        topInset: 0
                        bottomInset: 0
                        background: null
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: ThemePalette.border
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                NekoButton {
                    text: qsTr("Close")
                    onClicked: logDialog.close()
                }
            }
        }
    }

    // DKMS Dialog
    NekoDialog {
        id: dkmsDialog
        objectName: "dkmsDialog"
        width: Math.min(800, window.width - 48)
        height: Math.min(540, window.height - 48)
        title: qsTr("DKMS Module Management")

        contentItem: ColumnLayout {
            spacing: 10

            RowLayout {
                Layout.fillWidth: true
                Text {
                    Layout.fillWidth: true
                    text: qsTr("Manage out-of-tree DKMS modules and packages for installed kernels.")
                    color: ThemePalette.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                NekoButton {
                    text: qsTr("Refresh")
                    enabled: !bridge.busy
                    onClicked: bridge.updateDkmsModules()
                }
                NekoButton {
                    text: qsTr("Autoinstall")
                    enabled: !bridge.busy
                    suggested: true
                    onClicked: bridge.autoinstallDkms()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: ThemePalette.view
                border.color: ThemePalette.border
                border.width: 1
                radius: ThemePalette.cornerRadius
                clip: true

                ListView {
                    id: dkmsList
                    anchors.fill: parent
                    clip: true
                    model: bridge.dkmsModules
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: NekoScrollBar {}

                    delegate: Rectangle {
                        readonly property bool isRepoAvailable: modelData.isDkmsPackage && !modelData.packageInstalled
                        readonly property bool isModuleInstalled: !isRepoAvailable && modelData.status && (modelData.status === "installed" || modelData.status.indexOf("installed (") === 0)
                        readonly property bool isRemovable: modelData.isDkmsPackage ? !!modelData.packageInstalled : isModuleInstalled

                        width: dkmsList.width
                        height: 56
                        color: index % 2 === 0 ? "transparent" : ThemePalette.alternateRow
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: ThemePalette.border
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 12
                            anchors.rightMargin: 12
                            spacing: 12

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 2

                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name + " " + modelData.version
                                    color: ThemePalette.text
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.isDkmsPackage
                                          ? (modelData.packageInstalled ? qsTr("XBPS package installed; not registered in DKMS") : qsTr("Available from repository"))
                                          : qsTr("Kernel %1 · %2").arg(modelData.kernel || "—").arg(modelData.arch || "—")
                                    color: ThemePalette.muted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                Layout.preferredWidth: 110
                                text: modelData.status || qsTr("unknown")
                                color: isModuleInstalled ? ThemePalette.success
                                     : (modelData.status === "unregistered" || modelData.status === "built" || modelData.status === "added") ? ThemePalette.warning
                                     : ThemePalette.muted
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }

                            NekoButton {
                                text: isRemovable ? qsTr("Remove") : qsTr("Install")
                                destructive: isRemovable
                                suggested: !isRemovable
                                enabled: !bridge.busy
                                onClicked: {
                                    if (isRemovable)
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
                    Text { text: bridge.busy ? qsTr("Loading DKMS modules…") : qsTr("No DKMS modules found"); color: ThemePalette.text; font.pixelSize: 13 }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: ThemePalette.border
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                NekoButton {
                    text: qsTr("Close")
                    onClicked: dkmsDialog.close()
                }
            }
        }
    }

    // About Dialog
    NekoDialog {
        id: aboutDialog
        objectName: "aboutDialog"
        width: Math.min(440, window.width - 48)
        title: qsTr("About Neko Kernel Manager")

        contentItem: ColumnLayout {
            spacing: 12

            Image {
                source: "qrc:/neko/Data/logo.png"
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                sourceSize: Qt.size(128, 128)
                smooth: true
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Neko Kernel Manager")
                color: ThemePalette.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Graphical manager for installing, removing, and configuring Linux kernels and DKMS modules on Void Linux.\n\nThemed with Matecito Dark (GTK3).")
                color: ThemePalette.muted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Version %1 · GPL-3.0-only · Qt 6 / QML").arg(Qt.application.version)
                color: ThemePalette.faint
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: ThemePalette.border
            }

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                NekoButton {
                    text: qsTr("Close")
                    onClicked: aboutDialog.close()
                }
            }
        }
    }

    // File Save Dialog for Logs
    Platform.FileDialog {
        id: fileDialog
        title: qsTr("Save Logs")
        fileMode: Platform.FileDialog.SaveFile
        folder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        nameFilters: [qsTr("Text files (*.txt)"), qsTr("All files (*)")]
        defaultSuffix: "txt"
        onAccepted: {
            if (fileDialog.file.toString().length > 0) {
                var rawPath = fileDialog.file.toString()
                var filePath = decodeURIComponent(rawPath.replace(/^file:\/\//, ""))
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
