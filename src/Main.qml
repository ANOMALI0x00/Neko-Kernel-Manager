import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Controls.Material 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: window
    visible: true
    width: 1100
    height: 800
    minimumWidth: 900
    minimumHeight: 700
    title: "Neko Kernel Manager"
    Material.theme: Material.Dark
    Material.accent: palette.green
    Material.primary: palette.bg
    
    color: "transparent"
    flags: Qt.Window | Qt.FramelessWindowHint

    QtObject {
        id: palette
        readonly property color bg: "#1e1e2e"
        readonly property color surface: "#25263a"
        readonly property color currentLine: "#313244"
        readonly property color selection: "#45475a"
        readonly property color fg: "#cdd6f4"
        readonly property color comment: "#7f849c"
        readonly property color cyan: "#89dceb"
        readonly property color green: "#a6e3a1"
        readonly property color orange: "#fab387"
        readonly property color pink: "#f5c2e7"
        readonly property color purple: "#cba6f7"
    }

    QtObject {
        id: dracula
        readonly property color bg: "#282a36"
        readonly property color purple: "#bd93f9"
        readonly property color red: "#ff5555"
        readonly property color yellow: "#f1fa8c"
        readonly property color green: "#50fa7b"
    }

    // Main Border and Background
    Rectangle {
        anchors.fill: parent
        color: palette.bg
        border.color: dracula.purple
        border.width: 2
        radius: 4

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 2
            spacing: 0

            // Custom Dracula Title Bar
            Rectangle {
                Layout.fillWidth: true
                height: 35
                color: dracula.bg
                radius: 4
                
                // Keep bottom corners square for the title bar
                Rectangle {
                    anchors.bottom: parent.bottom
                    width: parent.width; height: 10
                    color: parent.color
                    visible: true
                }

                MouseArea {
                    anchors.fill: parent
                    property point lastMousePos
                    onPressed: lastMousePos = Qt.point(mouse.x, mouse.y)
                    onPositionChanged: {
                        var delta = Qt.point(mouse.x - lastMousePos.x, mouse.y - lastMousePos.y)
                        window.x += delta.x
                        window.y += delta.y
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 15
                    anchors.rightMargin: 10
                    
                    Text {
                        text: "󰄛  Neko Kernel Manager"
                        color: dracula.purple
                        font.pixelSize: 12
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    Row {
                        spacing: 10
                        Layout.alignment: Qt.AlignVCenter
                        
                        // Minimize
                        Rectangle {
                            width: 12; height: 12; radius: 6; color: dracula.green
                            MouseArea { anchors.fill: parent; onClicked: window.showMinimized() }
                        }
                        // Maximize
                        Rectangle {
                            width: 12; height: 12; radius: 6; color: dracula.yellow
                            MouseArea { anchors.fill: parent; onClicked: window.visibility === Window.Maximized ? window.showNormal() : window.showMaximized() }
                        }
                        // Close
                        Rectangle {
                            width: 12; height: 12; radius: 6; color: dracula.red
                            MouseArea { anchors.fill: parent; onClicked: window.close() }
                        }
                    }
                }
            }

            // Original Catppuccin Interface
            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Rectangle {
                    anchors.fill: parent
                    gradient: Gradient {
                        GradientStop { position: 0; color: palette.surface }
                        GradientStop { position: 1; color: palette.bg }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 25
                    spacing: 20

                    // Top Bar
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 20

                        Row {
                            spacing: 12
                            Item {
                                width: 56; height: 56; anchors.verticalCenter: parent.verticalCenter
                                Image {
                                    id: logoImage; source: "qrc:/neko/Data/logo.png"
                                    anchors.fill: parent; sourceSize: Qt.size(112, 112); smooth: true
                                }
                            }

                            Canvas {
                                id: mainTitle; width: 280; height: 40; anchors.verticalCenter: parent.verticalCenter
                                property color color1: palette.green
                                property color color2: palette.purple
                                onPaint: {
                                    var ctx = getContext("2d"); ctx.clearRect(0, 0, width, height);
                                    var gradient = ctx.createLinearGradient(0, 0, width, 0);
                                    gradient.addColorStop(0.0, color1); gradient.addColorStop(1.0, color2);
                                    ctx.fillStyle = gradient; ctx.font = "bold 24px '0xProto Nerd Font'";
                                    ctx.textBaseline = "middle"; ctx.fillText("Neko Kernel Manager", 0, height / 2);                                }
                                Component.onCompleted: requestPaint()
                            }
                        }

                        RowLayout {
                            spacing: 30; Layout.leftMargin: 60
                            Repeater {
                                model: ["XBPS Kernels", "Downloads", "Build & Config"]
                                Text {
                                    text: modelData
                                    color: mainStack.currentIndex === index ? palette.green : palette.comment
                                    font.pixelSize: 15; font.bold: mainStack.currentIndex === index
                                    Rectangle {
                                        anchors.top: parent.bottom; anchors.topMargin: 4
                                        width: parent.width; height: 2; color: palette.green
                                        visible: mainStack.currentIndex === index
                                    }
                                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: mainStack.currentIndex = index }
                                }
                            }
                        }
                        
                        Item { Layout.fillWidth: true }

                        Rectangle {
                            width: 14; height: 14; radius: 7
                            color: bridge.busy ? palette.orange : palette.green
                            Rectangle {
                                anchors.centerIn: parent; width: 24; height: 24; radius: 12
                                color: parent.color; opacity: 0.2
                                visible: bridge.busy
                            }
                        }

                        Button {
                            id: logBtn; Layout.leftMargin: 5
                            contentItem: Text { text: "󰋗"; color: palette.fg; font.pixelSize: 18; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                            background: Rectangle { implicitWidth: 32; implicitHeight: 32; radius: 16; color: "transparent" }
                            onClicked: logModal.open()
                        }
                    }

                    StackLayout {
                        id: mainStack
                        Layout.fillWidth: true; Layout.fillHeight: true

                        // 0: XBPS Kernels View
                        ColumnLayout {
                            spacing: 12
                            Layout.fillWidth: true; Layout.fillHeight: true
                            RowLayout {
                                Layout.fillWidth: true
                                ColumnLayout {
                                    spacing: 2
                                    Text { text: "Official Void Repositories"; color: palette.fg; font.bold: true; font.pixelSize: 16 }
                                    Text { 
                                        text: "Manage binary kernels and custom manual installations. Clean old kernels with vkpurge."
                                        color: palette.comment; font.pixelSize: 10; font.italic: true 
                                    }
                                }
                                Item { Layout.fillWidth: true }

                                Button {
                                    id: purgeBtn; text: "Purge Old Kernels"; enabled: !bridge.busy
                                    contentItem: Text { text: purgeBtn.text; font.bold: true; font.pixelSize: 11; color: palette.bg; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                    background: Rectangle { 
                                        implicitHeight: 32; implicitWidth: 140; radius: 8; color: purgeBtn.enabled ? palette.orange : palette.currentLine 
                                    }
                                    onClicked: bridge.vkpurge()
                                }
                            }

                            ScrollView {
                                Layout.fillWidth: true; Layout.fillHeight: true
                                clip: true

                                GridView {
                                    id: kernelGrid
                                    width: parent.width; height: contentHeight
                                    Layout.margins: 10
                                    cellWidth: width > 100 ? (width >= 600 ? width / 4 : (width >= 400 ? width / 3 : (width >= 300 ? width / 2 : width))) : 140
                                    cellHeight: 165
                                    model: bridge.kernels
                                    interactive: false // ScrollView handles interaction
                                    
                                    delegate: Rectangle {
                                        id: kernelDelegate
                                        width: kernelGrid.cellWidth - 10; height: kernelGrid.cellHeight - 10
                                        radius: 10
                                        color: palette.surface
                                        border.color: bridge.targetTemplate === modelData.name ? palette.green : (hoverHandler.hovered ? palette.purple : (modelData.type === "manual" ? palette.orange : palette.currentLine))
                                        border.width: bridge.targetTemplate === modelData.name ? 2 : 1

                                        HoverHandler { id: hoverHandler }

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: {
                                                // Extract version suffix from name (e.g. 6.6 from linux6.6)
                                                var verMatch = modelData.name.match(/linux(\d+\.\d+)/)
                                                var verSuffix = verMatch ? verMatch[1] : modelData.name.replace("linux", "")
                                                
                                                var category = modelData.category.toLowerCase()
                                                var finalName = "neko-kernel"
                                                
                                                if (category === "longterm") {
                                                    finalName += "-lts-" + verSuffix
                                                } else if (category !== "stable" && category !== "custom") {
                                                    finalName += "-" + category + "-" + verSuffix
                                                } else {
                                                    finalName += verSuffix
                                                }
                                                
                                                bridge.targetTemplate = finalName
                                                bridge.sourceTemplate = modelData.name
                                            }
                                        }

                                        ColumnLayout {
                                            anchors.centerIn: parent
                                            width: parent.width - 16
                                            spacing: 6
                                            
                                            Rectangle {
                                                Layout.alignment: Qt.AlignHCenter
                                                width: 38; height: 38; radius: 19
                                                color: palette.bg
                                                Image {
                                                    anchors.centerIn: parent
                                                    source: "qrc:/neko/Data/logo.png"
                                                    width: 22; height: 22; fillMode: Image.PreserveAspectFit
                                                }
                                                Rectangle {
                                                    anchors.bottom: parent.bottom; anchors.right: parent.right
                                                    width: 12; height: 12; radius: 6; color: modelData.type === "manual" ? palette.orange : palette.green
                                                    border.color: palette.bg; border.width: 2
                                                }
                                            }

                                            ColumnLayout {
                                                Layout.fillWidth: true; spacing: 1
                                                Text { 
                                                    text: modelData.name
                                                    color: palette.fg; font.bold: true; font.pixelSize: 11
                                                    Layout.alignment: Qt.AlignHCenter; Layout.fillWidth: true
                                                    horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
                                                }
                                                Text {
                                                    text: modelData.type === "manual" ? "Custom Installation" : modelData.version
                                                    color: modelData.type === "manual" ? palette.orange : palette.comment
                                                    font.pixelSize: 9
                                                    Layout.alignment: Qt.AlignHCenter; Layout.fillWidth: true
                                                    horizontalAlignment: Text.AlignHCenter; elide: Text.ElideRight
                                                }
                                            }

                                            Item {
                                                Layout.alignment: Qt.AlignHCenter
                                                Layout.preferredWidth: 85; Layout.preferredHeight: 26
                                                property bool isActive: modelData.installed && (bridge.activeKernelVersion === modelData.version || bridge.activeKernelVersion.includes(modelData.version))

                                                // Running Indicator (Non-actionable)
                                                Rectangle {
                                                    anchors.fill: parent
                                                    visible: parent.isActive
                                                    radius: 5; color: "transparent"; border.color: "#fab387"; border.width: 1
                                                    Text {
                                                        anchors.centerIn: parent
                                                        text: "Running"; font.bold: true; font.pixelSize: 10; color: "#fab387"
                                                    }
                                                }

                                                // Action Button (Install/Remove)
                                                Button {
                                                    id: actionBtn
                                                    anchors.fill: parent
                                                    visible: !parent.isActive
                                                    text: modelData.installed ? "Remove" : "Install"
                                                    enabled: !bridge.busy
                                                    contentItem: Text { 
                                                        text: actionBtn.text; font.bold: true; font.pixelSize: 10
                                                        color: !actionBtn.enabled ? palette.comment : palette.bg
                                                        horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter 
                                                    }
                                                    background: Rectangle { 
                                                        radius: 5
                                                        color: !actionBtn.enabled ? palette.currentLine : (modelData.installed ? palette.pink : palette.green)
                                                    }
                                                    onClicked: modelData.installed ? bridge.removeKernel(modelData.name) : bridge.installKernel(modelData.name)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        // 1: Downloads
                        ColumnLayout {
                            spacing: 25
                            Layout.fillWidth: true; Layout.fillHeight: true
                            GroupBox {
                                label: RowLayout {
                                    spacing: 12
                                    Rectangle { width: 4; height: 20; radius: 2; color: palette.green }
                                    Text { text: "KERNEL.ORG DOWNLOAD"; color: palette.green; font.bold: true; font.pixelSize: 13; font.letterSpacing: 1.1 }
                                }
                                Layout.fillWidth: true
                                background: Rectangle { color: palette.surface; radius: 12; border.color: palette.currentLine; y: 15 }
                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 20; anchors.topMargin: 30; spacing: 20
                                    RowLayout {
                                        spacing: 20
                                        ColumnLayout {
                                            Text { text: "Variant"; color: palette.comment; font.pixelSize: 12 }
                                            ComboBox { 
                                                id: kernelVariant; model: ["stable", "lts", "mainline", "all"]; Layout.preferredWidth: 160
                                                onCurrentTextChanged: bridge.fetchKernelOrgVersions(currentText)
                                                background: Rectangle { implicitHeight: 40; radius: 10; color: palette.bg; border.color: palette.currentLine }
                                                delegate: ItemDelegate {
                                                    width: kernelVariant.width
                                                    hoverEnabled: true
                                                    padding: 10
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: highlighted ? "#1e1e2e" : (hovered ? "#a6e3a1" : "#cdd6f4")
                                                        font: kernelVariant.font
                                                        elide: Text.ElideRight
                                                        verticalAlignment: Text.AlignVCenter
                                                        Behavior on color { ColorAnimation { duration: 150 } }
                                                    }
                                                    background: Rectangle {
                                                        color: highlighted ? "#a6e3a1" : (hovered ? "#313244" : "transparent")
                                                        radius: 4
                                                        Behavior on color { ColorAnimation { duration: 150 } }
                                                    }
                                                }
                                                popup: Popup {
                                                    y: kernelVariant.height + 2
                                                    width: kernelVariant.width
                                                    implicitHeight: Math.min(contentItem.implicitHeight, 400)
                                                    padding: 4
                                                    contentItem: ListView {
                                                        clip: true
                                                        implicitHeight: contentHeight
                                                        model: kernelVariant.popup.visible ? kernelVariant.delegateModel : null
                                                        currentIndex: kernelVariant.highlightedIndex
                                                        ScrollIndicator.vertical: ScrollIndicator { }
                                                    }
                                                    background: Rectangle {
                                                        color: "#1e1e2e"
                                                        border.color: "#a6e3a1"
                                                        border.width: 1
                                                        radius: 8
                                                    }
                                                }
                                            }
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Text { text: "Version"; color: palette.comment; font.pixelSize: 12 }
                                            ComboBox { 
                                                id: kernelVerCombo; model: bridge.kernelOrgVersions; Layout.fillWidth: true
                                                background: Rectangle { implicitHeight: 40; radius: 10; color: palette.bg; border.color: palette.currentLine }
                                                delegate: ItemDelegate {
                                                    width: kernelVerCombo.width
                                                    hoverEnabled: true
                                                    padding: 10
                                                    contentItem: Text {
                                                        text: modelData
                                                        color: highlighted ? "#1e1e2e" : (hovered ? "#a6e3a1" : "#cdd6f4")
                                                        font: kernelVerCombo.font
                                                        elide: Text.ElideRight
                                                        verticalAlignment: Text.AlignVCenter
                                                        Behavior on color { ColorAnimation { duration: 150 } }
                                                    }
                                                    background: Rectangle {
                                                        color: highlighted ? "#a6e3a1" : (hovered ? "#313244" : "transparent")
                                                        radius: 4
                                                        Behavior on color { ColorAnimation { duration: 150 } }
                                                    }
                                                }
                                                popup: Popup {
                                                    y: kernelVerCombo.height + 2
                                                    width: kernelVerCombo.width
                                                    implicitHeight: Math.min(contentItem.implicitHeight, 400)
                                                    padding: 4
                                                    contentItem: ListView {
                                                        clip: true
                                                        implicitHeight: contentHeight
                                                        model: kernelVerCombo.popup.visible ? kernelVerCombo.delegateModel : null
                                                        currentIndex: kernelVerCombo.highlightedIndex
                                                        ScrollIndicator.vertical: ScrollIndicator { }
                                                    }
                                                    background: Rectangle {
                                                        color: "#1e1e2e"
                                                        border.color: "#a6e3a1"
                                                        border.width: 1
                                                        radius: 8
                                                    }
                                                }
                                            }
                                        }
                                    }
                                    Button { 
                                        id: dlBtn; text: "Download Now"; Layout.fillWidth: true; enabled: kernelVerCombo.currentText !== "" && !bridge.busy
                                        contentItem: Text { text: dlBtn.text; font.bold: true; font.pixelSize: 13; color: palette.bg; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                        background: Rectangle { 
                                            implicitHeight: 40; radius: 10; color: dlBtn.enabled ? palette.green : palette.currentLine 
                                            Rectangle { anchors.fill: parent; radius: 10; color: "white"; opacity: dlBtn.pressed ? 0.2 : (dlBtn.hovered ? 0.1 : 0.0) }
                                        }
                                        onClicked: bridge.downloadKernelOrg(kernelVariant.currentText, kernelVerCombo.currentText) 
                                    }
                                }
                            }

                            // Visual Separator with Gradient Effect
                            Rectangle {
                                Layout.fillWidth: true
                                height: 2
                                Layout.topMargin: 10
                                Layout.bottomMargin: 10
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: "transparent" }
                                    GradientStop { position: 0.2; color: palette.green }
                                    GradientStop { position: 0.5; color: palette.purple }
                                    GradientStop { position: 0.8; color: palette.green }
                                    GradientStop { position: 1.0; color: "transparent" }
                                }
                            }

                            GroupBox {
                                label: RowLayout {
                                    spacing: 12
                                    Rectangle { width: 4; height: 20; radius: 2; color: palette.purple }
                                    Text { text: "CUSTOM TARBALL / GIT REPOSITORY"; color: palette.purple; font.bold: true; font.pixelSize: 13; font.letterSpacing: 1.1 }
                                }
                                Layout.fillWidth: true
                                background: Rectangle { color: palette.surface; radius: 12; border.color: palette.currentLine; y: 15 }

                                RowLayout {
                                    anchors.fill: parent; anchors.margins: 20; anchors.topMargin: 30; spacing: 15
                                    TextField {
                                        id: gitUrl
                                        placeholderText: "Git repo URL or tarball URL"
                                        Layout.fillWidth: true; color: palette.fg; verticalAlignment: TextInput.AlignVCenter
                                        background: Rectangle { implicitHeight: 40; color: palette.bg; radius: 10; border.color: palette.currentLine }
                                    }
                                    Button {
                                        id: cloneBtn
                                        text: "Clone / Download"
                                        Layout.preferredWidth: 160
                                        enabled: gitUrl.text.trim().length > 0 && !bridge.busy

                                        contentItem: Text { 
                                            text: cloneBtn.text
                                            font.bold: true
                                            font.pixelSize: 13
                                            color: cloneBtn.enabled ? palette.bg : palette.comment
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter 
                                        }

                                        background: Rectangle { 
                                            implicitHeight: 40
                                            radius: 10
                                            color: cloneBtn.enabled ? palette.purple : palette.currentLine

                                            Rectangle { 
                                                anchors.fill: parent
                                                radius: 10
                                                color: "white"
                                                opacity: cloneBtn.enabled ? (cloneBtn.pressed ? 0.2 : (cloneBtn.hovered ? 0.1 : 0.0)) : 0.0
                                                visible: cloneBtn.enabled
                                            }
                                        }

                                        onClicked: bridge.cloneCustomGit(gitUrl.text.trim())
                                    }
                                }
                            }                            Item { Layout.fillHeight: true }
                        }
                        // 2: Build & Config
                        ColumnLayout {
                            spacing: 30
                            Layout.fillWidth: true; Layout.fillHeight: true

                            GroupBox {                                Layout.fillWidth: true
                                label: RowLayout {
                                    spacing: 12
                                    Rectangle { width: 4; height: 20; radius: 2; color: palette.green }
                                    Text { text: "KERNEL OPTIMIZATION SETTINGS"; color: palette.green; font.bold: true; font.pixelSize: 13; font.letterSpacing: 1.1 }
                                }
                                background: Rectangle { color: palette.surface; radius: 12; border.color: palette.currentLine; y: 15 }

                                GridLayout {
                                    anchors.fill: parent; anchors.margins: 20; anchors.topMargin: 30
                                    columns: 3; rowSpacing: 20; columnSpacing: 25
                                    CheckBox { text: "Enable LTO"; checked: bridge.lto; onCheckedChanged: bridge.lto = checked }
                                    CheckBox { text: "ZFS Support"; checked: bridge.zfsSupport; onCheckedChanged: bridge.zfsSupport = checked }
                                    CheckBox { text: "NVIDIA Support"; checked: bridge.nvidiaSupport; onCheckedChanged: bridge.nvidiaSupport = checked }

                                    ColumnLayout {
                                        Text { text: "Preemption Mode"; color: palette.comment; font.pixelSize: 12 }
                                        ComboBox {
                                            id: preemptCombo
                                            model: ["none", "voluntary", "full"]; Layout.fillWidth: true
                                            currentIndex: model.indexOf(bridge.preempt)
                                            onCurrentTextChanged: bridge.preempt = currentText
                                            background: Rectangle { implicitHeight: 40; radius: 10; color: palette.bg; border.color: palette.currentLine }
                                            delegate: ItemDelegate {
                                                width: preemptCombo.width
                                                hoverEnabled: true
                                                padding: 10
                                                contentItem: Text {
                                                    text: modelData
                                                    color: highlighted ? "#1e1e2e" : (hovered ? "#a6e3a1" : "#cdd6f4")
                                                    font: preemptCombo.font
                                                    elide: Text.ElideRight
                                                    verticalAlignment: Text.AlignVCenter
                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                }
                                                background: Rectangle {
                                                    color: highlighted ? "#a6e3a1" : (hovered ? "#313244" : "transparent")
                                                    radius: 4
                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                }
                                            }
                                            popup: Popup {
                                                y: preemptCombo.height + 2
                                                width: preemptCombo.width
                                                implicitHeight: contentItem.implicitHeight
                                                padding: 4
                                                contentItem: ListView {
                                                    clip: true
                                                    implicitHeight: contentHeight
                                                    model: preemptCombo.popup.visible ? preemptCombo.delegateModel : null
                                                    currentIndex: preemptCombo.highlightedIndex
                                                    ScrollIndicator.vertical: ScrollIndicator { }
                                                }
                                                background: Rectangle {
                                                    color: "#1e1e2e"
                                                    border.color: "#a6e3a1"
                                                    border.width: 1
                                                    radius: 8
                                                }
                                            }
                                        }
                                        }

                                        ColumnLayout {
                                        Layout.columnSpan: 2
                                        Text { text: "CPU Architecture Optimization"; color: palette.comment; font.pixelSize: 12 }
                                        ComboBox {
                                            id: cpuOptCombo
                                            model: ["generic", "native", "zen2", "zen3", "skylake", "broadwell", "haswell"]; Layout.fillWidth: true
                                            currentIndex: model.indexOf(bridge.cpuOpt)
                                            onCurrentTextChanged: bridge.cpuOpt = currentText
                                            background: Rectangle { implicitHeight: 40; radius: 10; color: palette.bg; border.color: palette.currentLine }
                                            delegate: ItemDelegate {
                                                width: cpuOptCombo.width
                                                hoverEnabled: true
                                                padding: 10
                                                contentItem: Text {
                                                    text: modelData
                                                    color: highlighted ? "#1e1e2e" : (hovered ? "#a6e3a1" : "#cdd6f4")
                                                    font: cpuOptCombo.font
                                                    elide: Text.ElideRight
                                                    verticalAlignment: Text.AlignVCenter
                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                }
                                                background: Rectangle {
                                                    color: highlighted ? "#a6e3a1" : (hovered ? "#313244" : "transparent")
                                                    radius: 4
                                                    Behavior on color { ColorAnimation { duration: 150 } }
                                                }
                                            }
                                            popup: Popup {
                                                y: cpuOptCombo.height + 2
                                                width: cpuOptCombo.width
                                                implicitHeight: contentItem.implicitHeight
                                                padding: 4
                                                contentItem: ListView {
                                                    clip: true
                                                    implicitHeight: contentHeight
                                                    model: cpuOptCombo.popup.visible ? cpuOptCombo.delegateModel : null
                                                    currentIndex: cpuOptCombo.highlightedIndex
                                                    ScrollIndicator.vertical: ScrollIndicator { }
                                                }
                                                background: Rectangle {
                                                    color: "#1e1e2e"
                                                    border.color: "#a6e3a1"
                                                    border.width: 1
                                                    radius: 8
                                                }
                                            }
                                        }
                                        }                                }
                            }

                            // Visual Separator with Gradient Effect
                            Rectangle {
                                Layout.fillWidth: true
                                height: 2
                                Layout.topMargin: 10
                                Layout.bottomMargin: 10
                                gradient: Gradient {
                                    orientation: Gradient.Horizontal
                                    GradientStop { position: 0.0; color: "transparent" }
                                    GradientStop { position: 0.2; color: palette.green }
                                    GradientStop { position: 0.5; color: palette.purple }
                                    GradientStop { position: 0.8; color: palette.green }
                                    GradientStop { position: 1.0; color: "transparent" }
                                }
                            }

                            GroupBox {
                                Layout.fillWidth: true
                                label: RowLayout {
                                    spacing: 12
                                    Rectangle { width: 4; height: 20; radius: 2; color: palette.purple }
                                    Text { text: "BUILD ORCHESTRATION"; color: palette.purple; font.bold: true; font.pixelSize: 13; font.letterSpacing: 1.1 }
                                }
                                background: Rectangle { color: palette.surface; radius: 12; border.color: palette.currentLine; y: 15 }

                                ColumnLayout {
                                    anchors.fill: parent; anchors.margins: 20; anchors.topMargin: 30; spacing: 20
                                    RowLayout {
                                        spacing: 15
                                        TextField { 
                                            id: templateName; text: bridge.targetTemplate; Layout.fillWidth: true; color: palette.fg; verticalAlignment: TextInput.AlignVCenter
                                            onTextChanged: bridge.targetTemplate = text
                                            background: Rectangle { implicitHeight: 40; color: palette.bg; radius: 10; border.color: palette.currentLine }
                                        }
                                        Button { 
                                            id: buildBtn; text: "Build Kernel"; Layout.preferredWidth: 150; enabled: !bridge.busy
                                            contentItem: Text { text: buildBtn.text; font.bold: true; font.pixelSize: 13; color: palette.bg; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            background: Rectangle { 
                                                implicitHeight: 40; radius: 10; color: buildBtn.enabled ? palette.purple : palette.currentLine 
                                                Rectangle { anchors.fill: parent; radius: 10; color: "white"; opacity: buildBtn.pressed ? 0.2 : (buildBtn.hovered ? 0.1 : 0.0) }
                                            }
                                            onClicked: bridge.buildKernel(templateName.text) 
                                        }
                                    }
                                    RowLayout {
                                        spacing: 15
                                        TextField { 
                                            id: exportPath; placeholderText: "Export destination path..."; Layout.fillWidth: true; color: palette.fg; verticalAlignment: TextInput.AlignVCenter
                                            background: Rectangle { implicitHeight: 40; color: palette.bg; radius: 10; border.color: palette.currentLine }
                                        }
                            Button { 
                                id: exportBtn
                                text: "Export XBPS"
                                Layout.preferredWidth: 150
                                enabled: exportPath.text.trim().length > 0 && !bridge.busy
                                
                                contentItem: Text { 
                                    text: exportBtn.text
                                    font.bold: true
                                    font.pixelSize: 13
                                    color: exportBtn.enabled ? palette.bg : palette.comment
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter 
                                }
                                
                                background: Rectangle { 
                                    implicitHeight: 40
                                    radius: 10
                                    color: exportBtn.enabled ? palette.purple : palette.currentLine
                                    
                                    // Interactive overlay (only active when button is enabled)
                                    Rectangle { 
                                        anchors.fill: parent
                                        radius: 10
                                        color: "white"
                                        opacity: exportBtn.enabled ? (exportBtn.pressed ? 0.2 : (exportBtn.hovered ? 0.1 : 0.0)) : 0.0
                                        visible: exportBtn.enabled
                                    }
                                }
                                
                                // This will only fire if enabled is true
                                onClicked: {
                                    if (exportPath.text.trim() !== "") {
                                        bridge.exportPackages(exportPath.text.trim())
                                    }
                                }
                            }                                    }
                                    RowLayout {
                                        spacing: 20
                                        Button { 
                                            id: sBtn; text: "Save Configuration"; Layout.fillWidth: true
                                            contentItem: Text { text: sBtn.text; font.bold: true; font.pixelSize: 13; color: palette.fg; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            background: Rectangle { 
                                                implicitHeight: 40; radius: 10; color: palette.surface; border.color: palette.green; border.width: 1 
                                                Rectangle { anchors.fill: parent; radius: 10; color: palette.green; opacity: sBtn.pressed ? 0.2 : (sBtn.hovered ? 0.1 : 0.0) }
                                            }
                                            onClicked: bridge.saveConfig()
                                        }
                                        Button { 
                                            id: lBtn; text: "Load Configuration"; Layout.fillWidth: true
                                            contentItem: Text { text: lBtn.text; font.bold: true; font.pixelSize: 13; color: palette.fg; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                                            background: Rectangle { 
                                                implicitHeight: 40; radius: 10; color: palette.surface; border.color: palette.purple; border.width: 1 
                                                Rectangle { anchors.fill: parent; radius: 10; color: palette.purple; opacity: lBtn.pressed ? 0.2 : (lBtn.hovered ? 0.1 : 0.0) }
                                            }
                                            onClicked: bridge.loadConfig()
                                        }
                                    }
                                }
                            }
                            Item { Layout.fillHeight: true }
                        }
                    }

                    // Footer
                    RowLayout {
                        Layout.fillWidth: true; Layout.topMargin: 10
                        ColumnLayout {
                            spacing: 2
                            Text { text: "STATUS: " + (bridge.busy ? "BUSY" : "READY"); color: bridge.busy ? palette.orange : palette.green; font.bold: true; font.pixelSize: 10 }
                            Text { text: bridge.statusMessage; color: bridge.statusIsError ? palette.pink : palette.comment; font.pixelSize: 11; visible: bridge.statusMessage !== "" }
                        }
                        Item { Layout.fillWidth: true }
                        Text { text: "Neko-Kernel-Manager v1.1.0"; color: palette.comment; font.pixelSize: 10; opacity: 0.5 }
                    }
                }
            }
        }
    }

    Popup {
        id: logModal
        x: (window.width - width) / 2; y: (window.height - height) / 2
        width: 600; height: 400; modal: true; focus: true
        background: Rectangle { color: palette.surface; radius: 12; border.color: palette.currentLine; border.width: 1 }

        ColumnLayout {
            anchors.fill: parent; anchors.margins: 15; spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Text { text: "System Logs & Operations"; color: palette.green; font.bold: true; font.pixelSize: 14 }
                Item { Layout.fillWidth: true }
                Button {
                    text: "Clear"; onClicked: bridge.clearLogs()
                    background: Rectangle { implicitWidth: 60; implicitHeight: 25; radius: 5; color: palette.bg; border.color: palette.currentLine }
                    contentItem: Text { text: parent.text; color: palette.comment; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
                Button {
                    text: "Close"; onClicked: logModal.close()
                    background: Rectangle { implicitWidth: 60; implicitHeight: 25; radius: 5; color: palette.pink }
                    contentItem: Text { text: parent.text; color: palette.bg; font.bold: true; font.pixelSize: 10; horizontalAlignment: Text.AlignHCenter; verticalAlignment: Text.AlignVCenter }
                }
            }
            Rectangle {
                Layout.fillWidth: true; Layout.fillHeight: true; color: palette.bg; radius: 8; border.color: palette.currentLine
                ScrollView {
                    anchors.fill: parent; anchors.margins: 10; clip: true
                    TextArea {
                        text: bridge.logs; readOnly: true; color: "#a6e3a1"
                        font.family: "0xProto Nerd Font Mono"; font.pixelSize: 11
                        background: null; wrapMode: TextEdit.Wrap
                        cursorVisible: false
                    }
                }
            }
        }
    }

    Component.onCompleted: bridge.fetchKernelOrgVersions("stable")
}