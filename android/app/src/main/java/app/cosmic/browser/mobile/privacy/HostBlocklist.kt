package app.cosmic.browser.mobile.privacy

import java.util.Locale

/**
 * Domain-level request blocking, matching the desktop browser's bundled list.
 * This is not an EasyList or cosmetic-filtering engine.
 */
class HostBlocklist(lines: Sequence<String>) {
    private val domains = lines
        .map { it.trim().lowercase(Locale.ROOT) }
        .filter { it.isNotEmpty() && !it.startsWith("#") }
        .toSet()

    fun blocks(host: String?): Boolean {
        var candidate = host?.trimEnd('.')?.lowercase(Locale.ROOT) ?: return false
        while (candidate.isNotEmpty()) {
            if (candidate in domains) return true
            val separator = candidate.indexOf('.')
            if (separator < 0) break
            candidate = candidate.substring(separator + 1)
        }
        return false
    }
}
