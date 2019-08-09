import QtQuick 2.13
import QtQuick.Controls 2.12
import QtQuick.Dialogs 1.3 as Old

ApplicationWindow {
    id: mainWin
    visible: true
    width: 640
    height: 480
    title: qsTr("TFTP Client")

    //application style props
    DesktopStyle {
        id: appStyle
    }
    QtObject {
        id: msgDlgProps
        property string title: ""
        property string text: ""
        function show(t, m) {
            msgDlgProps.text = m
            msgDlgProps.title = t
        }
    }
    Connections {
        target: client
        onInfo: mainWinFooter.text = msg
    }

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

    Old.FileDialog {
        id: fileDialog
        property var callback: null
        function getServerIpAddresses(hosts) {
            hostTextField.text = hosts
        }
        function getFiles(files) {
            fileTextField.text = files
        }
        function getWorkingFolder(folder) {
            workingFolderField.text = folder
        }
        visible: false
        selectExisting: true
        selectFolder: false
        selectMultiple : false
        nameFilters: [ "All files (*)" ]
        onAccepted: {
            if (null !== fileDialog.callback) {
                var url = fileDialog.fileUrl.toString()
                fileDialog.callback(url.slice("file://".length))
                fileDialog.callback = null
            }
        }
    }

    Grid {
        id: grid
        anchors {
            top: logo.bottom
            topMargin: 20
            horizontalCenter: parent.horizontalCenter
        }
        rowSpacing: 5
        columnSpacing: 10
        columns: 3
        rows: 3
        Label {
            height: hostTextField.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Host(s)")
            font.pointSize: appStyle.textFontSize
        }
        TextField {
            id: hostTextField
            placeholderText: qsTr("Remote server IP address(es)")
            width: 0.4*mainWin.width
            font.pointSize: appStyle.textFontSize
        }
        Button {
            display: AbstractButton.TextOnly
            text: "..."
            font.pointSize: appStyle.buttonFontSize
            onClicked: {
                fileDialog.title = qsTr("Please choose a file with server IP addresses")
                fileDialog.selectExisting = true
                fileDialog.selectFolder = false
                fileDialog.callback = fileDialog.getServerIpAddresses
                fileDialog.visible = true
            }
        }
        Label {
            height: fileTextField.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Filename(s)")
            font.pointSize: appStyle.textFontSize
        }
        TextField {
            id: fileTextField
            placeholderText: qsTr("Remote filename(s)")
            width: hostTextField.width
            font.pointSize: appStyle.textFontSize
        }
        Button {
            display: AbstractButton.TextOnly
            text: "..."
            font.pointSize: appStyle.buttonFontSize
            onClicked: {
                fileDialog.title = qsTr("Please choose a file with filenames")
                fileDialog.selectExisting = true
                fileDialog.selectFolder = false
                fileDialog.callback = fileDialog.getFiles
                fileDialog.visible = true
            }
        }
        Label {
            height: workingFolderField.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Working folder")
            font.pointSize: appStyle.textFontSize
        }
        TextField {
            id: workingFolderField
            placeholderText: qsTr("Folder where all downloaded files are created")
            width: hostTextField.width
            text: client.workingFolder
            onEditingFinished: client.workingFolder = text
            font.pointSize: appStyle.textFontSize
        }
        Button {
            display: AbstractButton.TextOnly
            text: "..."
            font.pointSize: appStyle.buttonFontSize
            onClicked: {
                fileDialog.title = qsTr("Please choose the working folder")
                fileDialog.selectExisting = true
                fileDialog.selectFolder = true
                fileDialog.callback = fileDialog.getWorkingFolder
                fileDialog.visible = true
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
            enabled: !client.inProgress
            display: AbstractButton.TextOnly
            text: qsTr("Start")
            font.pointSize: appStyle.buttonFontSize
            onClicked: {
                if ("" === hostTextField.text) {
                    msgDlgProps.show(qsTr("Error"), qsTr("Hosts field cannot be empty"))
                    return
                }
                if ("" === fileTextField.text) {
                    msgDlgProps.show(qsTr("Error"), qsTr("Files field cannot be empty"))
                    return
                }
                if ("" === client.workingFolder) {
                    msgDlgProps.show(qsTr("Error"), qsTr("Working folder field cannot be empty"))
                    return
                }
                client.startDownload(hostTextField.text, fileTextField.text)
            }
        }
        Button {
            enabled: client.inProgress
            display: AbstractButton.TextOnly
            text: qsTr("Cancel")
            font.pointSize: appStyle.buttonFontSize
            onClicked: client.stopDownload()
        }
    }

    Loader {
        active: "" !== msgDlgProps.text
        source: "qrc:/qml/MessageDialog.qml"
    }

    footer: Label {
        id: mainWinFooter
        leftPadding: 10
        rightPadding: 10
        bottomPadding: 5
    }
}
