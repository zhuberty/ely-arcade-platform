#!/bin/bash
# Join or forget a Wi-Fi network through NetworkManager. Called by the menu's Settings > Wi-Fi screen
# as the arcade account (no root needed: the account is in the `netdev` group, which NetworkManager's
# polkit rule allows to change connections). Not meant to be typed by hand, but it works from a shell.
#
#   scripts/wifi.sh connect SSID [wpa-psk|sae] [hidden]
#                                                the password is read from STDIN (one line; empty line or
#                                                no input = an open network). It is never put on a command line,
#                                                where `ps` could show it. `hidden`: the network does not
#                                                announce its name.
#   scripts/wifi.sh forget SSID                  deletes the saved connection of that network
#
# `connect` replaces a saved connection of the same name (that is how a changed password is fixed), saves the
# new one for automatic use at every boot, and activates it. If that fails (wrong password, out of range...)
# the new connection is deleted again, so the cabinet does not keep retrying a wrong password forever.
# Output: the last line is OK on success; on failure nmcli's message is printed and the exit code is 1
# (2 = invalid arguments). nmcli is the program found in PATH (tests/wifi_test.sh puts a fake one there).
set -u
export LC_ALL=C

die() { echo "$*" >&2; exit "${2:-1}"; }

mode=${1:-}
ssid=${2:-}
sec=${3:-wpa-psk}
hidden=${4:-}

[ -n "$ssid" ] || die "usage: wifi.sh connect|forget SSID [wpa-psk|sae]" 2
[ "${#ssid}" -le 32 ] || die "The network name is longer than 32 characters" 2
case $ssid in
    -*) die "Network names that start with '-' are not supported" 2 ;;
esac
case $ssid in
    *$'\n'*|*$'\r'*) die "Invalid network name" 2 ;;
esac
name="ely-wifi $ssid"

# Delete every saved connection called $name (there should be at most one).
forget() {
    local n=0
    while [ $n -lt 5 ] && nmcli connection delete id "$name" > /dev/null 2>&1; do n=$((n + 1)); done
    return 0
}

case $mode in
    forget)
        forget
        echo OK
        ;;

    connect)
        case $sec in wpa-psk|sae) ;; *) die "unknown security type '$sec'" 2 ;; esac
        case $hidden in ''|hidden) ;; *) die "unknown option '$hidden'" 2 ;; esac
        IFS= read -r psk || psk=
        if [ -n "$psk" ]; then
            if ! [[ $psk =~ ^[[:print:]]{8,63}$ || $psk =~ ^[0-9A-Fa-f]{64}$ ]]; then
                die "The password must have 8 to 63 characters" 2
            fi
        fi

        nmcli radio wifi on > /dev/null 2>&1
        forget
        extra=()
        [ "$hidden" = hidden ] && extra=(802-11-wireless.hidden yes)
        out=$(nmcli connection add type wifi con-name "$name" ssid "$ssid" \
                  connection.autoconnect yes "${extra[@]}" 2>&1) || die "$out"
        if [ -n "$psk" ]; then
            out=$(nmcli connection modify id "$name" wifi-sec.key-mgmt "$sec" 2>&1) || { forget; die "$out"; }
            # The password goes in through the connection editor's STDIN, not through an argument.
            out=$(printf 'set wifi-sec.psk %s\nsave\nquit\n' "$psk" | nmcli connection edit id "$name" 2>&1) \
                || { forget; die "$out"; }
        fi
        if ! out=$(nmcli --wait 45 connection up id "$name" 2>&1); then
            forget
            die "$out"
        fi
        echo OK
        ;;

    *)
        die "usage: wifi.sh connect|forget SSID [wpa-psk|sae]" 2
        ;;
esac
