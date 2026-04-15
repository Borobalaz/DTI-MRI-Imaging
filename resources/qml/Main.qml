import QtQuick
import ConnectomicsImaging 1.0

Item {
    id: root
    width: 1600
    height: 900

    // Background
    Rectangle {
        anchors.fill: parent
        color: "#1b2635"
        z: -1
    }

    // Left panel: Object list
    Rectangle {
        id: objectsPanel
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        width: 280
        radius: 12
        color: "#131925"
        border.color: "#243042"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "Objects"
            color: "#9aa6b2"
            font.pixelSize: 18
            font.bold: true
        }
    }

    // Center panel: 3D scene viewport
    Rectangle {
        anchors.left: objectsPanel.right
        anchors.leftMargin: 12
        anchors.right: inspectionPanel.left
        anchors.rightMargin: 12
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        radius: 12
        color: '#d40606'
        border.color: "#243042"
        border.width: 1

        SceneViewport {
            id: sceneViewport
            anchors.fill: parent
            anchors.margins: 1
            focus: true
            dwiPath: "assets/volumes/dwi/HARDI150_hdbet_masked4d.nii.gz"
            bvalPath: "assets/volumes/dwi/HARDI150.bval"
            bvecPath: "assets/volumes/dwi/HARDI150.bvec"
        }
    }

    // Right panel: Object inspector
    Rectangle {
        id: inspectionPanel
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        width: 320
        radius: 12
        color: "#131925"
        border.color: "#243042"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "Inspector"
            color: "#9aa6b2"
            font.pixelSize: 18
            font.bold: true
        }
    }
}