package app.cosmic.browser.mobile

import java.net.URI
import java.net.URISyntaxException

internal fun validSearchTemplate(template: String): Boolean {
    if (!template.contains("%1")) return false
    return try {
        val uri = URI(template.replace("%1", "cosmicquery"))
        (uri.scheme.equals("https", true) || uri.scheme.equals("http", true)) &&
            !uri.host.isNullOrBlank() && uri.userInfo == null
    } catch (_: URISyntaxException) {
        false
    }
}
