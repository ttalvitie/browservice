#!/bin/bash
set -e

APP_NAME="browservice"
APP_DIR="$APP_NAME.app"
CONTENTS_DIR="$APP_DIR/Contents"
MACOS_DIR="$CONTENTS_DIR/MacOS"
FRAMEWORKS_DIR="$CONTENTS_DIR/Frameworks"

echo "Creating App Bundle structure..."
rm -rf "$APP_DIR"
mkdir -p "$MACOS_DIR"
mkdir -p "$FRAMEWORKS_DIR"

echo "Copying executable..."
cp "$APP_NAME" "$MACOS_DIR/"

echo "Copying Framework..."
cp -R "Chromium Embedded Framework.framework" "$FRAMEWORKS_DIR/"

echo "Copying Helper Apps..."
HELPER_BASE="libcef_dll_wrapper/tests/cefsimple/Release"

# List of suffixes for helpers. Empty string is the main helper.
SUFFIXES=("" " (Alerts)" " (GPU)" " (Plugin)" " (Renderer)")

for SUFFIX in "${SUFFIXES[@]}"; do
    SRC_NAME="cefsimple Helper${SUFFIX}"
    DEST_NAME="browservice Helper${SUFFIX}"
    HELPER_SRC="$HELPER_BASE/$SRC_NAME.app"
    HELPER_DEST="$FRAMEWORKS_DIR/$DEST_NAME.app"
    
    # Sanitize suffix for Bundle ID (e.g. " (GPU)" -> ".gpu")
    # Convert to lowercase and remove non-alphanumeric chars (except dot if needed, but simple removal works)
    # We want org.browservice.app.helper.gpu
    ID_SUFFIX=$(echo "$SUFFIX" | tr '[:upper:]' '[:lower:]' | tr -d ' ()')
    if [ -z "$ID_SUFFIX" ]; then
        BUNDLE_ID="org.browservice.app.helper"
    else
        BUNDLE_ID="org.browservice.app.helper.$ID_SUFFIX"
    fi

    if [ -d "$HELPER_SRC" ]; then
        echo "Processing $DEST_NAME..."
        cp -R "$HELPER_SRC" "$HELPER_DEST"
        
        # Update Info.plist
        /usr/libexec/PlistBuddy -c "Set :CFBundleName $DEST_NAME" "$HELPER_DEST/Contents/Info.plist"
        /usr/libexec/PlistBuddy -c "Set :CFBundleExecutable $DEST_NAME" "$HELPER_DEST/Contents/Info.plist"
        /usr/libexec/PlistBuddy -c "Set :CFBundleIdentifier $BUNDLE_ID" "$HELPER_DEST/Contents/Info.plist"
        
        # Fix Framework load path for Helper
        # Point to @executable_path/../../../Chromium Embedded Framework.framework/... (in main bundle)
        install_name_tool -change "@executable_path/../Frameworks/Chromium Embedded Framework.framework/Chromium Embedded Framework" "@executable_path/../../../Chromium Embedded Framework.framework/Chromium Embedded Framework" "$HELPER_DEST/Contents/MacOS/$SRC_NAME"

        # Rename executable
        mv "$HELPER_DEST/Contents/MacOS/$SRC_NAME" "$HELPER_DEST/Contents/MacOS/$DEST_NAME"

        # Re-sign the helper app
        codesign --force --deep --sign - "$HELPER_DEST"
    else
        echo "WARNING: Helper app not found at $HELPER_SRC"
    fi
done

echo "Copying Plugins..."
PLUGINS_DIR="$CONTENTS_DIR/PlugIns"
mkdir -p "$PLUGINS_DIR"
# Check where the plugin is located. It might be in viceplugins/retrojsvice or the current dir depending on build
if [ -f "viceplugins/retrojsvice/retrojsvice.so" ]; then
    cp "viceplugins/retrojsvice/retrojsvice.so" "$PLUGINS_DIR/"
elif [ -f "retrojsvice.so" ]; then
    cp "retrojsvice.so" "$PLUGINS_DIR/"
else
    echo "WARNING: retrojsvice.so not found!"
fi

echo "Creating Info.plist..."
cat > "$CONTENTS_DIR/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>$APP_NAME</string>
    <key>CFBundleIdentifier</key>
    <string>org.browservice.app</string>
    <key>CFBundleName</key>
    <string>$APP_NAME</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>LSMinimumSystemVersion</key>
    <string>10.15</string>
</dict>
</plist>
EOF

echo "App Bundle created at $APP_DIR"
echo "You can run it via: open $APP_DIR"
echo "Or executable directly: $MACOS_DIR/$APP_NAME"
