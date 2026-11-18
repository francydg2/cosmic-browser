package app.cosmic.browser.mobile

import java.net.URI
import java.io.IOException
import org.json.JSONArray

internal data class UpdateVersion(
    val major: Int, val minor: Int, val patch: Int, val beta: Int?
) : Comparable<UpdateVersion> {
    override fun compareTo(other: UpdateVersion): Int =
        compareValuesBy(this, other, { it.major }, { it.minor }, { it.patch }, { it.beta ?: Int.MAX_VALUE })

    companion object {
        fun parse(value: String): UpdateVersion? {
            val match = Regex("""^(\d+)\.(\d+)\.(\d+)(?:-beta(\d*))?$""").matchEntire(value)
                ?: return null
            val major = match.groupValues[1].toIntOrNull() ?: return null
            val minor = match.groupValues[2].toIntOrNull() ?: return null
            val patch = match.groupValues[3].toIntOrNull() ?: return null
            val betaGroup = match.groups[4]
            val beta = if (betaGroup == null) null else if (betaGroup.value.isEmpty()) 1
                else betaGroup.value.toIntOrNull()?.takeIf { it > 0 } ?: return null
            if (beta == Int.MAX_VALUE) return null
            return UpdateVersion(major, minor, patch, beta)
        }
    }
}

internal data class UpdateCandidate(
    val version: UpdateVersion,
    val versionName: String,
    val url: String,
    val sha256: String,
    val size: Long
)

internal const val MAX_UPDATE_BYTES = 300L * 1024 * 1024

internal fun candidateFromAsset(
    tag: String, name: String, url: String, digest: String, size: Long
): UpdateCandidate? {
    val match = Regex("""^cosmic-(\d+\.\d+\.\d+)-android(?:-(beta\d*))?\.apk$""").matchEntire(name)
        ?: return null
    val base = match.groupValues[1]
    val suffix = match.groups[2]?.value
    val versionName = if (suffix == null) base else "$base-$suffix"
    val version = UpdateVersion.parse(versionName) ?: return null
    if (tag != "v$base" && tag != "v$versionName") return null
    val uri = try { URI(url) } catch (_: java.net.URISyntaxException) { return null }
    if (uri.scheme != "https" || uri.host != "github.com" || uri.port != -1 ||
        uri.userInfo != null || uri.query != null || uri.fragment != null ||
        uri.rawPath != "/francydg2/cosmic-browser/releases/download/$tag/$name") return null
    if (!Regex("""^sha256:[0-9a-fA-F]{64}$""").matches(digest) ||
        size !in 1..MAX_UPDATE_BYTES) return null
    return UpdateCandidate(version, versionName, url, digest.substringAfter(':').lowercase(), size)
}

internal fun newestUpdate(json: String, current: UpdateVersion): UpdateCandidate? {
    val releases = JSONArray(json)
    val candidates = mutableListOf<UpdateCandidate>()
    for (i in 0 until releases.length()) {
        val release = releases.getJSONObject(i)
        if (release.optBoolean("draft", false)) continue
        val tag = release.optString("tag_name")
        val assets = release.optJSONArray("assets") ?: continue
        for (j in 0 until assets.length()) {
            val asset = assets.getJSONObject(j)
            val name = asset.optString("name")
            val match = Regex("""^cosmic-(\d+\.\d+\.\d+)-android(?:-(beta\d*))?\.apk$""")
                .matchEntire(name) ?: continue
            val versionText = match.groupValues[1] +
                (match.groups[2]?.value?.let { "-$it" } ?: "")
            val version = UpdateVersion.parse(versionText) ?: continue
            if (version <= current) continue
            val entry = candidateFromAsset(tag, name, asset.optString("browser_download_url"),
                asset.optString("digest"), asset.optLong("size"))
                ?: throw IOException("Metadati del pacchetto Android non validi.")
            if (candidates.any { it.version == entry.version }) {
                throw IOException("Pacchetti Android duplicati per la stessa versione.")
            }
            candidates += entry
        }
    }
    return candidates.maxByOrNull { it.version }
}
