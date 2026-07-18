import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

ApplicationWindow
{
    id: window
    visible: true
    width: 1100
    height: 800
    minimumWidth: 560
    minimumHeight: 440
    title: "Neko Kernel Manager"
    Material.theme: Material.Dark
    Material.accent: palette.accent
    Material.primary: palette.bg
    font.family: "Inter"

    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    // Neko Wizard theme — minimal / flat / sharp.
    //   Every colour derives from the system theme (`theme`, fed by Noctalia /
    //   matugen / pywal / libadwaita) so the whole app repaints coherently when
    //   the theme changes. Neutral tokens are mixed from the base colours with the
    //   same ratios Neko Wizard's CSS uses, so they render correctly for light or
    //   dark themes alike. Legacy names are kept as single-accent / error aliases.
    QtObject
    {
        id: palette
        // linear blend of two colours, t in [0,1] (Neko Wizard's mix())
        function mix(a, b, t) { return Qt.rgba(a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t, 1.0) }

        readonly property color bg: theme.windowBg
        readonly property color surface: theme.cardBg
        readonly property color surfaceHi: mix(theme.cardBg, theme.windowFg, 0.07)   // hover fill
        readonly property color currentLine: mix(theme.windowBg, theme.windowFg, 0.14) // border
        readonly property color borderHi: mix(theme.windowBg, theme.windowFg, 0.28)  // border hover
        readonly property color selection: mix(theme.cardBg, theme.windowFg, 0.07)
        readonly property color fg: theme.windowFg                                    // text
        readonly property color textSoft: mix(theme.windowFg, theme.windowBg, 0.25)
        readonly property color comment: mix(theme.windowFg, theme.windowBg, 0.45)   // text_dim
        readonly property color accent: theme.accentBg
        readonly property color accentHi: mix(theme.accentBg, theme.windowFg, 0.20)
        readonly property color accentFg: theme.accentFg
        readonly property color error: theme.errorBg
        // Legacy aliases → collapsed onto the neutral + single-accent scheme
        readonly property color cyan: theme.accentBg
        readonly property color green: theme.accentBg
        readonly property color orange: theme.accentBg
        readonly property color pink: theme.errorBg
        readonly property color purple: theme.accentBg
    }

    // Title-bar controls (monochrome; close keeps the theme's error red)
    QtObject
    {
        id: dracula
        readonly property color bg: theme.headerbarBg
        readonly property color purple: theme.accentBg
        readonly property color red: theme.errorBg      // close
        readonly property color yellow: palette.comment // maximize
        readonly property color green: palette.comment  // minimize
    }

    // Main Border and Background
    Rectangle
    {
        anchors.fill: parent
        color: palette.bg
        border.color: palette.currentLine
        border.width: 1
        radius: 0

        ColumnLayout
        {
            anchors.fill: parent
            anchors.margins: 2
            spacing: 0

            // Header bar
            Rectangle
            {
                Layout.fillWidth: true
                height: 36
                color: dracula.bg

                // Bottom divider (borders-only depth)
                Rectangle
                {
                    anchors.bottom: parent.bottom
                    width: parent.width; height: 1
                    color: palette.currentLine
                }

                MouseArea
                {
                    anchors.fill: parent
                    property point lastMousePos
                    onPressed: (mouse) => lastMousePos = Qt.point(mouse.x, mouse.y)
                    onPositionChanged: (mouse) =>
                    {
                        var delta = Qt.point(mouse.x - lastMousePos.x, mouse.y - lastMousePos.y)
                        window.x += delta.x
                        window.y += delta.y
                    }
                }

                RowLayout
                {
                    anchors.fill: parent
                    anchors.leftMargin: 15
                    anchors.rightMargin: 10

                    Text
                    {
                        text: qsTr("Neko Kernel Manager")
                        color: palette.comment
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        font.letterSpacing: 0.4
                    }

                    Item
                    {
                        Layout.fillWidth: true
                    }

                    Row
                    {
                        spacing: 8
                        Layout.alignment: Qt.AlignVCenter

                        // Minimize
                        Rectangle
                        {
                            width: 12; height: 12; radius: 0; color: dracula.green
                            MouseArea
                            {
                                anchors.fill: parent; onClicked: window.showMinimized()
                            }
                        }
                        // Maximize
                        Rectangle
                        {
                            width: 12; height: 12; radius: 0; color: dracula.yellow
                            MouseArea
                            {
                                anchors.fill: parent; onClicked: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized()
                            }
                        }
                        // Close
                        Rectangle
                        {
                            width: 12; height: 12; radius: 0; color: dracula.red
                            MouseArea
                            {
                                anchors.fill: parent; onClicked: window.close()
                            }
                        }
                    }
                }
            }

            // Main content surface (flat — borders only, no gradients)
            Item
            {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle
                {
                    anchors.fill: parent
                    color: palette.bg
                }

                ColumnLayout
                {
                    anchors.fill: parent
                    anchors.margins: 16
                    spacing: 12

                    // Top Bar: brand · navigation · status
                    RowLayout
                    {
                        Layout.fillWidth: true
                        spacing: 12

                        // Brand
                        Image
                        {
                            id: logoImage; source: "qrc:/neko/Data/logo.png"
                            Layout.preferredWidth: 32; Layout.preferredHeight: 32
                            sourceSize: Qt.size(64, 64); smooth: true
                        }
                        Text
                        {
                            id: mainTitle
                            text: "Neko Kernel Manager"
                            color: palette.fg
                            font.pixelSize: 17; font.weight: Font.Bold; font.letterSpacing: -0.3
                            visible: window.width >= 760   // hide brand text when space is tight
                        }

                        Item { Layout.fillWidth: true }

                        // Primary navigation — clearly clickable segmented tabs
                        Row
                        {
                            spacing: 8
                            Repeater
                            {
                                model: [
                                    { label: qsTr("Kernels"),   tip: qsTr("Install, remove and purge Void kernels") },
                                    { label: qsTr("Downloads"), tip: qsTr("Fetch kernels from kernel.org or a Git repo") },
                                    { label: qsTr("Build"),     tip: qsTr("Configure and build a custom kernel") }
                                ]
                                Rectangle
                                {
                                    readonly property bool active: mainStack.currentIndex === index
                                    width: navLabel.implicitWidth + 24; height: 30
                                    color: active ? palette.accent : (navHover.hovered ? palette.surfaceHi : palette.surface)
                                    border.width: 1
                                    border.color: active ? palette.accent : (navHover.hovered ? palette.borderHi : palette.currentLine)

                                    Text
                                    {
                                        id: navLabel
                                        anchors.centerIn: parent
                                        text: modelData.label
                                        color: active ? palette.accentFg : (navHover.hovered ? palette.fg : palette.textSoft)
                                        font.pixelSize: 13
                                        font.weight: active ? Font.DemiBold : Font.Medium
                                    }

                                    HoverHandler { id: navHover }
                                    ToolTip.text: modelData.tip
                                    ToolTip.visible: navHover.hovered
                                    ToolTip.delay: 500
                                    MouseArea
                                    {
                                        anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                        onClicked: mainStack.currentIndex = index
                                    }
                                }
                            }
                        }

                        Item { Layout.fillWidth: true }

                        // Status pill
                        Row
                        {
                            spacing: 8
                            Rectangle
                            {
                                anchors.verticalCenter: parent.verticalCenter
                                width: 8; height: 8; radius: 0
                                color: bridge.busy ? palette.accent : palette.comment
                            }
                            Text
                            {
                                anchors.verticalCenter: parent.verticalCenter
                                text: bridge.busy ? qsTr("Working") : qsTr("Ready")
                                color: palette.textSoft; font.pixelSize: 12; font.weight: Font.Medium
                                visible: window.width >= 680   // dot alone stays when narrow
                            }
                        }

                        // Logs
                        Button
                        {
                            id: logBtn; Layout.leftMargin: 4
                            hoverEnabled: true
                            contentItem: Text
                            {
                                text: qsTr("Logs"); color: logBtn.hovered ? palette.fg : palette.textSoft
                                font.pixelSize: 12; font.weight: Font.Medium
                                horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                            }
                            background: Rectangle
                            {
                                implicitWidth: 62; implicitHeight: 30; radius: 0
                                color: logBtn.hovered ? palette.surfaceHi : palette.surface
                                border.color: logBtn.hovered ? palette.borderHi : palette.currentLine
                            }
                            ToolTip.text: qsTr("System logs & operations")
                            ToolTip.visible: hovered
                            ToolTip.delay: 500
                            onClicked: logModal.open()
                        }
                    }

                    // Global Status and Progress Bar Section
                    ColumnLayout
                    {
                        Layout.fillWidth: true
                        Layout.leftMargin: 14
                        Layout.rightMargin: 14
                        Layout.topMargin: 2
                        Layout.bottomMargin: 6
                        visible: bridge.busy || bridge.progress > 0
                        spacing: 4

                        RowLayout
                        {
                            Layout.fillWidth: true
                            Text
                            {
                                text: bridge.statusMessage
                                color: palette.comment
                                font.pixelSize: 10
                                font.italic: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }
                            Text
                            {
                                text: bridge.progress + "%"
                                color: palette.green
                                font.bold: true
                                font.pixelSize: 10
                            }
                        }

                        // Progress bar — flat: bordered trough, solid accent fill
                        Rectangle
                        {
                            id: globalProgressBarBg
                            Layout.fillWidth: true
                            height: 4
                            color: palette.surface
                            border.color: palette.currentLine
                            radius: 0
                            clip: true

                            Rectangle
                            {
                                id: globalProgressBarFill
                                width: parent.width * (bridge.progress / 100.0)
                                height: parent.height
                                radius: 0
                                color: palette.accent

                                Behavior on width
                                {
                                    NumberAnimation
                                    {
                                        duration: 300; easing.type: Easing.OutCubic
                                    }
                                }
                            }
                        }
                    }

                    StackLayout
                    {
                        id: mainStack
                        Layout.fillWidth: true; Layout.fillHeight: true

                        // 0: XBPS Kernels View
                        ColumnLayout
                        {
                            spacing: 8
                            Layout.fillWidth: true; Layout.fillHeight: true
                            RowLayout
                            {
                                Layout.fillWidth: true
                                spacing: 12
                                ColumnLayout
                                {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text
                                    {
                                        text: qsTr("Installed & Available Kernels"); color: palette.fg; font.pixelSize: 16; font.weight: Font.Bold; font.letterSpacing: -0.2
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                    }
                                    Text
                                    {
                                        text: qsTr("Install or remove kernels from the official Void repositories, or purge old versions.")
                                        color: palette.comment; font.pixelSize: 11
                                        Layout.fillWidth: true; wrapMode: Text.WordWrap; maximumLineCount: 2; elide: Text.ElideRight
                                    }
                                }

                                // Search / filter
                                TextField
                                {
                                    id: kernelSearch
                                    Layout.preferredWidth: 220; Layout.minimumWidth: 120
                                    placeholderText: qsTr("Search kernels…")
                                    color: palette.fg; font.pixelSize: 12
                                    verticalAlignment: TextInput.AlignVCenter
                                    leftPadding: 10; rightPadding: 10
                                    background: Rectangle
                                    {
                                        implicitHeight: 28; radius: 0
                                        color: palette.bg
                                        border.color: kernelSearch.activeFocus ? palette.accent : palette.currentLine
                                    }
                                }

                                Button
                                {
                                    id: purgeBtn; text: qsTr("Purge Old Kernels"); enabled: !bridge.busy
                                    hoverEnabled: true
                                    ToolTip.text: qsTr("Remove old, unused kernel versions (vkpurge) to free disk space")
                                    ToolTip.visible: hovered
                                    ToolTip.delay: 500
                                    contentItem: Text
                                    {
                                        text: purgeBtn.text; font.bold: true; font.pixelSize: 11; color: purgeBtn.enabled ? palette.accentFg : palette.comment; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle
                                    {
                                        implicitHeight: 28; implicitWidth: 140; radius: 0
                                        color: !purgeBtn.enabled ? palette.currentLine :
                                        (purgeArea.pressed ? Qt.darker(palette.error, 1.2) :
                                        (purgeArea.containsMouse ? Qt.lighter(palette.error, 1.1) : palette.error))

                                        MouseArea
                                        {
                                            id: purgeArea
                                            anchors.fill: parent
                                            hoverEnabled: true
                                            onClicked: purgeBtn.clicked()
                                        }
                                    }
                                    onClicked: bridge.vkpurge()
                                }
                            }

                            // Kernel grid with empty / loading state
                            Item
                            {
                                Layout.fillWidth: true; Layout.fillHeight: true

                                ScrollView
                                {
                                    id: kernelScroll
                                    anchors.fill: parent
                                    clip: true
                                    ScrollBar.vertical.policy: ScrollBar.AsNeeded
                                    ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                                    GridView
                                    {
                                        id: kernelGrid
                                        // Constrain to the real viewport so columns never overflow
                                        width: kernelScroll.availableWidth; height: contentHeight
                                        // Responsive: aim for ~200px cards, add columns as the window grows
                                        cellWidth: width / Math.max(1, Math.floor(width / 200))
                                        cellHeight: 132
                                        model: kernelSearch.text.trim().length === 0 ? bridge.kernels
                                               : bridge.kernels.filter(function(k) { return (k.name || "").toLowerCase().indexOf(kernelSearch.text.trim().toLowerCase()) !== -1 })
                                        interactive: false // ScrollView handles interaction

                                        delegate: Rectangle
                                        {
                                            id: kernelDelegate
                                            width: kernelGrid.cellWidth - 10; height: kernelGrid.cellHeight - 10
                                            radius: 0
                                            color: palette.surface
                                            // Priority of borders:
                                            // 1. Purple if selected (sourceTemplate)
                                            // 2. Green if target of build (targetTemplate)
                                            // 3. Orange if manual
                                            // 4. Default currentLine
                                            border.color: bridge.sourceTemplate === modelData.name ? palette.purple :
                                            (bridge.targetTemplate === modelData.name ? palette.green :
                                            palette.currentLine)
                                            border.width: (bridge.sourceTemplate === modelData.name || bridge.targetTemplate === modelData.name) ? 2 : 1

                                            HoverHandler
                                            {
                                                id: hoverHandler
                                            }

                                            MouseArea
                                            {
                                                anchors.fill: parent
                                                onClicked:
                                                {
                                                    bridge.sourceTemplate = modelData.name

                                                    // Prepare targetTemplate name based on selection
                                                    var finalName = "neko-kernel"
                                                    var parts = modelData.name.split("-")

                                                    if (parts.length > 1 && parts[0] === "linux") {
                                                        if (parts[1] === "lts" || parts[1] === "zen" || parts[1] === "rt" || parts[1] === "mainline") {
                                                            finalName += "-" + parts[1] + "-" + parts.slice(2).join("-")
                                                        } else {
                                                            finalName += "-" + parts.slice(1).join("-")
                                                        }
                                                    } else {
                                                        finalName = modelData.name
                                                    }

                                                    bridge.targetTemplate = finalName
                                                }
                                            }

                                            ColumnLayout
                                            {
                                                anchors.centerIn: parent
                                                width: parent.width - 16
                                                spacing: 6

                                                Rectangle
                                                {
                                                    Layout.alignment: Qt.AlignHCenter
                                                    width: 38; height: 38; radius: 0
                                                    color: palette.bg
                                                    Image
                                                    {
                                                        anchors.centerIn: parent
                                                        source: "qrc:/neko/Data/logo.png"
                                                        width: 22; height: 22; fillMode: Image.PreserveAspectFit
                                                    }
                                                    Rectangle
                                                    {
                                                        anchors.bottom: parent.bottom; anchors.right: parent.right
                                                        width: 12; height: 12; radius: 0; color: modelData.type === "manual" ? palette.accent : palette.comment
                                                        border.color: palette.bg; border.width: 2
                                                    }
                                                }

                                                ColumnLayout
                                                {
                                                    Layout.fillWidth: true; spacing: 1
                                                    Text
                                                    {
                                                        text: modelData.name
                                                        color: palette.fg; font.bold: true; font.pixelSize: 11
                                                        Layout.alignment: Qt.AlignHCenter; Layout.fillWidth: true
                                                        horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
                                                    }
                                                    Text
                                                    {
                                                        text: modelData.type === "manual" ? "Custom Installation" : modelData.version
                                                        color: modelData.type === "manual" ? palette.orange : palette.comment
                                                        font.pixelSize: 9
                                                        Layout.alignment: Qt.AlignHCenter; Layout.fillWidth: true
                                                        horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
                                                    }
                                                }

                                                Item
                                                {
                                                    Layout.alignment: Qt.AlignHCenter
                                                    Layout.preferredWidth: 85; Layout.preferredHeight: 26
                                                    property bool isActive: modelData.installed && (bridge.activeKernelVersion === modelData.version || bridge.activeKernelVersion.includes(modelData.version))

                                                    // Running Indicator (Non-actionable)
                                                    Rectangle
                                                    {
                                                        anchors.fill: parent
                                                        visible: parent.isActive
                                                        radius: 0; color: "transparent"; border.color: palette.accent; border.width: 1
                                                        Text
                                                        {
                                                            anchors.centerIn: parent
                                                            text: qsTr("Running"); font.bold: true; font.pixelSize: 10; color: palette.accent
                                                        }
                                                    }

                                                    // Action Button (Install/Remove)
                                                    Button
                                                    {
                                                        id: actionBtn
                                                        anchors.fill: parent
                                                        visible: !parent.isActive
                                                        text: modelData.installed ? "Remove" : "Install"
                                                        enabled: !bridge.busy
                                                        contentItem: Text
                                                        {
                                                            text: actionBtn.text; font.bold: true; font.pixelSize: 10
                                                            color: !actionBtn.enabled ? palette.comment : palette.accentFg
                                                            horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                                        }
                                                        background: Rectangle
                                                        {
                                                            radius: 0
                                                            color: !actionBtn.enabled ? palette.currentLine : (modelData.installed ? palette.pink : palette.green)
                                                        }
                                                        onClicked: modelData.installed ? bridge.removeKernel(modelData.name) : bridge.installKernel(modelData.name)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }

                                // Shown when the grid has no items
                                ColumnLayout
                                {
                                    anchors.centerIn: parent
                                    spacing: 6
                                    visible: kernelGrid.count === 0
                                    Text
                                    {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: bridge.busy ? qsTr("Loading kernels…")
                                              : (kernelSearch.text.trim().length > 0 ? qsTr("No matches") : qsTr("No kernels found"))
                                        color: palette.textSoft; font.pixelSize: 14; font.weight: Font.Medium
                                    }
                                    Text
                                    {
                                        Layout.alignment: Qt.AlignHCenter
                                        text: bridge.busy ? qsTr("Querying the Void repositories")
                                              : (kernelSearch.text.trim().length > 0
                                                 ? qsTr("No kernel matches “%1”").arg(kernelSearch.text.trim())
                                                 : qsTr("Nothing to show yet — try the Downloads tab"))
                                        color: palette.comment; font.pixelSize: 11
                                    }
                                }
                            }
                        }

                        // 1: Downloads
                        ScrollView
                        {
                            id: downloadsScroll
                            Layout.fillWidth: true; Layout.fillHeight: true
                            clip: true
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout
                            {
                                width: downloadsScroll.availableWidth
                                spacing: 12

                                // Page header
                                ColumnLayout
                                {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text
                                    {
                                        text: qsTr("Download Kernels"); color: palette.fg; font.pixelSize: 16; font.weight: Font.Bold; font.letterSpacing: -0.2
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                    }
                                    Text
                                    {
                                        text: qsTr("Fetch a kernel from kernel.org, or clone a custom kernel from a Git repository.")
                                        color: palette.comment; font.pixelSize: 11
                                        Layout.fillWidth: true; wrapMode: Text.WordWrap; elide: Text.ElideRight
                                    }
                                }

                                GroupBox
                                {
                                    label: RowLayout
                                    {
                                        spacing: 8
                                        Rectangle
                                        {
                                            width: 4; height: 20; radius: 0; color: palette.green
                                        }
                                        Text
                                        {
                                            text: qsTr("KERNEL.ORG DOWNLOAD"); color: palette.comment; font.bold: true; font.pixelSize: 13; font.letterSpacing: 1.1
                                        }
                                    }
                                    Layout.fillWidth: true
                                    background: Rectangle
                                    {
                                        color: palette.surface; radius: 0; border.color: palette.currentLine; y: 15
                                    }
                                    ColumnLayout
                                    {
                                        anchors.fill: parent; anchors.margins: 12; anchors.topMargin: 24; spacing: 12
                                        RowLayout
                                        {
                                            spacing: 12
                                            ColumnLayout
                                            {
                                                Text
                                                {
                                                    text: qsTr("Variant"); color: palette.comment; font.pixelSize: 12
                                                }
                                                ComboBox
                                                {
                                                    id: kernelVariant; model: ["stable", "lts", "mainline", "all"]; Layout.preferredWidth: 160
                                                    onCurrentTextChanged: bridge.fetchKernelOrgVersions(currentText)
                                                    Component.onCompleted: bridge.fetchKernelOrgVersions(currentText)
                                                    background: Rectangle
                                                    {
                                                        implicitHeight: 32; radius: 0; color: palette.bg; border.color: palette.currentLine
                                                    }
                                                    delegate: ItemDelegate
                                                    {
                                                        width: kernelVariant.width
                                                        hoverEnabled: true
                                                        padding: 10
                                                        contentItem: Text
                                                        {
                                                            text: modelData
                                                            color: highlighted ? palette.accentFg : palette.fg
                                                            font: kernelVariant.font
                                                            elide: Text.ElideRight
                                                            verticalAlignment: Text.AlignVCenter
                                                            Behavior on color
                                                            {
                                                                ColorAnimation
                                                                {
                                                                    duration: 150
                                                                }
                                                            }
                                                        }
                                                        background: Rectangle
                                                        {
                                                            color: highlighted ? palette.accent : (hovered ? palette.surfaceHi : "transparent")
                                                            radius: 0
                                                            Behavior on color
                                                            {
                                                                ColorAnimation
                                                                {
                                                                    duration: 150
                                                                }
                                                            }
                                                        }
                                                    }
                                                    popup: Popup
                                                    {
                                                        y: kernelVariant.height + 2
                                                        width: kernelVariant.width
                                                        implicitHeight: Math.min(contentItem.implicitHeight, 400)
                                                        padding: 4
                                                        contentItem: ListView
                                                        {
                                                            clip: true
                                                            implicitHeight: contentHeight
                                                            model: kernelVariant.popup.visible ? kernelVariant.delegateModel : null
                                                            currentIndex: kernelVariant.highlightedIndex
                                                            ScrollIndicator.vertical: ScrollIndicator
                                                            {
                                                            }
                                                        }
                                                        background: Rectangle
                                                        {
                                                            color: palette.surface
                                                            border.color: palette.currentLine
                                                            border.width: 1
                                                            radius: 0
                                                        }
                                                    }
                                                }
                                            }
                                            ColumnLayout
                                            {
                                                Layout.fillWidth: true
                                                Text
                                                {
                                                    text: qsTr("Version"); color: palette.comment; font.pixelSize: 12
                                                }
                                                ComboBox
                                                {
                                                    id: kernelVerCombo; model: bridge.kernelOrgVersions; Layout.fillWidth: true
                                                    background: Rectangle
                                                    {
                                                        implicitHeight: 32; radius: 0; color: palette.bg; border.color: palette.currentLine
                                                    }
                                                    delegate: ItemDelegate
                                                    {
                                                        width: kernelVerCombo.width
                                                        hoverEnabled: true
                                                        padding: 10
                                                        contentItem: Text
                                                        {
                                                            text: modelData
                                                            color: highlighted ? palette.accentFg : palette.fg
                                                            font: kernelVerCombo.font
                                                            elide: Text.ElideRight
                                                            verticalAlignment: Text.AlignVCenter
                                                            Behavior on color
                                                            {
                                                                ColorAnimation
                                                                {
                                                                    duration: 150
                                                                }
                                                            }
                                                        }
                                                        background: Rectangle
                                                        {
                                                            color: highlighted ? palette.accent : (hovered ? palette.surfaceHi : "transparent")
                                                            radius: 0
                                                            Behavior on color
                                                            {
                                                                ColorAnimation
                                                                {
                                                                    duration: 150
                                                                }
                                                            }
                                                        }
                                                    }
                                                    popup: Popup
                                                    {
                                                        y: kernelVerCombo.height + 2
                                                        width: kernelVerCombo.width
                                                        implicitHeight: Math.min(contentItem.implicitHeight, 400)
                                                        padding: 4
                                                        contentItem: ListView
                                                        {
                                                            clip: true
                                                            implicitHeight: contentHeight
                                                            model: kernelVerCombo.popup.visible ? kernelVerCombo.delegateModel : null
                                                            currentIndex: kernelVerCombo.highlightedIndex
                                                            ScrollIndicator.vertical: ScrollIndicator
                                                            {
                                                            }
                                                        }
                                                        background: Rectangle
                                                        {
                                                            color: palette.surface
                                                            border.color: palette.currentLine
                                                            border.width: 1
                                                            radius: 0
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        Button
                                        {
                                            id: dlBtn; text: bridge.busy ? "Working..." : "Download Now"; Layout.fillWidth: true; enabled: kernelVerCombo.currentText !== "" && !bridge.busy
                                            contentItem: Text
                                            {
                                                text: dlBtn.text; font.bold: true; font.pixelSize: 13; color: palette.accentFg; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; radius: 0; color: dlBtn.enabled ? palette.green : (bridge.busy ? palette.purple : palette.currentLine)
                                                Rectangle
                                                {
                                                    anchors.fill: parent; radius: 0; color: "white"; opacity: dlBtn.pressed ? 0.2 : (dlBtn.hovered ? 0.1 : 0.0)
                                                }
                                            }
                                            onClicked: bridge.downloadKernelOrg(kernelVariant.currentText, kernelVerCombo.currentText)
                                        }
                                    }
                                }

                                // Separator — flat 1px divider (borders-only depth)
                                Rectangle
                                {
                                    Layout.fillWidth: true
                                    height: 1
                                    Layout.topMargin: 6
                                    Layout.bottomMargin: 6
                                    color: palette.currentLine
                                }

                                GroupBox
                                {
                                    label: RowLayout
                                    {
                                        spacing: 8
                                        Rectangle
                                        {
                                            width: 4; height: 20; radius: 0; color: palette.purple
                                        }
                                        Text
                                        {
                                            text: qsTr("CUSTOM TARBALL / GIT REPOSITORY"); color: palette.comment; font.bold: true; font.pixelSize: 13; font.letterSpacing: 1.1
                                        }
                                        Item
                                        {
                                            Layout.fillWidth: true
                                        }
                                        Button
                                        {
                                            id: cleanBtn
                                            text: qsTr("Clean Uninstalled Templates"); enabled: !bridge.busy
                                            hoverEnabled: true
                                            ToolTip.text: qsTr("Delete leftover build templates for kernels that are no longer installed")
                                            ToolTip.visible: hovered; ToolTip.delay: 500
                                            onClicked: bridge.cleanUninstalledTemplates()
                                            background: Rectangle
                                            {
                                                implicitWidth: 120; implicitHeight: 22; radius: 0
                                                color: cleanBtn.hovered ? palette.surfaceHi : palette.currentLine
                                            }
                                            contentItem: Text
                                            {
                                                text: parent.text; color: palette.fg; font.bold: true; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                        }
                                    }
                                    Layout.fillWidth: true
                                    background: Rectangle
                                    {
                                        color: palette.surface; radius: 0; border.color: palette.currentLine; y: 15
                                    }

                                    RowLayout
                                    {
                                        anchors.fill: parent; anchors.margins: 12; anchors.topMargin: 24; spacing: 10
                                        TextField
                                        {
                                            id: gitUrl
                                            placeholderText: qsTr("Git repo URL or tarball URL")
                                            Layout.fillWidth: true; color: palette.fg; verticalAlignment: TextInput.AlignVCenter
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; color: palette.bg; radius: 0; border.color: palette.currentLine
                                            }
                                        }
                                        Button
                                        {
                                            id: cloneBtn
                                            text: qsTr("Clone / Download")
                                            Layout.preferredWidth: 160
                                            enabled: gitUrl.text.trim().length > 0 && !bridge.busy

                                            contentItem: Text
                                            {
                                                text: cloneBtn.text
                                                font.bold: true
                                                font.pixelSize: 13
                                                color: cloneBtn.enabled ? palette.accentFg : palette.comment
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle
                                            {
                                                implicitHeight: 32
                                                radius: 0
                                                color: cloneBtn.enabled ? palette.purple : palette.currentLine

                                                Rectangle
                                                {
                                                    anchors.fill: parent
                                                    radius: 0
                                                    color: "white"
                                                    opacity: cloneBtn.enabled ? (cloneBtn.pressed ? 0.2 : (cloneBtn.hovered ? 0.1 : 0.0)) : 0.0
                                                    visible: cloneBtn.enabled
                                                }
                                            }

                                            onClicked: bridge.cloneCustomGit(gitUrl.text.trim())
                                        }
                                    }
                                }
                                Item
                                {
                                    Layout.fillHeight: true
                                }
                            }
                        }
                        // 2: Build & Config
                        ScrollView
                        {
                            id: buildConfigScroll
                            Layout.fillWidth: true; Layout.fillHeight: true
                            clip: true
                            ScrollBar.vertical.policy: ScrollBar.AsNeeded
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout
                            {
                                width: buildConfigScroll.availableWidth
                                spacing: 14
                                Layout.fillWidth: true; Layout.fillHeight: true

                                // Page header
                                ColumnLayout
                                {
                                    Layout.fillWidth: true
                                    spacing: 2
                                    Text
                                    {
                                        text: qsTr("Build & Configuration"); color: palette.fg; font.pixelSize: 16; font.weight: Font.Bold; font.letterSpacing: -0.2
                                        Layout.fillWidth: true; elide: Text.ElideRight
                                    }
                                    Text
                                    {
                                        text: qsTr("Tune optimization options, then build and export a custom kernel package.")
                                        color: palette.comment; font.pixelSize: 11
                                        Layout.fillWidth: true; wrapMode: Text.WordWrap; elide: Text.ElideRight
                                    }
                                }

                                GroupBox
                            {
                                Layout.fillWidth: true
                                // Grow to fit the content's real height (so 1/2-column reflow never overlaps)
                                implicitHeight: optGrid.implicitHeight + 40
                                label: RowLayout
                                {
                                    spacing: 8
                                    Rectangle
                                    {
                                        width: 4; height: 20; radius: 0; color: palette.green
                                    }
                                    Text
                                    {
                                        text: qsTr("KERNEL OPTIMIZATION SETTINGS"); color: palette.comment; font.bold: true; font.pixelSize: 13; font.letterSpacing: 1.1
                                    }
                                }
                                background: Rectangle
                                {
                                    color: palette.surface; radius: 0; border.color: palette.currentLine; y: 15
                                }

                                GridLayout
                                {
                                    id: optGrid
                                    // Anchor top/sides only — natural height, so rows never compress/overlap
                                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                                    anchors.leftMargin: 12; anchors.rightMargin: 12; anchors.topMargin: 24
                                    // Responsive: 3 columns when wide, reflow to 2 / 1 as the window shrinks
                                    columns: window.width < 720 ? 1 : (window.width < 980 ? 2 : 3)
                                    rowSpacing: 12; columnSpacing: 14
                                    CheckBox
                                    {
                                        id: ckLto; spacing: 8
                                        text: qsTr("Enable LTO"); checked: bridge.lto; onCheckedChanged: bridge.lto = checked
                                        hoverEnabled: true
                                        ToolTip.text: qsTr("Link Time Optimization — smaller, faster kernel but a much slower build")
                                        ToolTip.visible: hovered; ToolTip.delay: 500
                                        indicator: Rectangle
                                        {
                                            implicitWidth: 16; implicitHeight: 16; radius: 0
                                            x: 0; anchors.verticalCenter: parent.verticalCenter
                                            color: ckLto.checked ? palette.accent : palette.surface
                                            border.color: ckLto.checked ? palette.accent : (ckLto.hovered ? palette.borderHi : palette.currentLine)
                                            Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 0; color: palette.accentFg; visible: ckLto.checked }
                                        }
                                        contentItem: Text
                                        {
                                            text: ckLto.text; color: palette.fg; font.pixelSize: 13
                                            verticalAlignment: Text.AlignVCenter; leftPadding: ckLto.indicator.width + ckLto.spacing
                                        }
                                    }
                                    CheckBox
                                    {
                                        id: ckZfs; spacing: 8
                                        text: qsTr("ZFS Support"); checked: bridge.zfsSupport; onCheckedChanged: bridge.zfsSupport = checked
                                        hoverEnabled: true
                                        ToolTip.text: qsTr("Build the ZFS filesystem module against this kernel")
                                        ToolTip.visible: hovered; ToolTip.delay: 500
                                        indicator: Rectangle
                                        {
                                            implicitWidth: 16; implicitHeight: 16; radius: 0
                                            x: 0; anchors.verticalCenter: parent.verticalCenter
                                            color: ckZfs.checked ? palette.accent : palette.surface
                                            border.color: ckZfs.checked ? palette.accent : (ckZfs.hovered ? palette.borderHi : palette.currentLine)
                                            Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 0; color: palette.accentFg; visible: ckZfs.checked }
                                        }
                                        contentItem: Text
                                        {
                                            text: ckZfs.text; color: palette.fg; font.pixelSize: 13
                                            verticalAlignment: Text.AlignVCenter; leftPadding: ckZfs.indicator.width + ckZfs.spacing
                                        }
                                    }
                                    CheckBox
                                    {
                                        id: ckNvidia; spacing: 8
                                        text: qsTr("NVIDIA Support"); checked: bridge.nvidiaSupport; onCheckedChanged: bridge.nvidiaSupport = checked
                                        hoverEnabled: true
                                        ToolTip.text: qsTr("Build the proprietary NVIDIA driver against this kernel")
                                        ToolTip.visible: hovered; ToolTip.delay: 500
                                        indicator: Rectangle
                                        {
                                            implicitWidth: 16; implicitHeight: 16; radius: 0
                                            x: 0; anchors.verticalCenter: parent.verticalCenter
                                            color: ckNvidia.checked ? palette.accent : palette.surface
                                            border.color: ckNvidia.checked ? palette.accent : (ckNvidia.hovered ? palette.borderHi : palette.currentLine)
                                            Rectangle { anchors.centerIn: parent; width: 8; height: 8; radius: 0; color: palette.accentFg; visible: ckNvidia.checked }
                                        }
                                        contentItem: Text
                                        {
                                            text: ckNvidia.text; color: palette.fg; font.pixelSize: 13
                                            verticalAlignment: Text.AlignVCenter; leftPadding: ckNvidia.indicator.width + ckNvidia.spacing
                                        }
                                    }

                                    ColumnLayout
                                    {
                                        Text
                                        {
                                            text: qsTr("Preemption Mode"); color: palette.comment; font.pixelSize: 12
                                        }
                                        ComboBox
                                        {
                                            id: preemptCombo
                                            model: ["none", "voluntary", "full"]; Layout.fillWidth: true
                                            currentIndex: model.indexOf(bridge.preempt)
                                            onCurrentTextChanged: bridge.preempt = currentText
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; radius: 0; color: palette.bg; border.color: palette.currentLine
                                            }
                                            delegate: ItemDelegate
                                            {
                                                width: preemptCombo.width
                                                hoverEnabled: true
                                                padding: 10
                                                contentItem: Text
                                                {
                                                    text: modelData
                                                    color: highlighted ? palette.accentFg : palette.fg
                                                    font: preemptCombo.font
                                                    elide: Text.ElideRight
                                                    verticalAlignment: Text.AlignVCenter
                                                    Behavior on color
                                                    {
                                                        ColorAnimation
                                                        {
                                                            duration: 150
                                                        }
                                                    }
                                                }
                                                background: Rectangle
                                                {
                                                    color: highlighted ? palette.accent : (hovered ? palette.surfaceHi : "transparent")
                                                    radius: 0
                                                    Behavior on color
                                                    {
                                                        ColorAnimation
                                                        {
                                                            duration: 150
                                                        }
                                                    }
                                                }
                                            }
                                            popup: Popup
                                            {
                                                y: preemptCombo.height + 2
                                                width: preemptCombo.width
                                                implicitHeight: contentItem.implicitHeight
                                                padding: 4
                                                contentItem: ListView
                                                {
                                                    clip: true
                                                    implicitHeight: contentHeight
                                                    model: preemptCombo.popup.visible ? preemptCombo.delegateModel : null
                                                    currentIndex: preemptCombo.highlightedIndex
                                                    ScrollIndicator.vertical: ScrollIndicator
                                                    {
                                                    }
                                                }
                                                background: Rectangle
                                                {
                                                    color: palette.surface
                                                    border.color: palette.currentLine
                                                    border.width: 1
                                                    radius: 0
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout
                                    {
                                        Layout.columnSpan: Math.min(2, optGrid.columns)
                                        RowLayout
                                        {
                                            spacing: 8
                                            Text
                                            {
                                                text: qsTr("CPU Architecture Optimization"); color: palette.comment; font.pixelSize: 12
                                            }
                                            Rectangle
                                            {
                                                width: 65; height: 14; radius: 0; color: palette.green; opacity: 0.15
                                                Text
                                                {
                                                    anchors.centerIn: parent; text: bridge.detectedCpuLevel; color: palette.green; font.pixelSize: 8; font.bold: true
                                                }
                                            }
                                        }
                                        ComboBox
                                        {
                                            id: cpuOptCombo
                                            model: ["native", "x86-64-v1", "x86-64-v2", "x86-64-v3", "x86-64-v4", "generic"]; Layout.fillWidth: true
                                            currentIndex: model.indexOf(bridge.cpuOpt)
                                            onCurrentTextChanged: bridge.cpuOpt = currentText
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; radius: 0; color: palette.bg; border.color: palette.currentLine
                                            }
                                            delegate: ItemDelegate
                                            {
                                                width: cpuOptCombo.width
                                                hoverEnabled: true
                                                padding: 10
                                                contentItem: RowLayout
                                                {
                                                    spacing: 8
                                                    Text
                                                    {
                                                        text: modelData
                                                        // Using hardcoded color values based on palette definitions to avoid scope issues in ItemDelegate
                                                        color: (modelData === bridge.detectedCpuLevel) ? palette.accent : (highlighted ? palette.accentFg : palette.fg)
                                                        font.family: cpuOptCombo.font.family
                                                        font.pixelSize: cpuOptCombo.font.pixelSize
                                                        elide: Text.ElideRight
                                                        verticalAlignment: Text.AlignVCenter
                                                        font.bold: modelData === bridge.detectedCpuLevel
                                                    }
                                                    Item { Layout.fillWidth: true }
                                                    Text
                                                    {
                                                        text: "★"
                                                        color: palette.accent
                                                        visible: modelData === bridge.detectedCpuLevel
                                                        font.pixelSize: 10
                                                    }
                                                }
                                                background: Rectangle
                                                {
                                                    color: highlighted ? palette.accent : (hovered ? palette.surfaceHi : "transparent")
                                                    radius: 0
                                                }
                                            }
                                            popup: Popup
                                            {
                                                y: cpuOptCombo.height + 2
                                                width: cpuOptCombo.width
                                                implicitHeight: contentItem.implicitHeight
                                                padding: 4
                                                contentItem: ListView
                                                {
                                                    clip: true
                                                    implicitHeight: contentHeight
                                                    model: cpuOptCombo.popup.visible ? cpuOptCombo.delegateModel : null
                                                    currentIndex: cpuOptCombo.highlightedIndex
                                                    ScrollIndicator.vertical: ScrollIndicator
                                                    {
                                                    }
                                                }
                                                background: Rectangle
                                                {
                                                    color: palette.surface
                                                    border.color: palette.currentLine
                                                    border.width: 1
                                                    radius: 0
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout
                                    {
                                        Text
                                        {
                                            text: qsTr("Optimization Level"); color: palette.comment; font.pixelSize: 12
                                        }
                                        ComboBox
                                        {
                                            id: optLevelCombo
                                            model: ["O2", "O3", "Ofast"]; Layout.fillWidth: true
                                            currentIndex: model.indexOf(bridge.optLevel)
                                            onCurrentTextChanged: bridge.optLevel = currentText
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; radius: 0; color: palette.bg; border.color: palette.currentLine
                                            }
                                            delegate: ItemDelegate
                                            {
                                                width: optLevelCombo.width
                                                hoverEnabled: true
                                                padding: 10
                                                contentItem: Text
                                                {
                                                    text: modelData
                                                    color: highlighted ? palette.accentFg : palette.fg
                                                    font: optLevelCombo.font
                                                    elide: Text.ElideRight
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                background: Rectangle
                                                {
                                                    color: highlighted ? palette.accent : (hovered ? palette.surfaceHi : "transparent")
                                                    radius: 0
                                                }
                                            }
                                            popup: Popup
                                            {
                                                y: optLevelCombo.height + 2
                                                width: optLevelCombo.width
                                                implicitHeight: contentItem.implicitHeight
                                                padding: 4
                                                contentItem: ListView
                                                {
                                                    clip: true
                                                    implicitHeight: contentHeight
                                                    model: optLevelCombo.popup.visible ? optLevelCombo.delegateModel : null
                                                    currentIndex: optLevelCombo.highlightedIndex
                                                    ScrollIndicator.vertical: ScrollIndicator
                                                    {
                                                    }
                                                }
                                                background: Rectangle
                                                {
                                                    color: palette.surface
                                                    border.color: palette.currentLine
                                                    border.width: 1
                                                    radius: 0
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout
                                    {
                                        Layout.columnSpan: Math.min(2, optGrid.columns)
                                        Text
                                        {
                                            text: qsTr("ZRAM Compression"); color: palette.comment; font.pixelSize: 12
                                        }
                                        ComboBox
                                        {
                                            id: zramCombo
                                            model: ["none", "lzo-rle", "lz4", "zstd", "deflate", "gzip"]; Layout.fillWidth: true
                                            currentIndex: model.indexOf(bridge.zramType)
                                            onCurrentTextChanged: bridge.zramType = currentText
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; radius: 0; color: palette.bg; border.color: palette.currentLine
                                            }
                                            delegate: ItemDelegate
                                            {
                                                width: zramCombo.width
                                                hoverEnabled: true
                                                padding: 10
                                                contentItem: Text
                                                {
                                                    text: modelData
                                                    color: highlighted ? palette.accentFg : palette.fg
                                                    font: zramCombo.font
                                                    elide: Text.ElideRight
                                                    verticalAlignment: Text.AlignVCenter
                                                }
                                                background: Rectangle
                                                {
                                                    color: highlighted ? palette.accent : (hovered ? palette.surfaceHi : "transparent")
                                                    radius: 0
                                                }
                                            }
                                            popup: Popup
                                            {
                                                y: zramCombo.height + 2
                                                width: zramCombo.width
                                                implicitHeight: contentItem.implicitHeight
                                                padding: 4
                                                contentItem: ListView
                                                {
                                                    clip: true
                                                    implicitHeight: contentHeight
                                                    model: zramCombo.popup.visible ? zramCombo.delegateModel : null
                                                    currentIndex: zramCombo.highlightedIndex
                                                    ScrollIndicator.vertical: ScrollIndicator
                                                    {
                                                    }
                                                }
                                                background: Rectangle
                                                {
                                                    color: palette.surface
                                                    border.color: palette.currentLine
                                                    border.width: 1
                                                    radius: 0
                                                }
                                            }
                                        }
                                    }

                                    ColumnLayout
                                    {
                                        Layout.columnSpan: Math.min(3, optGrid.columns)
                                        Text
                                        {
                                            text: qsTr("Extra KCFLAGS"); color: palette.comment; font.pixelSize: 12
                                        }
                                        TextField
                                        {
                                            id: extraFlagsField
                                            Layout.fillWidth: true
                                            text: bridge.extraFlags
                                            placeholderText: qsTr("-funroll-loops -fomit-frame-pointer")
                                            onTextChanged: bridge.extraFlags = text
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; radius: 0; color: palette.bg; border.color: palette.currentLine
                                            }
                                            font.pixelSize: 12
                                            color: palette.fg
                                            cursorVisible: true
                                        }
                                    }
                                }
                            }

                            // Separator — flat 1px divider (borders-only depth)
                            Rectangle
                            {
                                Layout.fillWidth: true
                                height: 1
                                Layout.topMargin: 6
                                Layout.bottomMargin: 6
                                color: palette.currentLine
                            }

                            GroupBox
                            {
                                Layout.fillWidth: true
                                label: RowLayout
                                {
                                    spacing: 8
                                    Rectangle
                                    {
                                        width: 4; height: 20; radius: 0; color: palette.purple
                                    }
                                    Text
                                    {
                                        text: qsTr("BUILD ORCHESTRATION"); color: palette.comment; font.bold: true; font.pixelSize: 13; font.letterSpacing: 1.1
                                    }
                                }
                                background: Rectangle
                                {
                                    color: palette.surface; radius: 0; border.color: palette.currentLine; y: 15
                                }

                                ColumnLayout
                                {
                                    anchors.fill: parent; anchors.margins: 12; anchors.topMargin: 24; spacing: 12
                                    RowLayout
                                    {
                                        spacing: 10
                                        TextField
                                        {
                                            id: templateName; text: bridge.targetTemplate; Layout.fillWidth: true; color: palette.fg; verticalAlignment: TextInput.AlignVCenter
                                            onTextChanged: bridge.targetTemplate = text
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; color: palette.bg; radius: 0; border.color: palette.currentLine
                                            }
                                        }
                                        Button
                                        {
                                            id: buildBtn; text: qsTr("Build Kernel"); Layout.preferredWidth: 150; enabled: !bridge.busy
                                            contentItem: Text
                                            {
                                                text: buildBtn.text; font.bold: true; font.pixelSize: 13; color: palette.accentFg; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; radius: 0; color: buildBtn.enabled ? palette.purple : palette.currentLine
                                                Rectangle
                                                {
                                                    anchors.fill: parent; radius: 0; color: "white"; opacity: buildBtn.pressed ? 0.2 : (buildBtn.hovered ? 0.1 : 0.0)
                                                }
                                            }
                                            onClicked: bridge.buildKernel(templateName.text)
                                        }
                                    }
                                    RowLayout
                                    {
                                        spacing: 10
                                        TextField
                                        {
                                            id: exportPath; placeholderText: qsTr("Export destination path..."); Layout.fillWidth: true; color: palette.fg; verticalAlignment: TextInput.AlignVCenter
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; color: palette.bg; radius: 0; border.color: palette.currentLine
                                            }
                                        }
                                        Button
                                        {
                                            id: exportBtn
                                            text: qsTr("Export XBPS")
                                            Layout.preferredWidth: 150
                                            hoverEnabled: true
                                            ToolTip.text: qsTr("Copy the built .xbps package(s) to the destination folder above")
                                            ToolTip.visible: hovered; ToolTip.delay: 500
                                            enabled: exportPath.text.trim().length > 0 && !bridge.busy

                                            contentItem: Text
                                            {
                                                text: exportBtn.text
                                                font.bold: true
                                                font.pixelSize: 13
                                                color: exportBtn.enabled ? palette.accentFg : palette.comment
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }

                                            background: Rectangle
                                            {
                                                implicitHeight: 32
                                                radius: 0
                                                color: exportBtn.enabled ? palette.purple : palette.currentLine

                                                // Interactive overlay (only active when button is enabled)
                                                Rectangle
                                                {
                                                    anchors.fill: parent
                                                    radius: 0
                                                    color: "white"
                                                    opacity: exportBtn.enabled ? (exportBtn.pressed ? 0.2 : (exportBtn.hovered ? 0.1 : 0.0)) : 0.0
                                                    visible: exportBtn.enabled
                                                }
                                            }

                                            // This will only fire if enabled is true
                                            onClicked:
                                            {
                                                if (exportPath.text.trim() !== "")
                                                {
                                                    bridge.exportPackages(exportPath.text.trim())
                                                }
                                            }
                                        }
                                    }
                                    RowLayout
                                    {
                                        spacing: 12
                                        Button
                                        {
                                            id: sBtn; text: qsTr("Save Configuration"); Layout.fillWidth: true
                                            contentItem: Text
                                            {
                                                text: sBtn.text; font.bold: true; font.pixelSize: 13; color: palette.fg; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; radius: 0; color: palette.surface; border.color: palette.green; border.width: 1
                                                Rectangle
                                                {
                                                    anchors.fill: parent; radius: 0; color: palette.green; opacity: sBtn.pressed ? 0.2 : (sBtn.hovered ? 0.1 : 0.0)
                                                }
                                            }
                                            onClicked: bridge.saveConfig()
                                        }
                                        Button
                                        {
                                            id: lBtn; text: qsTr("Load Configuration"); Layout.fillWidth: true
                                            contentItem: Text
                                            {
                                                text: lBtn.text; font.bold: true; font.pixelSize: 13; color: palette.fg; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle
                                            {
                                                implicitHeight: 32; radius: 0; color: palette.surface; border.color: palette.purple; border.width: 1
                                                Rectangle
                                                {
                                                    anchors.fill: parent; radius: 0; color: palette.purple; opacity: lBtn.pressed ? 0.2 : (lBtn.hovered ? 0.1 : 0.0)
                                                }
                                            }
                                            onClicked: bridge.loadConfig()
                                        }
                                    }
                                }
                            }
                            Item
                            {
                                Layout.fillHeight: true
                            }
                        }
                    }
                }

                    // Footer divider
                    Rectangle
                    {
                        Layout.fillWidth: true; Layout.topMargin: 6
                        height: 1; color: palette.currentLine
                    }

                    // Footer
                    RowLayout
                    {
                        Layout.fillWidth: true
                        ColumnLayout
                        {
                            spacing: 2
                            Text
                            {
                                text: qsTr("STATUS: %1").arg(bridge.busy ? qsTr("BUSY") : qsTr("READY")); color: bridge.busy ? palette.accent : palette.comment; font.bold: true; font.pixelSize: 10; font.letterSpacing: 0.5
                            }
                            Text
                            {
                                text: bridge.statusMessage; color: bridge.statusIsError ? palette.pink : palette.comment; font.pixelSize: 11; visible: bridge.statusMessage !== ""
                            }
                        }
                        Item
                        {
                            Layout.fillWidth: true
                        }
                        Text
                        {
                            text: qsTr("Neko-Kernel-Manager v1.2.0"); color: palette.comment; font.pixelSize: 10; opacity: 0.5
                        }
                    }
                }
            }
        }
    }

    Popup
    {
        id: logModal
        x: (window.width - width) / 2; y: (window.height - height) / 2
        width: 600; height: 400; modal: true; focus: true
        background: Rectangle
        {
            color: palette.surface; radius: 0; border.color: palette.currentLine; border.width: 1
        }

        ColumnLayout
        {
            anchors.fill: parent; anchors.margins: 10; spacing: 8
            RowLayout
            {
                Layout.fillWidth: true
                Text
                {
                    text: qsTr("System Logs & Operations"); color: palette.fg; font.bold: true; font.pixelSize: 14
                }
                Item
                {
                    Layout.fillWidth: true
                }
                Button
                {
                    text: qsTr("Clear"); onClicked: bridge.clearLogs()
                    background: Rectangle
                    {
                        implicitWidth: 60; implicitHeight: 22; radius: 0; color: palette.bg; border.color: palette.currentLine
                    }
                    contentItem: Text
                    {
                        text: parent.text; color: palette.comment; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
                Button
                {
                    text: qsTr("Close"); onClicked: logModal.close()
                    background: Rectangle
                    {
                        implicitWidth: 60; implicitHeight: 22; radius: 0; color: palette.accent
                    }
                    contentItem: Text
                    {
                        text: parent.text; color: palette.accentFg; font.bold: true; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter
                    }
                }
            }
            Rectangle
            {
                Layout.fillWidth: true; Layout.fillHeight: true; color: palette.bg; radius: 0; border.color: palette.currentLine
                ScrollView
                {
                    anchors.fill: parent; anchors.margins: 8; clip: true
                    TextArea
                    {
                        id: logArea
                        text: bridge.logs; readOnly: true; color: palette.textSoft
                        font.family: "0xProto Nerd Font Mono"; font.pixelSize: 11
                        textFormat: TextEdit.RichText
                        background: null
                        wrapMode: TextEdit.Wrap
                        cursorVisible: false
                        onTextChanged: {
                            cursorPosition = text.length
                            if (parent && parent.contentHeight) {
                                parent.contentY = parent.contentHeight
                            }
                        }
                    }
                }
            }
        }
    }

    Component.onCompleted: bridge.fetchKernelOrgVersions("stable")
}
