package app.cosmic.browser.mobile

import android.content.ContentValues
import android.content.Context
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
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import java.io.File
import java.io.IOException
import java.io.OutputStream
import java.net.HttpURLConnection
import java.net.URL
import java.util.concurrent.atomic.AtomicLong

internal enum class DownloadState { RUNNING, DONE, FAILED }

internal class DownloadEntry(val id: Long, val name: String) {
    var state by mutableStateOf(DownloadState.RUNNING)
    var done by mutableLongStateOf(0L)
    var total by mutableLongStateOf(-1L)
    var error by mutableStateOf("")
    var uri: Uri? = null
    var mime: String? = null
}

internal class DownloadCenter(private val context: Context) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val nextId = AtomicLong(1)
    val items = mutableStateListOf<DownloadEntry>()
    var banner by mutableStateOf<String?>(null)

    fun start(url: String, userAgent: String?, disposition: String?, mimeType: String?) {
        val parsed = Uri.parse(url)
        if (parsed.scheme != "http" && parsed.scheme != "https") {
            banner = "Download non supportato per questo tipo di indirizzo"
            return
        }
        val name = URLUtil.guessFileName(url, disposition, mimeType)
            .replace(Regex("[\\\\/:*?\"<>|\\u0000-\\u001F]"), "_").take(180).ifBlank { "download" }
        val entry = DownloadEntry(nextId.getAndIncrement(), name).also { it.mime = mimeType }
        items.add(0, entry)
        banner = "Download avviato: $name"
        val cookie = CookieManager.getInstance().getCookie(url)
        scope.launch {
            try {
                withContext(Dispatchers.IO) { run(entry, url, userAgent, cookie) }
                entry.state = DownloadState.DONE
                banner = "Download completato: $name"
            } catch (e: Exception) {
                entry.state = DownloadState.FAILED
                entry.error = e.message ?: "errore di rete"
                banner = "Download non riuscito: $name"
            }
        }
    }

    fun remove(entry: DownloadEntry) {
        items.remove(entry)
    }

    private fun run(entry: DownloadEntry, url: String, userAgent: String?, cookie: String?) {
        val connection = URL(url).openConnection() as HttpURLConnection
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
                val count = input.read(buffer)
                if (count < 0) break
                output.write(buffer, 0, count)
                entry.done += count
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
            file = File(directory, "${entry.name.substringBeforeLast('.')} ($n)" +
                (entry.name.substringAfterLast('.', "").let { if (it.isEmpty()) "" else ".$it" }))
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
}
