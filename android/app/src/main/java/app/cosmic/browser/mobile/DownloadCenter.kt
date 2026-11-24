package app.cosmic.browser.mobile

import android.Manifest
import android.app.DownloadManager
import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.ContentValues
import android.content.Context
import android.content.Intent
import android.content.pm.PackageManager
import android.net.Uri
import android.os.Build
import android.os.Environment
import android.provider.MediaStore
import android.webkit.CookieManager
import android.webkit.URLUtil
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableLongStateOf
import androidx.compose.runtime.mutableStateListOf
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.Job
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.IOException
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.URL

internal class DownloadEntry(
    val id: Long,
    val name: String,
    val url: String,
    var mime: String? = null
) {
    var state by mutableStateOf(DownloadState.RUNNING)
    var done by mutableLongStateOf(0L)
    var total by mutableLongStateOf(-1L)
    var error by mutableStateOf("")
    var speed by mutableStateOf(0.0)
    var remaining by mutableLongStateOf(-1L)
    var uri: Uri? = null
    var finishedMs = 0L
    var job: Job? = null
    val tracker = SpeedTracker()
}

/**
 * In-app downloader: files go to the public Downloads folder, the list
 * (finished entries only) is kept in app storage across restarts.
 * Resuming a partial transfer is not supported; a cancelled or failed
 * download can be started again from the list.
 */
