# Cosmic for Android (beta)

This is a separate, native Android frontend for the Cosmic browser. It keeps
Cosmic's dark charcoal and violet visual style while using a Chrome-inspired
tab grid and Android's system WebView.

The Compose preview implements browsing and search, multiple tabs, bookmarks,
history, built-in and custom search engines, configurable session restore, system downloads and
controls for clearing site data. A switch enables blocking requests to domains
in the bundled desktop host list. WebView request interception has limitations
and this is not an EasyList/EasyPrivacy or cosmetic-filtering engine.

The tools menu includes tab search, a text-only reader, translation and locally
generated page QR codes. Reader mode extracts text and may not work on every
site. Translation opens Google Translate in a new tab after confirmation and
sends it the page address, including query parameters; authenticated pages may
not be accessible. URLs used for translation and QR codes omit user credentials.

The Android version remains incomplete: isolated private tabs and profiles,
tab groups, an integrated password vault and themes are
not yet available. The Compose preview has not yet been verified on a device.
Settings use the desktop categories and explain which desktop options are not
yet supported on Android.

Add the **Ricerca Cosmic** widget from the Android home screen's widget picker.
Tapping it opens Cosmic and focuses the address bar; searches use the selected
search engine.

On startup and from Settings > Updates, the app checks GitHub releases for a
newer Android APK, including beta versions. With your consent it downloads the
APK, verifies its SHA-256, package ID, version code and signing certificate,
then asks Android to install it. Android may require permission to install
unknown apps and always asks for confirmation. Checks contact the GitHub API
and reveal the device's IP address to GitHub. GitHub-hosted direct distribution
is separate from Google Play; Play policies restrict self-updating apps.
The previously published beta 3 does not contain this updater, so an APK with
the updater must first be installed manually over it using the same signing
key. If Android restarts Cosmic while granting installation permission, check
for and download the update again from Settings > Updates.

Release APKs must use a higher `versionCode`, a unique `versionName` in the
form `X.Y.Z`, `X.Y.Z-beta` or `X.Y.Z-betaN`, and the matching asset name
`cosmic-X.Y.Z-android[-betaN].apk`. The release tag must be `vX.Y.Z` or the
full version, and GitHub must report the asset's SHA-256 digest.

## Build

Install JDK 17 or newer and Android SDK platform/build tools for API 35, then:

```sh
cd android
./gradlew assembleDebug
```

The preview APK is written to
`app/build/outputs/apk/debug/app-debug.apk`.

The browser uses Android System WebView for page rendering. JavaScript is
enabled for site compatibility; local-file access, mixed content, and
JavaScript-created windows are disabled.
