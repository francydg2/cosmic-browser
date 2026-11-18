#!/bin/sh
set -u

target=$1
image=$2
pid=$3
helper=$4
parent=${target%/*}
work=
mounted=

cleanup() {
    if [ -n "$mounted" ]; then
        /usr/bin/hdiutil detach "$mounted" -quiet || true
    fi
    /bin/rm -f -- "$image" "$helper"
}

fail() {
    cleanup
    /usr/bin/osascript -e 'on run argv' \
        -e 'display dialog (item 1 of argv) buttons {"OK"} default button "OK" with title "Cosmic update"' \
        -e 'end run' -- "$1" || true
    if [ -d "$target" ]; then
        /usr/bin/open "$target" || true
    fi
    exit 1
}

work=$(/usr/bin/mktemp -d "$parent/.cosmic-update.XXXXXXXX") \
    || fail "Could not create an update directory beside Cosmic."
/bin/mkdir "$work/mount" || fail "Could not prepare the update."
/usr/bin/hdiutil attach "$image" -nobrowse -readonly -mountpoint "$work/mount" -quiet \
    || fail "Could not open the downloaded disk image."
mounted=$work/mount
source="$mounted/Cosmic.app"
identifier=$(/usr/libexec/PlistBuddy -c 'Print :CFBundleIdentifier' \
    "$source/Contents/Info.plist" 2>/dev/null) || fail "The update is not a valid Cosmic application."
[ "$identifier" = "app.cosmic.browser" ] && [ -x "$source/Contents/MacOS/cosmic" ] \
    || fail "The update is not a valid Cosmic application."
/usr/bin/ditto "$source" "$work/Cosmic.app" || fail "Could not prepare the new Cosmic application."
/usr/bin/hdiutil detach "$mounted" -quiet || fail "Could not safely unmount the update."
mounted=

count=0
while kill -0 "$pid" 2>/dev/null; do
    count=$((count + 1))
    [ "$count" -le 120 ] || fail "Cosmic did not close; the update was not installed."
    /bin/sleep 1
done

/bin/mv "$target" "$work/previous.app" || fail "Could not move the existing Cosmic application."
if ! /bin/mv "$work/Cosmic.app" "$target"; then
    if ! /bin/mv "$work/previous.app" "$target"; then
        fail "The update failed. Your previous Cosmic application is at $work/previous.app."
    fi
    fail "The update failed; the previous Cosmic application was restored."
fi
cleanup
/usr/bin/open "$target" || fail "The update was installed, but Cosmic could not restart."
