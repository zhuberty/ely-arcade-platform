#!/bin/bash
# Arcade launcher script -- run by /usr/share/xsessions/arcade.desktop
# 'exec' replaces this shell so that when the menu exits the X session ends
# and LightDM takes back control. The menu sets its own CWD (platform root).

cd "$(dirname "$(readlink -f "$0")")"
exec ./bin/Release/ely-arcade-platform
