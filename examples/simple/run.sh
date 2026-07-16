#!/bin/sh
set -e
cd "$(dirname "$0")"

uniform=../..
wasm_clang=/opt/homebrew/opt/llvm/bin/clang
wasm_sysroot=$uniform/external/wasi-libc-27.0/wasm32-wasip1-threads
wasm_builtins=$uniform/external/compiler-rt-22.1.8/wasm32-wasip1-threads/lib

mkdir -p build/macos build/ios build/wasm

if [ "$1" = macos ]; then
    xcrun clang -O2 -I$uniform/include main.c $uniform/lib/macos-release/libuniform.a -framework AppKit -framework Metal -framework QuartzCore -o build/macos/simple
    build/macos/simple
elif [ "$1" = ios ]; then
    if [ -z "$DEVICE" ]; then
        echo "set DEVICE to the name or identifier of a connected device, see xcrun devicectl list devices"
        exit 1
    fi

    app=build/ios/simple.app
    rm -rf $app && mkdir $app
    xcrun -sdk iphoneos clang -target arm64-apple-ios16.0 -O2 -I$uniform/include main.c $uniform/lib/ios-release/libuniform.a -framework Foundation -framework CoreGraphics -framework UIKit -framework Metal -framework QuartzCore -o $app/simple
    cat > $app/Info.plist <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>CFBundleExecutable</key>
	<string>simple</string>
	<key>CFBundleIdentifier</key>
	<string>com.example.simple</string>
	<key>CFBundleName</key>
	<string>simple</string>
	<key>CFBundlePackageType</key>
	<string>APPL</string>
	<key>CFBundleShortVersionString</key>
	<string>1.0</string>
	<key>CFBundleVersion</key>
	<string>1</string>
	<key>CFBundleSupportedPlatforms</key>
	<array>
		<string>iPhoneOS</string>
	</array>
	<key>MinimumOSVersion</key>
	<string>16.0</string>
	<key>UIDeviceFamily</key>
	<array>
		<integer>1</integer>
		<integer>2</integer>
	</array>
	<key>UILaunchScreen</key>
	<dict/>
</dict>
</plist>
EOF

    profiles="$HOME/Library/Developer/Xcode/UserData/Provisioning Profiles"
    profile=$(cd "$profiles" && grep -laF "<string>iOS Team Provisioning Profile: *</string>" $(ls -t) | head -1)
    if [ -z "$profile" ]; then
        echo "no team provisioning profile in $profiles, sign in to a team in Xcode"
        exit 1
    fi
    cp "$profiles/$profile" $app/embedded.mobileprovision
    team=$(security cms -D -i $app/embedded.mobileprovision | plutil -extract TeamIdentifier.0 raw -o - -)
    cat > build/ios/simple.entitlements <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
	<key>application-identifier</key>
	<string>$team.com.example.simple</string>
	<key>com.apple.developer.team-identifier</key>
	<string>$team</string>
	<key>get-task-allow</key>
	<true/>
</dict>
</plist>
EOF
    codesign --force --sign "Apple Development" --entitlements build/ios/simple.entitlements $app

    xcrun devicectl device install app --device "$DEVICE" $app
    xcrun devicectl device process launch --console --terminate-existing --device "$DEVICE" com.example.simple
elif [ "$1" = wasm ]; then
    $wasm_clang --target=wasm32-wasip1-threads -pthread -msimd128 -O2 --no-default-config --sysroot=$wasm_sysroot -I$uniform/include -nostdlib main.c $uniform/lib/wasm-release/libuniform.a \
        -Wl,$wasm_sysroot/lib/crt1-reactor.o -Wl,--entry=_initialize -L$wasm_sysroot/lib -L$wasm_builtins -lc -lpthread -lsetjmp -lclang_rt.builtins \
        -Wl,--import-memory -Wl,--max-memory=2147483648 -Wl,--export-table -Wl,--export-if-defined=main -Wl,--export-if-defined=__main_argc_argv -Wl,--export=malloc -Wl,--export=free \
        -o build/wasm/index.wasm

    cp $uniform/lib/wasm-release/lib.js build/wasm/
    cat > build/wasm/index.html <<EOF
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="color-scheme" content="dark light">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<link rel="icon" href="data:,">
<title>simple</title>
<style>
html, body { margin: 0px; overflow: hidden; display: block; -webkit-touch-callout: none; -webkit-user-select: none; user-select: none; touch-action: none; -webkit-tap-highlight-color: transparent; }
canvas { display: block; }
</style>
</head>
<body>
<script src="lib.js"></script>
</body>
</html>
EOF

    xcrun clang -O2 -I$uniform/include server.c $uniform/lib/macos-release/libuniform.a -o build/server
    build/server
else
    echo "usage: $0 macos|ios|wasm"
    exit 1
fi
