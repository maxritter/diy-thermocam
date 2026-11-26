#!/bin/sh
# Adds the signed, notarized Mac app to a draft release from .github/workflows/release.yml and publishes it.
# Use it when the repository has no Mac signing secrets, so the workflow stopped at a draft.
#
#   ASC_KEY_ID=… ASC_ISSUER_ID=… ASC_KEY_PATH=/path/AuthKey_….p8 scripts/release-mac.sh v4.0.1 [--no-publish]
#
# Needs: a Mac with the "Developer ID Application" identity in the keychain, Rust with the aarch64 and x86_64
# Apple targets, bun, and gh signed in to an account that can edit releases. The working tree must be clean
# and checked out at the tag, so the app matches the rest of the release.
set -eu

tag="${1:?usage: scripts/release-mac.sh vX.Y.Z [--no-publish]}"
publish=true
[ "${2:-}" = "--no-publish" ] && publish=false
repo=maxritter/diy-thermocam
dmg_name=DIY-Thermocam-Studio-macOS.dmg
: "${ASC_KEY_ID:?set ASC_KEY_ID to the App Store Connect key id}"
: "${ASC_ISSUER_ID:?set ASC_ISSUER_ID to the App Store Connect issuer id}"
: "${ASC_KEY_PATH:?set ASC_KEY_PATH to the .p8 file}"

root=$(git rev-parse --show-toplevel)
cd "$root"
[ -z "$(git status --porcelain -- app)" ] || { echo "app/ has uncommitted changes" >&2; exit 1; }
[ "$(git rev-parse HEAD)" = "$(git rev-parse "$tag^{commit}")" ] || { echo "HEAD is not $tag; run: git checkout $tag" >&2; exit 1; }
version=$(sed -n 's/^  "version": "\(.*\)",$/\1/p' app/package.json | head -n 1)
[ "v$version" = "$tag" ] || { echo "app/package.json is $version, not ${tag#v}" >&2; exit 1; }
[ "$(gh release view "$tag" -R "$repo" --json isDraft -q .isDraft)" = true ] || { echo "$tag is not a draft release" >&2; exit 1; }

identity=$(security find-identity -v -p codesigning | sed -n 's/.*"\(Developer ID Application: [^"]*\)".*/\1/p' | head -n 1)
[ -n "$identity" ] || { echo "no Developer ID Application identity in the keychain" >&2; exit 1; }
echo "Signing as $identity"

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

(
  cd app
  bun install --frozen-lockfile
  CI=true APPLE_SIGNING_IDENTITY="$identity" APPLE_API_KEY="$ASC_KEY_ID" APPLE_API_ISSUER="$ASC_ISSUER_ID" \
    APPLE_API_KEY_PATH="$ASC_KEY_PATH" bunx @tauri-apps/cli@2.12.1 build --target universal-apple-darwin --bundles app,dmg
)
app="app/target/universal-apple-darwin/release/bundle/macos/DIY-Thermocam Studio.app"
cp app/target/universal-apple-darwin/release/bundle/dmg/*.dmg "$work/$dmg_name"

codesign --force --timestamp --sign "$identity" "$work/$dmg_name"
xcrun notarytool submit "$work/$dmg_name" --key "$ASC_KEY_PATH" --key-id "$ASC_KEY_ID" --issuer "$ASC_ISSUER_ID" --wait
for attempt in 1 2 3 4 5; do xcrun stapler staple "$work/$dmg_name" && break; sleep $((20 * attempt)); done
xcrun stapler validate "$work/$dmg_name"
codesign --verify --deep --strict "$app"
spctl -a -t exec "$app"
spctl -a -t open --context context:primary-signature "$work/$dmg_name"
[ "$(lipo -archs "$app/Contents/MacOS/diy-thermocam")" = "x86_64 arm64" ]

# Checksums for every file in the release, the Mac app included.
gh release download "$tag" -R "$repo" --dir "$work/assets" --skip-existing
cp "$work/$dmg_name" "$work/assets/"
rm -f "$work/assets/SHA256SUMS.txt"
(cd "$work/assets" && shasum -a 256 -- * > SHA256SUMS.txt && cat SHA256SUMS.txt)
gh release upload "$tag" -R "$repo" --clobber "$work/assets/$dmg_name" "$work/assets/SHA256SUMS.txt"

mkdir "$work/check"
gh release download "$tag" -R "$repo" --dir "$work/check"
(cd "$work/check" && shasum -a 256 -c SHA256SUMS.txt)

if [ "$publish" = true ]; then
  gh release edit "$tag" -R "$repo" --draft=false --latest
  echo "Published $tag"
else
  echo "$tag stays a draft with the Mac app added"
fi
