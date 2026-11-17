package app.cosmic.browser.mobile.privacy

import android.content.Context
import android.net.Uri
import android.webkit.WebResourceResponse
import java.io.ByteArrayInputStream

class RequestProtection(context: Context) {
    private val preferences = context.getSharedPreferences("cosmic_privacy", Context.MODE_PRIVATE)
    private val blocker = context.assets.open("blocklist.txt").bufferedReader().use { reader ->
        HostBlocklist(reader.lineSequence())
    }

    @Volatile
    var enabled: Boolean = preferences.getBoolean("blockKnownHosts", true)
        private set

    fun setEnabled(value: Boolean) {
        preferences.edit().putBoolean("blockKnownHosts", value).apply()
        enabled = value
    }

    fun intercept(uri: Uri): WebResourceResponse? {
        if (!enabled || (!uri.scheme.equals("http", ignoreCase = true)
                    && !uri.scheme.equals("https", ignoreCase = true))
            || !blocker.blocks(uri.host)) return null
        return WebResourceResponse("text/plain", "utf-8", ByteArrayInputStream(ByteArray(0)))
    }
}
