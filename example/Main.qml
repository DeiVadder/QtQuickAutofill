// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuickAutofill

ApplicationWindow {
    id: root
    visible: true
    width: 400
    height: 700
    title: qsTr("QtQuickAutofill example")

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        anchors.topMargin: 64
        spacing: 12

        Label {
            text: qsTr("Sign in")
            font.pixelSize: 24
            font.bold: true
        }

        TextField {
            id: username
            Layout.fillWidth: true
            placeholderText: qsTr("Username")
            inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
            Autofill.role: Autofill.Username
            Autofill.onFilled: status.text = qsTr("Username filled by the password manager")
        }

        TextField {
            id: password
            Layout.fillWidth: true
            placeholderText: qsTr("Password")
            echoMode: TextInput.Password
            inputMethodHints: Qt.ImhHiddenText | Qt.ImhNoPredictiveText | Qt.ImhSensitiveData
            Autofill.role: Autofill.Password
            Autofill.onFilled: status.text = qsTr("Password filled by the password manager")
        }

        Button {
            Layout.fillWidth: true
            text: qsTr("Sign in")
            enabled: username.text.length > 0 && password.text.length > 0
            onClicked: {
                // Pretend the sign-in succeeded; the platform may now offer to save the credentials
                AutofillService.commit()
                status.text = qsTr("Signed in as %1").arg(username.text)
            }
        }

        Label {
            id: status
            Layout.fillWidth: true
            wrapMode: Text.Wrap
            text: AutofillService.available ? qsTr("Autofill available") : qsTr("Autofill not available on this platform")
        }

        Item { Layout.fillHeight: true }
    }
}
