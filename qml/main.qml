import QtQuick 2.13
import QtQuick.Controls 2.12
import QtQuick.Dialogs 1.3 as Old

ApplicationWindow {
    id: mainWin
    visible: true
    width: 640
    height: 550
    title: qsTr("TFTP Client")

    //application style props
    DesktopStyle {
        id: appStyle
    }
    QtObject {
        id: msgDlgProps
        property bool okCancel: false
        property string title: ""
        property string text: ""
        property bool fatalError: false
        function show(t, m) {
            msgDlgProps.text = m
            msgDlgProps.title = t
        }
    }
    Connections {
        target: client
        onInfo: mainWinFooter.text = msg
    }

    Button {
        enabled: !client.running
        anchors {
            top: parent.top
            topMargin: 0
            right: parent.right
            rightMargin: 5
        }
        text: qsTr("Settings")
        display: AbstractButton.TextOnly
        onClicked: {
            settingsDlg.active = true
            settingsDlg.item.visible = true
        }
    }

    Image {
        id: logo
        anchors {
            top: parent.top
            topMargin: 20
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
            client.hosts = hosts
            client.parseAddressList()
        }
        function getFiles(files) {
            fileTextField.text = files
            client.files = files
            client.parseFileList()
        }
        function getWorkingFolder(folder) {
            workingFolderField.text = folder
            client.workingFolder = folder
        }
        visible: false
        selectExisting: true
        selectFolder: false
        selectMultiple : false
        nameFilters: [ "All files (*)" ]
        onAccepted: {
            if (null !== fileDialog.callback) {
                fileDialog.callback(client.toLocalFile(fileDialog.fileUrl))
                fileDialog.callback = null
            }
        }
    }

    Label {
        id: addrIndex
        anchors {
            top: logo.bottom
            topMargin: 20
            horizontalCenter: parent.horizontalCenter
        }
        visible: progressBar.visible
        font.pointSize: appStyle.textFontSize - 2
        text: qsTr("Address index ") + client.addrIndex
        horizontalAlignment: Text.AlignHCenter
    }
    Label {
        anchors {
            horizontalCenter: progressBar.left
            verticalCenter: addrIndex.verticalCenter
        }
        font: addrIndex.font
        text: progressBar.from
        horizontalAlignment: Text.AlignHCenter
    }
    Label {
        anchors {
            horizontalCenter: progressBar.right
            verticalCenter: addrIndex.verticalCenter
        }
        font: addrIndex.font
        text: progressBar.to
        horizontalAlignment: Text.AlignHCenter
    }

    ProgressBar {
        id: progressBar
        anchors {
            top: addrIndex.bottom
            topMargin: 2
            horizontalCenter: parent.horizontalCenter
        }
        visible: true
        width: grid.width
        from: 0
        to: client.addrCount
        value: client.addrIndex
    }
    Label {
        id: currentAddr
        visible: ("" !== client.currentAddress) && progressBar.visible
        anchors {
            top: progressBar.bottom
            topMargin: 2
            horizontalCenter: parent.horizontalCenter
        }
        font: addrIndex.font
        text: qsTr("Downloading from ") + client.currentAddress + " ..."
        horizontalAlignment: Text.AlignHCenter
    }
    Label {
        anchors {
            horizontalCenter: fileProgressBar.left
            verticalCenter: currentAddr.verticalCenter
        }
        font: addrIndex.font
        text: fileProgressBar.from
        horizontalAlignment: Text.AlignHCenter
    }
    Label {
        anchors {
            horizontalCenter: fileProgressBar.right
            verticalCenter: currentAddr.verticalCenter
        }
        font: addrIndex.font
        text: fileProgressBar.to
        horizontalAlignment: Text.AlignHCenter
    }
    ProgressBar {
        id: fileProgressBar
        anchors {
            top: currentAddr.bottom
            topMargin: 2
            horizontalCenter: parent.horizontalCenter
        }
        visible: true
        width: grid.width
        from: 0
        to: client.fileCount
        value: client.fileIndex
    }

    Grid {
        id: grid
        enabled: startBtn.enabled
        anchors {
            top: fileProgressBar.bottom
            topMargin: 20
            horizontalCenter: parent.horizontalCenter
        }
        rowSpacing: 5
        columnSpacing: 10
        columns: 3

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
            text: client.hosts
            selectByMouse: true
            onEditingFinished: {
                client.parseAddressList()
                client.hosts = text
            }
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
            height: prefixTextField.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Filename prefix")
            font.pointSize: appStyle.textFontSize
        }
        TextField {
            id: prefixTextField
            placeholderText: qsTr("Remote filename prefix")
            width: hostTextField.width
            font.pointSize: appStyle.textFontSize
            text: client.prefix
            selectByMouse: true
            onEditingFinished: client.prefix = text
        }
        Item {
            width: 1
            height: 1
        }

        Label {
            height: fileTextField.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Filename suffix")
            font.pointSize: appStyle.textFontSize
        }
        TextField {
            id: fileTextField
            placeholderText: qsTr("Remote filename suffix")
            width: hostTextField.width
            font.pointSize: appStyle.textFontSize
            text: client.files
            selectByMouse: true
            onEditingFinished: client.files = text
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
            height: extTextField.height
            verticalAlignment: Text.AlignVCenter
            text: qsTr("Filename extension")
            font.pointSize: appStyle.textFontSize
        }
        TextField {
            id: extTextField
            placeholderText: qsTr("Remote filename extension")
            width: hostTextField.width
            font.pointSize: appStyle.textFontSize
            text: client.extension
            selectByMouse: true
            onEditingFinished: client.extension = text
        }
        Item {
            width: 1
            height: 1
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
            selectByMouse: true
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
            id: startBtn
            enabled: !client.running
            display: AbstractButton.TextOnly
            text: qsTr("Start")
            font.pointSize: appStyle.buttonFontSize
            onClicked: {
                if (0 === client.addrCount) {
                    msgDlgProps.show(qsTr("Error"), qsTr("At least one host IP address must be specified"))
                    return
                }
                if ("" === client.fileCount) {
                    msgDlgProps.show(qsTr("Error"), qsTr("At least one filename must be specified"))
                    return
                }
                if ("" === client.workingFolder) {
                    msgDlgProps.show(qsTr("Error"), qsTr("Working folder must be specified"))
                    return
                }
                client.parseFileList()
                client.startDownload(hostTextField.text, fileTextField.text)
            }
        }
        Button {
            enabled: client.running
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
    Loader {
        id: settingsDlg
        active: false
        source: "qrc:/qml/SettingsDialog.qml"
    }

    footer: Label {
        id: mainWinFooter
        leftPadding: 5
        bottomPadding: 5
    }
}
