import QtQuick 2.13

QtObject {
    readonly property bool isWindows: Qt.platform.os === "windows"
    readonly property bool isMac: Qt.platform.os === "osx"

    readonly property int textFontSize: isMac ? 14 : 12
    readonly property int buttonFontSize: isMac ? 13 : 11
    readonly property int textFieldWidth: 200
}
