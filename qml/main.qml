import QtQuick 2.13
import QtQuick.Controls 2.12

ApplicationWindow {
    id: mainWin
    visible: true
    width: 640
    height: 480
    title: qsTr("TFTP Client")

    Image {
        id: logo
        anchors {
            top: parent.top
            topMargin: 30
            horizontalCenter: parent.horizontalCenter
        }
        height: 100
        width: height
        source: "qrc:/img/logo.png"
        mipmap: true
        fillMode: Image.PreserveAspectFit
    }

    Grid {
        id: grid
        anchors {
            top: logo.bottom
            topMargin: 20
            horizontalCenter: parent.horizontalCenter
        }
        rowSpacing: 5
        columnSpacing: 5
        columns: 3
        rows: 2
        Label {
            height: hostTextField.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Host(s)")
        }
        TextField {
            id: hostTextField
            placeholderText: qsTr("Remote server IP address(es)")
            width: 0.4*mainWin.width
        }
        Button {
            display: AbstractButton.TextOnly
            text: "..."
            onClicked: {
                //TODO
            }
        }
        Label {
            height: fileTextField.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Filename(s)")
        }
        TextField {
            id: fileTextField
            placeholderText: qsTr("Remote filename(s)")
            width: hostTextField.width
        }
        Button {
            display: AbstractButton.TextOnly
            text: "..."
            onClicked: {
                //TODO
            }
        }
        Label {
            height: workingFolderField.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Working folder")
        }
        TextField {
            id: workingFolderField
            placeholderText: qsTr("Folder where all downloaded files are created")
            width: hostTextField.width
        }
        Button {
            display: AbstractButton.TextOnly
            text: "..."
            onClicked: {
                //TODO
            }
        }
    }
    Row {
        anchors {
            top: grid.bottom
            topMargin: 20
            horizontalCenter: parent.horizontalCenter
        }
        spacing: 10
        Button {
            display: AbstractButton.TextOnly
            text: qsTr("Start")
            onClicked: {
                client.startDownload(hostTextField.text, fileTextField.text)
            }
        }
        Button {
            display: AbstractButton.TextOnly
            text: qsTr("Cancel")
            onClicked: {
                client.stopDownload()
            }
        }
    }
}
