import QtQuick 2.13
import QtQuick.Controls 2.12

Dialog {
    id: control
    implicitWidth: 400
    implicitHeight: 200
    x: (mainWin.width-width)/2
    y: (mainWin.height-height)/2
    z: 2
    onAccepted: {
        msgDlgProps.title = ""
        msgDlgProps.text = ""
    }
    visible: "" !== controlLabel.text
    title: msgDlgProps.title
    modal: true
    closePolicy: Popup.CloseOnEscape
    standardButtons: msgDlgProps.okCancel ? (Dialog.Ok | Dialog.Cancel) :  Dialog.Ok
    LabelToolTip {
        id: controlLabel
        text: msgDlgProps.text
        anchors.fill: parent
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
        elide: Text.ElideRight
        clip: true
        font.pointSize: appStyle.textFontSize
        toolTipZ: 3
    }
}
