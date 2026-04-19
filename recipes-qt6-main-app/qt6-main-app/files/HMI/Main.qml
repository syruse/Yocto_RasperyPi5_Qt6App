import QtQuick
import QtQuick.Controls

Window {
    id: id_main
    width: 1980
    height: 1080
    visible: true

    Image {
        id: id_bg
        source: "qrc:/images/bg/bg.png"
        width: parent.width
        height: parent.height
        anchors.centerIn: parent
    }

    Image {
        id: id_preview
        source: id_listModel.get(id_list.currentIndex).bg_src
        width: 0.7 * parent.width
        height: 0.6 * parent.height
        anchors.right: parent.right
        anchors.top: parent.top
    }

    Settings {
        id: idSettings
        anchors.top: parent.top
        anchors.left: parent.left
        width: 0.25 * parent.width
        height: 0.55 * parent.height

        wifiModel: wifiNetworksList
    }

    Container {
        width: parent.width
        height: 0.2 * parent.height
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 0.1 * parent.height

        contentItem: ListView {
            id: id_list
            model: ListModel {
                id: id_listModel
                ListElement {
                    name: "Alien"
                    bg_src: "qrc:/images/bg/alien.jpg"
                }
                ListElement {
                    name: "Ballerina"
                    bg_src: "qrc:/images/bg/ballerina.jpg"
                }
                ListElement {
                    name: "Jurassic World"
                    bg_src: "qrc:/images/bg/jurassic.jpg"
                }
                ListElement {
                    name: "Hobbit"
                    bg_src: "qrc:/images/bg/hobbit.jpg"
                }
                ListElement {
                    name: "Terminator"
                    bg_src: "qrc:/images/bg/terminator.jpg"
                }
            }
            snapMode: ListView.SnapOneItem
            orientation: ListView.Horizontal
            spacing: 10
            anchors.fill: parent

            delegate: Rectangle {
                width: 0.2 * id_main.width + (ListView.isCurrentItem ? 0.03 * id_main.width : 0)
                height: 0.2 * id_main.height + (ListView.isCurrentItem ? 0.03 * id_main.height : 0)

                Image {
                    id: id_butIm
                    source: bg_src
                    width: parent.width
                    height: parent.height
                    anchors.centerIn: parent
                    mipmap: true
                }

                Text {
                    text: name
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: -30
                    anchors.horizontalCenter: parent.horizontalCenter
                    font.family: "Junicode"
                    font.pointSize: 21
                    font.bold: true
                    color: "black"
                }
                MouseArea {
                    anchors.fill: parent
                    onClicked: {
                        id_list.currentIndex = index
                    }
                }
                Behavior on width {
                    enabled: true
                    NumberAnimation {
                        duration: 1000
                        easing.type: Easing.InOutQuad
                    }
                }
                Behavior on height {
                    enabled: true
                    NumberAnimation {
                        duration: 1000
                        easing.type: Easing.InOutQuad
                    }
                }
            }
        }
    }

    Timer {
        interval: 5000
        running: true
        repeat: true

        onTriggered: {
            id_list.currentIndex = ++id_list.currentIndex % id_list.count
        }
    }
}
