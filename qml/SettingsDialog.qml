import QtQuick 2.13
import QtQuick.Controls 2.12

Dialog {
    id: control
    implicitWidth: 400
    implicitHeight: 260
    x: (mainWin.width-width)/2
    y: (mainWin.height-height)/2
    z: 2
    onAccepted: {
        client.serverPort = tftpPort.text
        client.readDelayMs = timeout.text
        if (client.numWorkers !== numWorkers.value) {
            client.numWorkers = numWorkers.value
            msgDlgProps.fatalError = true
            msgDlgProps.show(qsTr("Warning"), qsTr("The application will be closed.\nPlease restart the application."))
        }
    }
    visible: true
    title: qsTr("Settings")
    modal: true
    closePolicy: Popup.CloseOnEscape
    standardButtons: Dialog.Ok | Dialog.Cancel
    Grid {
        rows: 3
        columns: 2
        rowSpacing: 5
        columnSpacing: 10
        Label {
            text: qsTr("TFTP port")
            elide: Text.ElideRight
            clip: true
            font.pointSize: appStyle.textFontSize
            height: tftpPort.height
            verticalAlignment: Text.AlignVCenter
        }
        TextField {
            id: tftpPort
            text: client.serverPort
            validator: IntValidator { bottom: 0; top: 65535 }
            width: appStyle.textFieldWidth
            font.pointSize: appStyle.textFontSize
            selectByMouse: true
        }
        Label {
            text: qsTr("Timeout [milliseconds]")
            elide: Text.ElideRight
            clip: true
            font.pointSize: appStyle.textFontSize
            height: timeout.height
            verticalAlignment: Text.AlignVCenter
        }
        TextField {
            id: timeout
            text: client.readDelayMs
            validator: IntValidator { bottom: 0 }
            width: appStyle.textFieldWidth
            font.pointSize: appStyle.textFontSize
            selectByMouse: true
        }
        Label {
            text: qsTr("Number of workers")
            elide: Text.ElideRight
            clip: true
            font.pointSize: appStyle.textFontSize
            height: numWorkers.height
            verticalAlignment: Text.AlignVCenter
            width: subLabel.width
            Label {
                id: subLabel
                anchors.bottom: parent.bottom
                font.pointSize: appStyle.textFontSize - 4
                text: qsTr("(the application must be restarted)")
            }
        }
        SpinBox {
            id: numWorkers
            value: client.numWorkers
            from: 1
            editable: true
            validator: IntValidator { bottom: 1 }
            width: appStyle.textFieldWidth
            font.pointSize: appStyle.textFontSize
        }
    }
}
