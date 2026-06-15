#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
app_name="Claude Light"
bundle_id="com.thebigboy.claude-light"
dist_dir="${repo_root}/dist"
app_dir="${dist_dir}/${app_name}.app"
contents_dir="${app_dir}/Contents"
macos_dir="${contents_dir}/MacOS"
resources_dir="${contents_dir}/Resources"
binary_name="claude-light"
icon_source="${repo_root}/assets/AppIcon.png"
iconset_dir="${dist_dir}/AppIcon.iconset"
icon_name="AppIcon"
icon_file="${icon_name}.icns"

cd "$repo_root"

swift build -c release

rm -rf "$app_dir"
mkdir -p "$macos_dir" "$resources_dir"

cp ".build/release/${binary_name}" "${macos_dir}/${binary_name}"
chmod +x "${macos_dir}/${binary_name}"

if [[ -f "$icon_source" ]]; then
  rm -rf "$iconset_dir"
  mkdir -p "$iconset_dir"

  sips -z 16 16 "$icon_source" --out "${iconset_dir}/icon_16x16.png" >/dev/null
  sips -z 32 32 "$icon_source" --out "${iconset_dir}/icon_16x16@2x.png" >/dev/null
  sips -z 32 32 "$icon_source" --out "${iconset_dir}/icon_32x32.png" >/dev/null
  sips -z 64 64 "$icon_source" --out "${iconset_dir}/icon_32x32@2x.png" >/dev/null
  sips -z 128 128 "$icon_source" --out "${iconset_dir}/icon_128x128.png" >/dev/null
  sips -z 256 256 "$icon_source" --out "${iconset_dir}/icon_128x128@2x.png" >/dev/null
  sips -z 256 256 "$icon_source" --out "${iconset_dir}/icon_256x256.png" >/dev/null
  sips -z 512 512 "$icon_source" --out "${iconset_dir}/icon_256x256@2x.png" >/dev/null
  sips -z 512 512 "$icon_source" --out "${iconset_dir}/icon_512x512.png" >/dev/null
  sips -z 1024 1024 "$icon_source" --out "${iconset_dir}/icon_512x512@2x.png" >/dev/null

  iconutil -c icns "$iconset_dir" -o "${resources_dir}/${icon_file}"
  rm -rf "$iconset_dir"
fi

cat > "${contents_dir}/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
  <key>CFBundleDevelopmentRegion</key>
  <string>zh_CN</string>
  <key>CFBundleExecutable</key>
  <string>${binary_name}</string>
  <key>CFBundleIdentifier</key>
  <string>${bundle_id}</string>
  <key>CFBundleIconFile</key>
  <string>${icon_name}</string>
  <key>CFBundleIconName</key>
  <string>${icon_name}</string>
  <key>CFBundleInfoDictionaryVersion</key>
  <string>6.0</string>
  <key>CFBundleName</key>
  <string>${app_name}</string>
  <key>CFBundleDisplayName</key>
  <string>${app_name}</string>
  <key>CFBundlePackageType</key>
  <string>APPL</string>
  <key>CFBundleShortVersionString</key>
  <string>0.1.0</string>
  <key>CFBundleVersion</key>
  <string>2</string>
  <key>LSMinimumSystemVersion</key>
  <string>13.0</string>
  <key>LSUIElement</key>
  <true/>
  <key>NSHighResolutionCapable</key>
  <true/>
  <key>NSBluetoothAlwaysUsageDescription</key>
  <string>Connect to the Claude Light ESP32 status indicator.</string>
</dict>
</plist>
PLIST

echo "Built ${app_dir}"
touch "$app_dir"