internal class DownloadCenter(private val context: Context) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val historyFile = File(context.filesDir, "downloads.json")
    private var nextId = 1L
    val items = mutableStateListOf<DownloadEntry>()
    var banner by mutableStateOf<String?>(null)

    init {
        loadHistory()
    }

    fun start(url: String, userAgent: String?, disposition: String?, mimeType: String?) {
        val parsed = Uri.parse(url)
        if (parsed.scheme != "http" && parsed.scheme != "https") {
            banner = "Download non supportato per questo tipo di indirizzo"
            return
        }
        val name = URLUtil.guessFileName(url, disposition, mimeType)
            .replace(Regex("[\\\\/:*?\"<>|\\u0000-\\u001F]"), "_").take(180).ifBlank { "download" }
        val entry = DownloadEntry(nextId++, name, url, mimeType)
        items.add(0, entry)
        banner = "Download avviato: $name"
        val cookie = CookieManager.getInstance().getCookie(url)
        entry.job = scope.launch {
            try {
                withContext(Dispatchers.IO) { run(entry, userAgent, cookie) }
                entry.state = DownloadState.DONE
                finish(entry, "Download completato: ${entry.name}")
            } catch (e: CancellationException) {
                entry.state = DownloadState.CANCELLED
                entry.finishedMs = System.currentTimeMillis()
                saveHistory()
                throw e
            } catch (e: Exception) {
                entry.state = DownloadState.FAILED
                entry.error = e.message ?: "errore di rete"
                finish(entry, "Download non riuscito: ${entry.name}")
            }
        }
    }

    fun cancel(entry: DownloadEntry) {
        if (entry.state == DownloadState.RUNNING) {
            entry.job?.cancel()
            banner = "Download annullato: ${entry.name}"
        }
    }

    fun retry(entry: DownloadEntry) {
        if (entry.state == DownloadState.RUNNING) return
        items.remove(entry)
        saveHistory()
        start(entry.url, null, null, entry.mime)
    }

    fun remove(entry: DownloadEntry) {
        if (entry.state == DownloadState.RUNNING) return
        items.remove(entry)
        saveHistory()
    }

    fun clearFinished() {
        items.removeAll { it.state != DownloadState.RUNNING }
        saveHistory()
    }

    fun close() {
        scope.coroutineContext[Job]?.cancel()
    }

    private fun finish(entry: DownloadEntry, message: String) {
        entry.finishedMs = System.currentTimeMillis()
        entry.speed = 0.0
        entry.remaining = -1
        banner = message
        saveHistory()
        notifyFinished(entry, message)
    }

    private fun run(entry: DownloadEntry, userAgent: String?, cookie: String?) {
        val connection = URL(entry.url).openConnection() as HttpURLConnection
        connection.connectTimeout = 20000
        connection.readTimeout = 30000
        userAgent?.takeIf { it.isNotBlank() }?.let { connection.setRequestProperty("User-Agent", it) }
        cookie?.let { connection.setRequestProperty("Cookie", it) }
        try {
            val code = connection.responseCode
            if (code !in 200..299) throw IOException("Server HTTP $code")
            entry.total = connection.contentLengthLong
            val mime = entry.mime?.takeIf { it.isNotBlank() }
                ?: connection.contentType?.substringBefore(';') ?: "application/octet-stream"
            entry.mime = mime
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) saveWithMediaStore(entry, connection, mime)
            else saveToPublicDirectory(entry, connection)
        } finally {
            connection.disconnect()
        }
    }

    private fun copy(entry: DownloadEntry, connection: HttpURLConnection, output: OutputStream) {
        connection.inputStream.use { input ->
            val buffer = ByteArray(64 * 1024)
            while (true) {
                if (entry.job?.isActive == false) throw CancellationException("annullato")
                val count = input.read(buffer)
                if (count < 0) break
                output.write(buffer, 0, count)
                entry.done += count
                entry.tracker.sample(System.currentTimeMillis(), entry.done)
                entry.speed = entry.tracker.bytesPerSecond()
                entry.remaining = entry.tracker.secondsRemaining(entry.total, entry.done)
            }
        }
        output.flush()
    }

    @androidx.annotation.RequiresApi(Build.VERSION_CODES.Q)
    private fun saveWithMediaStore(entry: DownloadEntry, connection: HttpURLConnection, mime: String) {
        val resolver = context.contentResolver
        val values = ContentValues().apply {
            put(MediaStore.Downloads.DISPLAY_NAME, entry.name)
            put(MediaStore.Downloads.MIME_TYPE, mime)
            put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS)
            put(MediaStore.Downloads.IS_PENDING, 1)
        }
        val target = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values)
            ?: throw IOException("Impossibile creare il file in Download")
        try {
            (resolver.openOutputStream(target) ?: throw IOException("Impossibile scrivere il file")).use {
                copy(entry, connection, it)
            }
            resolver.update(target, ContentValues().apply { put(MediaStore.Downloads.IS_PENDING, 0) }, null, null)
            entry.uri = target
        } catch (e: Exception) {
            resolver.delete(target, null, null)
            throw e
        }
    }

    @Suppress("DEPRECATION")
    private fun saveToPublicDirectory(entry: DownloadEntry, connection: HttpURLConnection) {
        val directory = Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS)
        if (!directory.exists() && !directory.mkdirs()) throw IOException("Cartella Download non disponibile")
        var file = File(directory, entry.name)
        var n = 1
        while (file.exists()) {
            val extension = entry.name.substringAfterLast('.', "").let { if (it.isEmpty()) "" else ".$it" }
            file = File(directory, "${entry.name.substringBeforeLast('.')} ($n)$extension")
            n++
        }
        try {
            file.outputStream().use { copy(entry, connection, it) }
        } catch (e: Exception) {
            file.delete()
            throw e
        }
        entry.uri = Uri.fromFile(file)
    }

    private fun loadHistory() {
        val text = try { historyFile.takeIf { it.isFile }?.readText() } catch (_: IOException) { null } ?: return
        for (saved in savedFromJson(text)) {
            val entry = DownloadEntry(saved.id, saved.name, saved.url, saved.mime)
            entry.state = saved.state
            entry.total = saved.total
            entry.done = saved.done
            entry.error = saved.error
            entry.uri = saved.uri?.let(Uri::parse)
            entry.finishedMs = saved.finishedMs
            items.add(entry)
            nextId = maxOf(nextId, saved.id + 1)
        }
    }

    private fun saveHistory() {
        val snapshot = items.filter { it.state != DownloadState.RUNNING }.map {
            SavedDownload(it.id, it.name, it.url, it.mime, it.state, it.total, it.done,
                it.uri?.toString(), it.error, it.finishedMs)
        }
        scope.launch(Dispatchers.IO) {
            try {
                val temp = File(historyFile.parentFile, "downloads.json.tmp")
                temp.writeText(savedToJson(snapshot))
                if (!temp.renameTo(historyFile)) {
                    historyFile.writeText(savedToJson(snapshot))
                    temp.delete()
                }
            } catch (e: IOException) {
                android.util.Log.w("CosmicDownloads", "Impossibile salvare l'elenco dei download")
            }
        }
    }

    /** System notification when finished; skipped silently if the user has not allowed notifications. */
    private fun notifyFinished(entry: DownloadEntry, message: String) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU &&
            context.checkSelfPermission(Manifest.permission.POST_NOTIFICATIONS) != PackageManager.PERMISSION_GRANTED
        ) return
        val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
        if (!manager.areNotificationsEnabled()) return
        manager.createNotificationChannel(
            NotificationChannel(CHANNEL_ID, "Download", NotificationManager.IMPORTANCE_DEFAULT)
        )
        val open = PendingIntent.getActivity(
            context, entry.id.toInt(), Intent(DownloadManager.ACTION_VIEW_DOWNLOADS),
            PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT
        )
        val notification = android.app.Notification.Builder(context, CHANNEL_ID)
            .setSmallIcon(R.drawable.ic_cosmic)
            .setContentTitle(message)
            .setContentText(if (entry.state == DownloadState.DONE) "Salvato in Download" else entry.error)
            .setContentIntent(open)
            .setAutoCancel(true)
            .build()
        manager.notify(entry.id.toInt(), notification)
    }

    private companion object {
        const val CHANNEL_ID = "downloads"
    }
}
