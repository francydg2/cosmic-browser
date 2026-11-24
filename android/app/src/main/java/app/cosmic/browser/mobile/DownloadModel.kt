package app.cosmic.browser.mobile

import org.json.JSONArray
import org.json.JSONException
import org.json.JSONObject
import java.util.Locale

internal enum class DownloadState { RUNNING, DONE, FAILED, CANCELLED }

internal enum class DownloadFilter(val label: String) {
    ALL("Tutti"), ACTIVE("Attivi"), DONE("Completati"), FAILED("Falliti")
}

internal fun DownloadFilter.accepts(state: DownloadState): Boolean = when (this) {
    DownloadFilter.ALL -> true
    DownloadFilter.ACTIVE -> state == DownloadState.RUNNING
    DownloadFilter.DONE -> state == DownloadState.DONE
    DownloadFilter.FAILED -> state == DownloadState.FAILED || state == DownloadState.CANCELLED
}

internal fun matchesQuery(name: String, url: String, query: String): Boolean {
    val needle = query.trim()
    return needle.isEmpty() || name.contains(needle, ignoreCase = true) ||
        url.contains(needle, ignoreCase = true)
}

/** Rolling transfer-rate estimate so a stalled download does not show a stale average. */
internal class SpeedTracker(private val windowMs: Long = 5000) {
    private val samples = ArrayDeque<Pair<Long, Long>>()

    fun sample(timeMs: Long, bytes: Long) {
        val last = samples.lastOrNull()
        if (last != null && (timeMs < last.first || bytes < last.second)) samples.clear()
        samples.addLast(timeMs to bytes)
        while (samples.size > 2 && timeMs - samples.first().first > windowMs) samples.removeFirst()
    }

    fun bytesPerSecond(): Double {
        if (samples.size < 2) return 0.0
        val elapsed = samples.last().first - samples.first().first
        return if (elapsed > 0) (samples.last().second - samples.first().second) * 1000.0 / elapsed else 0.0
    }

    fun secondsRemaining(total: Long, done: Long): Long {
        val speed = bytesPerSecond()
        if (total <= 0 || speed < 1 || done > total) return -1
        return ((total - done) / speed + 0.5).toLong()
    }
}

internal data class SavedDownload(
    val id: Long,
    val name: String,
    val url: String,
    val mime: String?,
    val state: DownloadState,
    val total: Long,
    val done: Long,
    val uri: String?,
    val error: String,
    val finishedMs: Long
)

internal const val MAX_SAVED_DOWNLOADS = 300

internal fun savedToJson(items: List<SavedDownload>): String {
    val array = JSONArray()
    for (item in items.take(MAX_SAVED_DOWNLOADS)) {
        array.put(JSONObject().apply {
            put("id", item.id)
            put("name", item.name)
            put("url", item.url)
            put("mime", item.mime ?: JSONObject.NULL)
            put("state", item.state.name)
            put("total", item.total)
            put("done", item.done)
            put("uri", item.uri ?: JSONObject.NULL)
            put("error", item.error)
            put("finished", item.finishedMs)
        })
    }
    return JSONObject().put("version", 1).put("downloads", array).toString()
}

/** Reads the saved list; a download that was still running when the app died is shown as interrupted. */
internal fun savedFromJson(text: String): List<SavedDownload> {
    val result = ArrayList<SavedDownload>()
    try {
        val array = JSONObject(text).optJSONArray("downloads") ?: return emptyList()
        for (i in 0 until array.length()) {
            val o = array.optJSONObject(i) ?: continue
            val name = o.optString("name")
            if (name.isEmpty() || result.size >= MAX_SAVED_DOWNLOADS) continue
            val state = runCatching { DownloadState.valueOf(o.optString("state")) }.getOrNull() ?: continue
            val interrupted = state == DownloadState.RUNNING
            result += SavedDownload(
                id = o.optLong("id"),
                name = name,
                url = o.optString("url"),
                mime = if (o.isNull("mime")) null else o.optString("mime"),
                state = if (interrupted) DownloadState.FAILED else state,
                total = o.optLong("total", -1),
                done = o.optLong("done"),
                uri = if (o.isNull("uri")) null else o.optString("uri"),
                error = if (interrupted) "interrotto alla chiusura dell'app" else o.optString("error"),
                finishedMs = o.optLong("finished")
            )
        }
    } catch (_: JSONException) {
        return emptyList()
    }
    return result
}

internal fun formatSize(bytes: Long): String {
    if (bytes < 1024) return "$bytes B"
    val units = arrayOf("KB", "MB", "GB", "TB")
    var value = bytes / 1024.0
    var unit = 0
    while (value >= 1024 && unit < units.size - 1) {
        value /= 1024
        unit++
    }
    return String.format(Locale.ITALIAN, "%.1f %s", value, units[unit])
}

internal fun formatSpeed(bytesPerSecond: Double): String =
    if (bytesPerSecond < 1) "" else formatSize(bytesPerSecond.toLong()) + "/s"

internal fun formatDuration(seconds: Long): String = when {
    seconds < 0 -> "—"
    seconds < 60 -> "$seconds s"
    seconds < 3600 -> "${seconds / 60} min ${seconds % 60} s"
    else -> "${seconds / 3600} h ${(seconds % 3600) / 60} min"
}
