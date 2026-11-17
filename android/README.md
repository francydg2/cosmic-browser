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
