import QtQuick 2.13
import QtQuick.Controls 2.12

Label {
    id: controlLabel
    property alias toolTipZ: controlToolTip.z
    MouseArea {
        id: controlLabelMouseArea
        enabled: controlLabel.truncated
        anchors.fill: parent
        hoverEnabled: true
    }
    ToolTip {
        id: controlToolTip
        visible: controlLabelMouseArea.containsMouse
        text: controlLabel.text
    }
}
