#!/bin/bash
# Tests scripts/wifi.sh against a FAKE nmcli (no network, no root). Run: scripts/test.sh
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
T=$(mktemp -d)
trap 'rm -rf "$T"' EXIT
mkdir -p "$T/bin"
failures=0
check() { if ! eval "$1"; then echo "FAIL: $2"; failures=$((failures + 1)); fi; }

# The fake nmcli logs every call (arguments) and what it read from stdin, and fails where told to.
cat > "$T/bin/nmcli" <<'EOF'
#!/bin/bash
echo "ARGS: $*" >> "$FAKE_LOG"
case "$*" in
    "connection edit"*) cat >> "$FAKE_LOG.stdin" ;;
esac
case "$*" in
    *"connection up"*)   [ -n "${FAKE_UP_FAILS:-}" ] && { echo "Error: Connection activation failed: (7) Secrets were required, but not provided."; exit 4; } ;;
    *"connection add"*)  [ -n "${FAKE_ADD_FAILS:-}" ] && { echo "Error: Failed to add"; exit 2; } ;;
esac
exit 0
EOF
chmod +x "$T/bin/nmcli"
export PATH="$T/bin:$PATH" FAKE_LOG="$T/log"
wifi() { rm -f "$T/log" "$T/log.stdin"; touch "$T/log"; "$ROOT/scripts/wifi.sh" "$@"; }

# 1. secured network: password only on stdin, never in the arguments; connection saved + activated
out=$(printf 'p@ss word;\\1 "q"\n' | wifi connect 'Cafe Wifi' wpa-psk 2>&1); rc=$?
check '[ $rc -eq 0 ] && [ "${out##*$'"'"'\n'"'"'}" = OK ]' "secured connect succeeds (rc=$rc out=$out)"
check '! grep -q "p@ss" "$T/log"' "the password is not in any nmcli argument"
check 'grep -q "^set wifi-sec.psk p@ss word;\\\\1 \"q\"$" "$T/log.stdin"' "the password reaches nmcli through stdin"
check 'grep -q "connection add type wifi con-name ely-wifi Cafe Wifi ssid Cafe Wifi" "$T/log"' "connection added with the SSID"
check 'grep -q "key-mgmt wpa-psk" "$T/log"' "wpa-psk key management"
check 'grep -q "connection up id ely-wifi Cafe Wifi" "$T/log"' "connection activated"

# 2. SAE
printf 'longenoughpw\n' | wifi connect Home sae > /dev/null 2>&1
check 'grep -q "key-mgmt sae" "$T/log"' "sae key management"

# 2b. hidden network
printf 'longenoughpw\n' | wifi connect Secret wpa-psk hidden > /dev/null 2>&1
check 'grep -q "802-11-wireless.hidden yes" "$T/log"' "hidden network is flagged"
printf 'longenoughpw\n' | wifi connect Plain wpa-psk > /dev/null 2>&1
check '! grep -q "hidden" "$T/log"' "visible network is not flagged hidden"
printf 'x\n' | wifi connect Plain wpa-psk bogus > /dev/null 2>&1; check '[ $? -eq 2 ]' "unknown option is rejected"

# 3. open network: no key management, no editor
out=$(printf '\n' | wifi connect Guest 2>&1); rc=$?
check '[ $rc -eq 0 ]' "open connect succeeds"
check '! grep -q "key-mgmt\|connection edit" "$T/log"' "open network has no security settings"
out=$(wifi connect Guest < /dev/null 2>&1); rc=$?
check '[ $rc -eq 0 ]' "open connect with no stdin succeeds"

# 4. activation fails (wrong password): the new connection is deleted again, rc=1, message kept
out=$(printf 'wrongpassword\n' | FAKE_UP_FAILS=1 wifi connect Cafe 2>&1); rc=$?
check '[ $rc -eq 1 ]' "failed activation returns 1"
check 'echo "$out" | grep -q "Secrets were required"' "the nmcli message is passed on"
check 'tail -n 3 "$T/log" | grep -q "connection delete id ely-wifi Cafe"' "the failed connection is deleted"

# 5. nmcli cannot add the connection
out=$(printf 'wrongpassword\n' | FAKE_ADD_FAILS=1 wifi connect Cafe 2>&1); rc=$?
check '[ $rc -eq 1 ] && ! grep -q "connection up" "$T/log"' "add failure stops before activation"

# 6. invalid input
for bad in "short" "$(printf 'x%.0s' $(seq 1 64))"; do
    out=$(printf '%s\n' "$bad" | wifi connect Cafe 2>&1); rc=$?
    check '[ $rc -eq 2 ] && ! grep -q "connection add" "$T/log"' "password of ${#bad} characters is rejected"
done
printf '%064d\n' 0 | wifi connect Cafe > /dev/null 2>&1
check 'grep -q "connection up" "$T/log"' "a 64 digit hex key is accepted"
wifi connect '-oops' < /dev/null > /dev/null 2>&1; check '[ $? -eq 2 ]' "SSID starting with - is rejected"
wifi connect "$(printf 'x%.0s' $(seq 1 33))" < /dev/null > /dev/null 2>&1; check '[ $? -eq 2 ]' "SSID longer than 32 is rejected"
wifi connect '' < /dev/null > /dev/null 2>&1; check '[ $? -eq 2 ]' "empty SSID is rejected"
wifi bogus x < /dev/null > /dev/null 2>&1; check '[ $? -eq 2 ]' "unknown mode is rejected"
wifi connect x badsec < /dev/null > /dev/null 2>&1; check '[ $? -eq 2 ]' "unknown security type is rejected"

# 7. forget
out=$(wifi forget 'Cafe Wifi' 2>&1); rc=$?
check '[ $rc -eq 0 ] && grep -q "connection delete id ely-wifi Cafe Wifi" "$T/log"' "forget deletes the connection"

[ $failures -eq 0 ] && echo "wifi.sh: ALL TESTS PASSED"
exit $failures
