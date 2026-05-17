import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    color: "black"

    required property QtObject wifiModel

    StackView {
        id: settingsStack
        anchors.fill: parent
        // Start with the main settings list
        initialItem: mainSettingsComponent

        // --- DISABLE TRANSITIONS ---
        pushEnter: Transition {}
        pushExit: Transition {}
        popEnter: Transition {}
        popExit: Transition {}
    }

    // --- 1. MAIN SETTINGS PAGE ---
    Component {
        id: mainSettingsComponent
        Rectangle {
            color: "black"

            Column {
                anchors.fill: parent

                // Header for Main Settings
                Rectangle {
                    width: parent.width; height: 60; color: "#1a1a1a"
                    Text {
                        text: "SETTINGS"
                        color: "white"
                        font.pixelSize: 22
                        font.bold: true
                        anchors.centerIn: parent
                    }
                }

                // WiFi Menu Item
                Rectangle {
                    width: parent.width; height: 60; color: "black"
                    border.color: "#333333"; border.width: 1

                    Row {
                        anchors.fill: parent; anchors.leftMargin: 20; spacing: 15
                        Text {
                            text: "WiFi"
                            color: "white"; font.pixelSize: 18
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            console.debug("WiFi menu clicked, refreshing network list");
                            wifiModel.refresh(); // Trigger WiFi scan
                            settingsStack.push(wifiListComponent); // Switch to WiFi
                        }
                    }
                }
            }
        }
    }

    // --- 2. WIFI SUBMENU PAGE ---
    Component {
        id: wifiListComponent
        Rectangle {
            color: "black"
            readonly property bool isLoading: true

            Column {
                anchors.fill: parent

                // Header with Back Button
                Rectangle {
                    id: wifiHeader
                    width: parent.width; height: 60; color: "#1a1a1a"
                    clip: true // Important: hides the line when it's outside the header

                    Row {
                        anchors.fill: parent; anchors.leftMargin: 15; spacing: 15
                        z: 2 // Keep text above the loading line

                        // Back Button
                        Text {
                            text: "< Back"
                            color: "#007AFF"; font.pixelSize: 20; font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                            MouseArea {
                                anchors.fill: parent
                                onClicked: settingsStack.pop() // Return to Main Settings
                            }
                        }

                        Text {
                            text: "WiFi Networks"
                            color: "white"; font.pixelSize: 20; font.bold: true
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        // --- MOVING BLUE LINE (Loading Indicator) ---
                        Rectangle {
                            id: loadingLine
                            width: 100 // Short line
                            height: 3
                            color: "#007AFF" // Blue color
                            anchors.bottom: parent.bottom
                            z: 1

                            // Hide the line when loading is finished
                            visible: opacity > 0
                            opacity: isLoading ? 1.0 : 0.0

                            // Smooth fade out when loading stops
                            Behavior on opacity {
                                NumberAnimation { duration: 500 }
                            }

                            // Animation runs on main GUI thread
                            ///SequentialAnimation {
                            ///    running: isLoading
                            ///    loops: Animation.Infinite

                            ///    NumberAnimation {
                            ///        target: loadingLine
                            ///        property: "x"
                            ///        from: -loadingLine.width
                            ///        to: root.width
                            ///        duration: 2000
                            ///    }
                            ///}

                            // Animators run on the Render Thread (smooth even if GUI is busy)
                            XAnimator {
                                target: loadingLine
                                from: -loadingLine.width
                                to: root.width
                                duration: 2000
                                loops: Animation.Infinite
                                running: isLoading
                            }
                        }
                    }
                }

                // The List of Networks
                ListView {
                    id: wifiListView
                    width: parent.width
                    height: parent.height - wifiHeader.height
                    model: wifiModel
                    clip: true

                    // Property to store the currently selected index
                    property int selectedIndex: -1

                    delegate: Rectangle {
                        id: itemDelegate
                        width: wifiListView.width
                        // Dynamic height: expanded if selected
                        height: wifiListView.selectedIndex === index ? 120 : 60
                        color: "black"
                        clip: true

                        property bool isConnected: model.ssid === wifiModel.connectedSSID

                        // Smooth height transition
                        Behavior on height { NumberAnimation { duration: 200 } }

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 10

                            // 1. Top Row (Always visible)
                            Item {
                                width: parent.width
                                height: 40

                                Image {
                                    id: wifiIcon
                                    source: model.quality <= 25 ? "qrc:/images/icons/wifi_1.png" : model.quality > 25 && model.quality < 65 ? "qrc:/images/icons/wifi_2.png" : "qrc:/images/icons/wifi_3.png"
                                    width: 32; height: 32
                                    fillMode: Image.PreserveAspectFit
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                }

                                Text {
                                    text: model.ssid
                                    color: "white"
                                    font.pixelSize: 16
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: wifiIcon.right
                                    anchors.leftMargin: 15
                                    anchors.right: statusText.left
                                    anchors.rightMargin: 10
                                    elide: Text.ElideRight
                                }

                                Text {
                                    id: statusText
                                    text: "(Connected)"
                                    color: "#a0a0a0"
                                    font.pixelSize: 15
                                    font.bold: true
                                    font.italic: true
                                    visible: itemDelegate.isConnected
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.right: parent.right
                                    anchors.rightMargin: 10
                                }
                            }

                            // 2. Expandable Section (Visible only when selected)
                            Item {
                                width: parent.width
                                height: 40
                                visible: wifiListView.selectedIndex === index
                                opacity: visible ? 1 : 0
                                Behavior on opacity { NumberAnimation { duration: 200 } }
                    
                                // --- BLOCK A: FOR DISCONNECTED NETWORKS ---
                                Row {
                                    anchors.fill: parent
                                    spacing: 10
                                    visible: !itemDelegate.isConnected // Show only if NOT connected
                    
                                    TextField {
                                        id: passwordField
                                        placeholderText: "Enter password"
                                        width: parent.width * 0.65
                                        color: "white"
                                        //echoMode: TextInput.Password // Masks the password input
                                        verticalAlignment: TextInput.AlignVCenter
                                        background: Rectangle {
                                            color: "#1a1a1a"
                                            border.color: "#007AFF"
                                            border.width: 1
                                        }
                                    }
                    
                                    Button {
                                        text: "Connect"
                                        width: parent.width - passwordField.width - parent.spacing
                                        contentItem: Text {
                                            text: parent.text
                                            color: "white"
                                            horizontalAlignment: Text.AlignHCenter
                                            verticalAlignment: Text.AlignVCenter
                                        }
                                        background: Rectangle {
                                            color: parent.pressed ? "#005bb5" : "#007AFF"
                                            radius: 4
                                        }
                                        onClicked: {
                                            console.debug("Connecting to " + model.ssid + " with pass: " + passwordField.text);
                                            wifiModel.connectToNetwork(model.ssid, passwordField.text);
                                        }
                                    }
                                }
                    
                                // --- BLOCK B: FOR THE CURRENTLY CONNECTED NETWORK ---
                                Button {
                                    text: "Disconnect"
                                    anchors.fill: parent
                                    visible: itemDelegate.isConnected // Show only if already connected
                                    
                                    contentItem: Text {
                                        text: parent.text
                                        color: "white"
                                        font.bold: true
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                    }
                                    background: Rectangle {
                                        color: parent.pressed ? "#b52a2a" : "#D32F2F" // Red color for destructive action
                                        radius: 4
                                        border.color: "#ff6666"
                                        border.width: parent.pressed ? 2 : 0
                                    }
                                    onClicked: {
                                        console.debug("Disconnecting from " + model.ssid)
                                        wifiModel.disconnectFromNetwork();
                                    }
                                }
                            }
                        }

                        // Click to select/expand
                        MouseArea {
                            anchors.fill: parent
                            z: -1 // Behind the button and textfield
                            onClicked: {
                                if (wifiListView.selectedIndex === index) {
                                    wifiListView.selectedIndex = -1 // Collapse if clicked again
                                } else {
                                    wifiListView.selectedIndex = index // Expand
                                }
                            }
                        }

                        // Separator
                        Rectangle {
                            anchors.bottom: parent.bottom
                            width: parent.width; height: 1; color: "#222222"
                        }
                    }
                }
            }
        }
    }
}

