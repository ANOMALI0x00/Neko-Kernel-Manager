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
    minimumWidth: 860
    minimumHeight: 620
    title: qsTr("Neko Kernel Manager")
    color: themeColors.window

    property string selectedKernelName: ""
    property bool installedOnly: false

    // Matecito Dark (GTK3) Theme Colors & Metrics
    QtObject {
        id: themeColors

        function mix(a, b, amount) {
            return Qt.rgba(a.r + (b.r - a.r) * amount,
                           a.g + (b.g - a.g) * amount,
                           a.b + (b.b - a.b) * amount, 1.0)
        }

        // Matecito Dark core palette
        readonly property color window: theme.windowBg          // #1e2320 (warm olive dark)
        readonly property color view: theme.viewBg              // #1e2320
        readonly property color surface: theme.cardBg           // #272e2a (header/card dark slate)
        readonly property color toolbar: theme.headerbarBg      // #272e2a
        readonly property color popover: theme.popoverBg        // #272e2a
        readonly property color text: theme.windowFg            // #d3cdc3 (soft warm cream)
        readonly property color muted: mix(theme.windowFg, theme.windowBg, 0.45) // #9a948b
        readonly property color faint: mix(theme.windowFg, theme.windowBg, 0.68) // #68645e
        readonly property color border: Qt.rgba(211/255, 205/255, 195/255, 0.12)
        readonly property color strongBorder: Qt.rgba(211/255, 205/255, 195/255, 0.22)
        readonly property color hover: Qt.rgba(211/255, 205/255, 195/255, 0.08)
        readonly property color active: Qt.rgba(0, 0, 0, 0.25)

        // Accent & status
        readonly property color selected: theme.accentBg        // #8da383 (sage green)
        readonly property color selectedText: theme.accentFg    // #1e2320 (dark contrast text)
        readonly property color error: theme.errorBg            // #c27979 (muted coral)
        readonly property color errorText: theme.errorFg        // #1e2320
        readonly property color success: theme.accentBg         // #8da383
        readonly property color warning: "#d49a5b"              // warm amber
        readonly property color info: mix(theme.accentBg, theme.windowBg, 0.14)

        // GTK3 Controls specific
        readonly property color buttonBg: "#2d3530"
        readonly property color buttonHover: "#37413b"
        readonly property color buttonActive: "#242a26"
        readonly property color buttonBorder: Qt.rgba(211/255, 205/255, 195/255, 0.15)
        readonly property color inputBg: "#222724"
        readonly property color inputBorder: Qt.rgba(211/255, 205/255, 195/255, 0.16)
        readonly property real  cornerRadius: 6
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

    // GTK3 Styled Headerbar Button
    component GtkHeaderButton: Button {
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
                color: !hdrBtn.enabled ? themeColors.faint
                     : hdrBtn.checked ? themeColors.selected
                     : themeColors.text
                font.family: "0xProto Nerd Font"
                font.pixelSize: 15
                verticalAlignment: Text.AlignVCenter
            }
            Text {
                visible: hdrBtn.text.length > 0
                text: hdrBtn.text
                color: !hdrBtn.enabled ? themeColors.faint
                     : hdrBtn.checked ? themeColors.selected
                     : themeColors.text
                font.pixelSize: 12
                font.weight: hdrBtn.checked ? Font.DemiBold : Font.Normal
                verticalAlignment: Text.AlignVCenter
            }
        }

        background: Rectangle {
            color: !hdrBtn.enabled ? "transparent"
                 : hdrBtn.checked ? Qt.rgba(141/255, 163/255, 131/255, 0.18)
                 : hdrBtn.down ? themeColors.buttonActive
                 : hdrBtn.hovered ? themeColors.buttonHover
                 : themeColors.buttonBg
            border.color: hdrBtn.checked ? themeColors.selected
                        : hdrBtn.activeFocus ? themeColors.selected
                        : themeColors.buttonBorder
            border.width: (hdrBtn.activeFocus || hdrBtn.checked) ? 1.5 : 1
            radius: themeColors.cornerRadius
            Behavior on color { ColorAnimation { duration: 120 } }
        }

        ToolTip.visible: hovered && text.length > 0
        ToolTip.delay: 500
        ToolTip.text: text
    }

    // GTK3 Action Button (Standard, Suggested, Destructive)
    component GtkButton: Button {
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
            color: !gtkBtn.enabled ? themeColors.faint
                 : gtkBtn.suggested ? themeColors.selectedText
                 : gtkBtn.destructive ? themeColors.errorText
                 : themeColors.text
            font.pixelSize: 12
            font.weight: (gtkBtn.suggested || gtkBtn.destructive) ? Font.DemiBold : Font.Normal
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }

        background: Rectangle {
            color: !gtkBtn.enabled ? Qt.rgba(211/255, 205/255, 195/255, 0.05)
                 : gtkBtn.suggested ? (gtkBtn.down ? Qt.darker(themeColors.selected, 1.15)
                                                   : gtkBtn.hovered ? Qt.lighter(themeColors.selected, 1.10)
                                                                    : themeColors.selected)
                 : gtkBtn.destructive ? (gtkBtn.down ? Qt.darker(themeColors.error, 1.15)
                                                     : gtkBtn.hovered ? Qt.lighter(themeColors.error, 1.10)
                                                                      : themeColors.error)
                 : gtkBtn.down ? themeColors.buttonActive
                 : gtkBtn.hovered ? themeColors.buttonHover
                 : themeColors.buttonBg
            border.color: gtkBtn.activeFocus ? themeColors.selected
                        : (gtkBtn.suggested || gtkBtn.destructive) ? "transparent"
                        : themeColors.buttonBorder
            border.width: gtkBtn.activeFocus ? 2 : 1
            radius: themeColors.cornerRadius
            Behavior on color { ColorAnimation { duration: 120 } }
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

        // ====================================================================
        // GTK3 HEADERBAR (Matecito Dark)
        // ====================================================================
        Rectangle {
            id: headerBar
            Layout.fillWidth: true
            Layout.preferredHeight: 52
            color: themeColors.toolbar

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: themeColors.border
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                // Left buttons
                GtkHeaderButton {
                    symbol: "\uf021"
                    text: qsTr("Refresh")
                    enabled: !bridge.busy
                    onClicked: window.refreshKernels()
                }

                GtkHeaderButton {
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
                GtkHeaderButton {
                    symbol: "\uf1b3"
                    text: qsTr("DKMS")
                    enabled: !bridge.busy
                    onClicked: {
                        bridge.updateDkmsModules()
                        dkmsDialog.open()
                    }
                }

                GtkHeaderButton {
                    symbol: "\uf1f8"
                    text: qsTr("Remove Old")
                    enabled: !bridge.busy
                    onClicked: purgeDialog.open()
                }

                GtkHeaderButton {
                    symbol: "\uf15c"
                    text: qsTr("Logs")
                    onClicked: logDialog.open()
                }

                GtkHeaderButton {
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
            color: themeColors.surface

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: themeColors.border
            }

            // Left vertical accent stripe
            Rectangle {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 4
                color: bridge.statusIsError ? themeColors.error : themeColors.selected
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 14
                spacing: 12

                Text {
                    text: bridge.statusIsError ? "\uf071" : "\uf058"
                    color: bridge.statusIsError ? themeColors.error : themeColors.selected
                    font.family: "0xProto Nerd Font"
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
                        color: themeColors.text
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: bridge.statusMessage !== ""
                              ? bridge.statusMessage
                              : qsTr("Select a kernel below to inspect its details and actions.")
                        color: themeColors.muted
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                }

                GtkButton {
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
                        color: themeColors.text
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: qsTr("%1 kernels available · %2 installed")
                              .arg(bridge.kernels.length).arg(window.installedKernelCount)
                        color: themeColors.muted
                        font.pixelSize: 11
                    }
                }

                // GTK3 SearchEntry
                Rectangle {
                    Layout.preferredWidth: Math.min(320, window.width * 0.32)
                    Layout.minimumWidth: 200
                    implicitHeight: 34
                    color: themeColors.inputBg
                    border.color: kernelSearch.activeFocus ? themeColors.selected : themeColors.inputBorder
                    border.width: kernelSearch.activeFocus ? 1.5 : 1
                    radius: themeColors.cornerRadius

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 8
                        spacing: 6

                        Text {
                            text: "\uf002"
                            font.family: "0xProto Nerd Font"
                            font.pixelSize: 13
                            color: themeColors.muted
                        }

                        TextInput {
                            id: kernelSearch
                            Layout.fillWidth: true
                            color: themeColors.text
                            font.pixelSize: 12
                            clip: true
                            selectByMouse: true
                            selectionColor: themeColors.selected
                            selectedTextColor: themeColors.selectedText
                            verticalAlignment: Text.AlignVCenter
                            onTextChanged: window.selectFirstVisibleKernel(true)

                            Text {
                                anchors.fill: parent
                                text: qsTr("Search by name or version…")
                                color: themeColors.faint
                                font.pixelSize: 12
                                verticalAlignment: Text.AlignVCenter
                                visible: !kernelSearch.text && !kernelSearch.activeFocus
                            }
                        }

                        Text {
                            visible: kernelSearch.text.length > 0
                            text: "\uf00d"
                            font.family: "0xProto Nerd Font"
                            font.pixelSize: 12
                            color: themeColors.muted
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
                color: themeColors.view
                border.color: themeColors.border
                border.width: 1
                radius: themeColors.cornerRadius
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    // Column Headers
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 36
                        color: themeColors.surface
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: themeColors.border
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

                            Text { x: 0; width: headerColumns.statusWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Status"); color: themeColors.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { x: headerColumns.kernelX; width: headerColumns.kernelWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Kernel"); color: themeColors.muted; font.pixelSize: 11; font.weight: Font.DemiBold; horizontalAlignment: Text.AlignLeft; elide: Text.ElideRight }
                            Text { x: headerColumns.versionX; width: headerColumns.versionWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Version"); color: themeColors.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { visible: headerColumns.showType; x: headerColumns.versionX + headerColumns.versionWidth + headerColumns.gap; width: headerColumns.typeWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Type"); color: themeColors.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { visible: headerColumns.showSize; x: headerColumns.versionX + headerColumns.versionWidth + headerColumns.gap + (headerColumns.showType ? headerColumns.typeWidth + headerColumns.gap : 0); width: headerColumns.sizeWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Size"); color: themeColors.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Text { visible: headerColumns.showInstalled; x: headerColumns.versionX + headerColumns.versionWidth + headerColumns.gap + (headerColumns.showType ? headerColumns.typeWidth + headerColumns.gap : 0) + (headerColumns.showSize ? headerColumns.sizeWidth + headerColumns.gap : 0); width: headerColumns.installedWidth; anchors.verticalCenter: parent.verticalCenter; text: qsTr("Installed"); color: themeColors.muted; font.pixelSize: 11; font.weight: Font.DemiBold; elide: Text.ElideRight }
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
                            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
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
                                    color: kernelRow.isSelected ? themeColors.selected
                                         : kernelRow.hovered ? themeColors.hover
                                         : (index % 2 === 0 ? "transparent" : Qt.rgba(211/255, 205/255, 195/255, 0.02))
                                    border.color: kernelRow.activeFocus ? themeColors.selected : "transparent"
                                    border.width: kernelRow.activeFocus ? 1.5 : 0

                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: parent.width
                                        height: 1
                                        color: kernelRow.isSelected ? Qt.rgba(0, 0, 0, 0.15) : themeColors.border
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
                                            color: kernelRow.isSelected ? Qt.rgba(30/255, 35/255, 32/255, 0.20)
                                                 : kernelRow.running ? Qt.rgba(212/255, 154/255, 91/255, 0.18)
                                                 : kernelRow.defaultKernel ? Qt.rgba(141/255, 163/255, 131/255, 0.22)
                                                 : isOrphan ? Qt.rgba(194/255, 121/255, 121/255, 0.16)
                                                 : modelData.installed ? Qt.rgba(211/255, 205/255, 195/255, 0.10)
                                                 : "transparent"
                                            border.color: kernelRow.isSelected ? Qt.rgba(30/255, 35/255, 32/255, 0.40)
                                                        : kernelRow.running ? themeColors.warning
                                                        : kernelRow.defaultKernel ? themeColors.selected
                                                        : isOrphan ? themeColors.error
                                                        : modelData.installed ? themeColors.strongBorder
                                                        : "transparent"
                                            border.width: 1

                                            RowLayout {
                                                anchors.centerIn: parent
                                                spacing: 5
                                                Rectangle {
                                                    width: 6
                                                    height: 6
                                                    radius: 3
                                                    color: kernelRow.isSelected ? themeColors.selectedText
                                                         : kernelRow.running ? themeColors.warning
                                                         : kernelRow.defaultKernel ? themeColors.selected
                                                         : statusPill.isOrphan ? themeColors.error
                                                         : statusPill.isNoFiles ? themeColors.muted
                                                         : modelData.installed ? themeColors.text
                                                         : themeColors.faint
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
                                                    color: kernelRow.isSelected ? themeColors.selectedText
                                                         : kernelRow.running ? themeColors.warning
                                                         : kernelRow.defaultKernel ? themeColors.selected
                                                         : statusPill.isOrphan ? themeColors.error
                                                         : statusPill.isNoFiles ? themeColors.muted
                                                         : modelData.installed ? themeColors.text
                                                         : themeColors.muted
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
                                        color: kernelRow.isSelected ? themeColors.selectedText : themeColors.text
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
                                        color: kernelRow.isSelected ? themeColors.selectedText : themeColors.text
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
                                        color: kernelRow.isSelected ? themeColors.selectedText : themeColors.muted
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
                                        color: kernelRow.isSelected ? themeColors.selectedText : themeColors.muted
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
                                        color: kernelRow.isSelected ? themeColors.selectedText : themeColors.muted
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
                                color: themeColors.text
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
                                color: themeColors.muted
                                font.pixelSize: 12
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                            }
                            GtkButton {
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
                color: themeColors.surface
                border.color: themeColors.border
                border.width: 1
                radius: themeColors.cornerRadius

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
                            color: themeColors.surface
                            Rectangle {
                                anchors.bottom: parent.bottom
                                width: parent.width
                                height: 1
                                color: themeColors.border
                            }
                        }

                        TabButton {
                            id: detailsTab
                            text: qsTr("Details")
                            width: 105
                            contentItem: Text {
                                text: detailsTab.text
                                color: detailsTab.checked ? themeColors.text : themeColors.muted
                                font.pixelSize: 12
                                font.weight: detailsTab.checked ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: detailsTab.hovered ? themeColors.hover : "transparent"
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: detailsTab.checked ? 2.5 : 0
                                    color: themeColors.selected
                                }
                            }
                        }

                        TabButton {
                            id: systemTab
                            text: qsTr("System")
                            width: 105
                            contentItem: Text {
                                text: systemTab.text
                                color: systemTab.checked ? themeColors.text : themeColors.muted
                                font.pixelSize: 12
                                font.weight: systemTab.checked ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: systemTab.hovered ? themeColors.hover : "transparent"
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: systemTab.checked ? 2.5 : 0
                                    color: themeColors.selected
                                }
                            }
                        }

                        TabButton {
                            id: activityTab
                            text: qsTr("Activity")
                            width: 105
                            contentItem: Text {
                                text: activityTab.text
                                color: activityTab.checked ? themeColors.text : themeColors.muted
                                font.pixelSize: 12
                                font.weight: activityTab.checked ? Font.DemiBold : Font.Normal
                                horizontalAlignment: Text.AlignHCenter
                                verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle {
                                color: activityTab.hovered ? themeColors.hover : "transparent"
                                Rectangle {
                                    anchors.bottom: parent.bottom
                                    width: parent.width
                                    height: activityTab.checked ? 2.5 : 0
                                    color: themeColors.selected
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
                                        color: themeColors.text
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
                                        color: window.kernelIsRunning(window.selectedKernel) ? themeColors.warning
                                             : window.kernelIsDefault(window.selectedKernel) ? themeColors.selected
                                             : (window.selectedKernel && window.selectedKernel.type === "vkpurge") ? themeColors.error
                                             : themeColors.muted
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                }

                                RowLayout {
                                    spacing: 10

                                    GtkButton {
                                        visible: window.selectedKernel && !window.selectedKernel.installed
                                        enabled: visible && !bridge.busy
                                        suggested: true
                                        text: qsTr("Install")
                                        onClicked: bridge.installKernel(window.selectedKernel.name)
                                    }

                                    GtkButton {
                                        visible: window.selectedKernel && window.selectedKernel.installed && window.selectedKernel.hasFiles && window.selectedKernel.type !== "vkpurge" && !window.kernelIsDefault(window.selectedKernel)
                                        enabled: visible && !bridge.busy
                                        text: qsTr("Set as Default")
                                        onClicked: bridge.setDefaultKernel(window.selectedKernel.version)
                                    }

                                    GtkButton {
                                        visible: window.selectedKernel && window.selectedKernel.installed && window.selectedKernel.hasFiles && window.selectedKernel.type !== "vkpurge"
                                        enabled: visible && !bridge.busy
                                        text: qsTr("Reconfigure")
                                        onClicked: bridge.reconfigureKernel(window.selectedKernel.name, window.selectedKernel.type || "", window.selectedKernel.version || "")
                                    }

                                    GtkButton {
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

                                Text { text: qsTr("Running kernel:"); color: themeColors.muted; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true; text: bridge.activeKernelVersion || qsTr("Detecting…"); color: themeColors.text; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }

                                Text { text: qsTr("Default kernel:"); color: themeColors.muted; font.pixelSize: 12 }
                                Text { Layout.fillWidth: true; text: bridge.defaultKernel || qsTr("Not detected"); color: themeColors.text; font.pixelSize: 12; font.weight: Font.DemiBold; elide: Text.ElideRight }

                                Text { text: qsTr("Bootloader:"); color: themeColors.muted; font.pixelSize: 12 }
                                Text { text: bridge.detectedBootloader || "—"; color: themeColors.text; font.pixelSize: 12 }

                                Text { text: qsTr("CPU instruction level:"); color: themeColors.muted; font.pixelSize: 12 }
                                Text { text: bridge.detectedCpuLevel || "—"; color: themeColors.text; font.pixelSize: 12 }
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
                                        color: bridge.statusIsError ? themeColors.error : themeColors.text
                                        font.pixelSize: 12
                                        elide: Text.ElideRight
                                    }
                                    GtkButton {
                                        text: qsTr("Open Full Log")
                                        onClicked: logDialog.open()
                                    }
                                }

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    color: themeColors.inputBg
                                    border.color: themeColors.border
                                    border.width: 1
                                    radius: themeColors.cornerRadius

                                    Text {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        text: bridge.logs.length > 0 ? bridge.logs : qsTr("The activity log is empty.")
                                        color: themeColors.muted
                                        font.family: "0xProto Nerd Font Mono"
                                        font.pixelSize: 11
                                        textFormat: Text.RichText
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

        // ====================================================================
        // GTK3 STATUSBAR / PROGRESS
        // ====================================================================
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: bridge.busy || bridge.progress > 0 ? 34 : 28
            color: themeColors.toolbar

            Rectangle {
                anchors.top: parent.top
                width: parent.width
                height: 1
                color: themeColors.border
            }

            Rectangle {
                anchors.top: parent.top
                anchors.left: parent.left
                height: 2.5
                width: parent.width * Math.max(0, Math.min(100, bridge.progress)) / 100
                visible: bridge.busy || bridge.progress > 0
                color: themeColors.selected
                Behavior on width { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }
            }

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                spacing: 8

                Text {
                    text: bridge.busy ? qsTr("Working…") : qsTr("Ready")
                    color: bridge.busy ? themeColors.selected : themeColors.muted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
                Text {
                    Layout.fillWidth: true
                    text: qsTr("%1 kernels displayed").arg(window.filteredKernels.length)
                    color: themeColors.muted
                    font.pixelSize: 11
                }
                Text {
                    text: qsTr("Neko Kernel Manager %1").arg(Qt.application.version)
                    color: themeColors.faint
                    font.pixelSize: 10
                }
            }
        }
    }

    // ========================================================================
    // MODAL DIALOGS (Matecito Dark GTK3 Style)
    // ========================================================================

    // Remove Confirmation Dialog
    Dialog {
        id: removeDialog
        property var kernelData: null
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(480, window.width - 48)
        modal: true
        focus: true
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
                color: themeColors.text
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: themeColors.border
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                GtkButton {
                    text: qsTr("Cancel")
                    onClicked: removeDialog.close()
                }

                GtkButton {
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

        background: Rectangle {
            color: themeColors.popover
            border.color: themeColors.strongBorder
            border.width: 1
            radius: 8
        }
    }

    // Purge Dialog
    Dialog {
        id: purgeDialog
        property string vkList: ""
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(500, window.width - 48)
        modal: true
        focus: true
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
                color: themeColors.text
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(90, Math.max(40, purgeListText.contentHeight + 16))
                visible: purgeDialog.vkList.length > 0
                color: themeColors.inputBg
                border.color: themeColors.inputBorder
                radius: 4

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 8
                    Text {
                        id: purgeListText
                        text: purgeDialog.vkList
                        color: themeColors.warning
                        font.family: "0xProto Nerd Font Mono"
                        font.pixelSize: 12
                    }
                }
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("The currently active and installed kernels will remain safe and untouched.")
                color: themeColors.muted
                font.pixelSize: 11
            }

            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: themeColors.border
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                Item { Layout.fillWidth: true }

                GtkButton {
                    text: qsTr("Cancel")
                    onClicked: purgeDialog.close()
                }

                GtkButton {
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

        background: Rectangle {
            color: themeColors.popover
            border.color: themeColors.strongBorder
            border.width: 1
            radius: 8
        }
    }

    // Activity Log Dialog
    Dialog {
        id: logDialog
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(780, window.width - 48)
        height: Math.min(520, window.height - 48)
        modal: true
        focus: true
        title: qsTr("Activity Log")
        standardButtons: Dialog.Close

        ColumnLayout {
            anchors.fill: parent
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Item { Layout.fillWidth: true }
                GtkButton {
                    text: qsTr("Export…")
                    enabled: bridge.logs.length > 0
                    onClicked: fileDialog.open()
                }
                GtkButton {
                    text: qsTr("Clear")
                    enabled: bridge.logs.length > 0
                    onClicked: bridge.clearLogs()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: themeColors.inputBg
                border.color: themeColors.border
                border.width: 1
                radius: themeColors.cornerRadius

                ScrollView {
                    anchors.fill: parent
                    anchors.margins: 8
                    clip: true

                    TextArea {
                        text: bridge.logs.length > 0 ? bridge.logs : qsTr("No log entries recorded.")
                        readOnly: true
                        color: themeColors.text
                        selectionColor: themeColors.selected
                        selectedTextColor: themeColors.selectedText
                        font.family: "0xProto Nerd Font Mono"
                        font.pixelSize: 11
                        textFormat: TextEdit.RichText
                        wrapMode: TextEdit.Wrap
                        background: null
                    }
                }
            }
        }
        background: Rectangle {
            color: themeColors.popover
            border.color: themeColors.strongBorder
            border.width: 1
            radius: 8
        }
    }

    // DKMS Dialog
    Dialog {
        id: dkmsDialog
        objectName: "dkmsDialog"
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(800, window.width - 48)
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
                    text: qsTr("Manage out-of-tree DKMS modules and packages for installed kernels.")
                    color: themeColors.muted
                    font.pixelSize: 12
                    wrapMode: Text.WordWrap
                }
                GtkButton {
                    text: qsTr("Refresh")
                    enabled: !bridge.busy
                    onClicked: bridge.updateDkmsModules()
                }
                GtkButton {
                    text: qsTr("Autoinstall")
                    enabled: !bridge.busy
                    suggested: true
                    onClicked: bridge.autoinstallDkms()
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: themeColors.view
                border.color: themeColors.border
                border.width: 1
                radius: themeColors.cornerRadius
                clip: true

                ListView {
                    id: dkmsList
                    anchors.fill: parent
                    clip: true
                    model: bridge.dkmsModules
                    boundsBehavior: Flickable.StopAtBounds
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

                    delegate: Rectangle {
                        readonly property bool isRepoAvailable: modelData.isDkmsPackage && !modelData.packageInstalled
                        readonly property bool isModuleInstalled: !isRepoAvailable && modelData.status && (modelData.status === "installed" || modelData.status.indexOf("installed (") === 0)
                        readonly property bool isRemovable: modelData.isDkmsPackage ? !!modelData.packageInstalled : isModuleInstalled

                        width: dkmsList.width
                        height: 56
                        color: index % 2 === 0 ? "transparent" : Qt.rgba(211/255, 205/255, 195/255, 0.02)
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width
                            height: 1
                            color: themeColors.border
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
                                    color: themeColors.text
                                    font.pixelSize: 12
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.isDkmsPackage
                                          ? (modelData.packageInstalled ? qsTr("XBPS package installed; not registered in DKMS") : qsTr("Available from repository"))
                                          : qsTr("Kernel %1 · %2").arg(modelData.kernel || "—").arg(modelData.arch || "—")
                                    color: themeColors.muted
                                    font.pixelSize: 11
                                    elide: Text.ElideRight
                                }
                            }

                            Text {
                                Layout.preferredWidth: 110
                                text: modelData.status || qsTr("unknown")
                                color: isModuleInstalled ? themeColors.success
                                     : (modelData.status === "unregistered" || modelData.status === "built" || modelData.status === "added") ? themeColors.warning
                                     : themeColors.muted
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                horizontalAlignment: Text.AlignRight
                                elide: Text.ElideRight
                            }

                            GtkButton {
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
                    Text { text: bridge.busy ? qsTr("Loading DKMS modules…") : qsTr("No DKMS modules found"); color: themeColors.text; font.pixelSize: 13 }
                }
            }
        }
        background: Rectangle {
            color: themeColors.popover
            border.color: themeColors.strongBorder
            border.width: 1
            radius: 8
        }
    }

    // About Dialog
    Dialog {
        id: aboutDialog
        x: Math.round((window.width - width) / 2)
        y: Math.round((window.height - height) / 2)
        width: Math.min(440, window.width - 48)
        modal: true
        focus: true
        title: qsTr("About Neko Kernel Manager")
        standardButtons: Dialog.Close

        ColumnLayout {
            width: parent.width
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
                color: themeColors.text
                font.pixelSize: 18
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Graphical manager for installing, removing, and configuring Linux kernels and DKMS modules on Void Linux.\n\nThemed with Matecito Dark (GTK3).")
                color: themeColors.muted
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
            }

            Text {
                Layout.fillWidth: true
                text: qsTr("Version %1 · GPL-3.0-only · Qt 6 / QML").arg(Qt.application.version)
                color: themeColors.faint
                font.pixelSize: 11
                horizontalAlignment: Text.AlignHCenter
            }
        }
        background: Rectangle {
            color: themeColors.popover
            border.color: themeColors.strongBorder
            border.width: 1
            radius: 8
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
