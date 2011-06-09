// This file is part of Cosmographia.
//
// Copyright (C) 2011 Chris Laurel <claurel@gmail.com>
//
// Cosmographia is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; either
// version 2 of the License, or (at your option) any later version.
//
// Cosmographia is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public
// License along with Cosmographia. If not, see <http://www.gnu.org/licenses/>.

import QtQuick 1.0

Item {
    id: container

    property string fontFamily: "Century Gothic"
    property int fontSize: 14
    property color textColor: "#72c0ff"

    function show()
    {
        state = "visible"
    }

    function hide()
    {
        state = ""
    }

    width: 300
    height: 600
    opacity: 0

    x: 32
    anchors {
        verticalCenter: parent.verticalCenter
    }

    Rectangle {
        anchors.fill: parent
        color: "#404040"
        opacity: 0.5
    }

    Image {
        id: close
        width: 20; height: 20
        smooth: true
        anchors {
            right: parent.right
            rightMargin: 8
            top: parent.top
            topMargin: 8
        }
        source: "qrc:/icons/clear.png"

        MouseArea {
            anchors.fill: parent
            onClicked: { container.hide() }
        }
    }

    Column {
        anchors.fill: parent
        spacing: 5

        Item {
            width: parent.width
            height: 36

/*            
            Rectangle {
                anchors.fill: parent
                opacity: 0.25
                border.color: "#ffffff"
                border.width: 1
                color: "transparent"
            }
*/

            Text {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.leftMargin: 8
                font.family: fontFamily
                font.pixelSize: fontSize
                font.weight: Font.Bold
                color: "white"
                text: "Settings"
            }
        }

        Grid {
            anchors {
                left: parent.left
                margins: 8
            }

            columns: 2
            spacing: 8

            // Blank line
            Item { height: 20 } Item {}

            Text {
                font.family: fontFamily
                font.pixelSize: fontSize
                color: textColor
                text: "Labels"
            }

            TextToggle {
                enabled: true
                onToggled: { universeView.labelsVisible = enabled }
            }

            Text {
                font.family: fontFamily
                font.pixelSize: fontSize
                color: textColor
                text: "Equatorial Grid"
            }

            TextToggle {
                enabled: false
                onToggled: { universeView.equatorialGridVisible = enabled }
            }

            Text {
                font.family: fontFamily
                font.pixelSize: fontSize
                color: textColor
                text: "Ecliptic"
            }

            TextToggle {
                onToggled: { universeView.eclipticVisible = enabled }
                enabled: false
            }

            Text {
                font.family: fontFamily
                font.pixelSize: fontSize
                color: textColor
                text: "Constellation Figures"
            }

            TextToggle {
                onToggled: { universeView.constellationFiguresVisible = enabled }
                enabled: false
            }

            Text {
                font.family: fontFamily
                font.pixelSize: fontSize
                color: textColor
                text: "Constellation Names"
            }

            TextToggle {
                onToggled: { universeView.constellationNamesVisible = enabled }
                enabled: false
            }
        }
    }

    states: State {
        name: "visible"
        PropertyChanges { target: container; opacity: 1 }
    }

    transitions: [
        Transition {
            from: ""; to: "visible"
            NumberAnimation { properties: "opacity" }
        },
        Transition {
            from: "visible"; to: ""
            NumberAnimation { properties: "opacity" }
        }
    ]
}