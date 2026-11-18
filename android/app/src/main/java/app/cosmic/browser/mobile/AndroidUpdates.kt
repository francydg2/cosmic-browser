package app.cosmic.browser.mobile

import android.content.Context
import android.content.pm.PackageInfo
import android.content.pm.PackageManager
import android.os.Build
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.setValue
import kotlinx.coroutines.CancellationException
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.SupervisorJob
import kotlinx.coroutines.cancel
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext
import org.json.JSONException
import java.io.File
import java.io.IOException
import java.net.URL
import javax.net.ssl.HttpsURLConnection

private const val RELEASES_URL =
    "https://api.github.com/repos/francydg2/cosmic-browser/releases?per_page=30"
private val DOWNLOAD_HOSTS = setOf("github.com", "release-assets.githubusercontent.com", "objects.githubusercontent.com")

internal class AndroidUpdates(private val context: Context) {
    private val scope = CoroutineScope(SupervisorJob() + Dispatchers.Main.immediate)
    private val pm = context.packageManager
    private val installed = packageInfo(context.packageName)
    private val currentVersion = installed.versionName ?: ""
    private val parsedVersion = UpdateVersion.parse(currentVersion)

    var status by mutableStateOf("Nessuna verifica effettuata.")
        private set
    var busy by mutableStateOf(false)
        private set
    var candidate by mutableStateOf<UpdateCandidate?>(null)
        private set
    var readyFile by mutableStateOf<File?>(null)
        private set
    var showOffer by mutableStateOf(false)
    var showReady by mutableStateOf(false)

    fun check(manual: Boolean) {
        if (busy) return
        busy = true
        candidate = null
        readyFile = null
        status = "Verifica degli aggiornamenti…"
        scope.launch {
            try {
                val found = withContext(Dispatchers.IO) { findUpdate() }
                candidate = found
                readyFile = null
                status = if (found == null) "Cosmic è aggiornato." else "Disponibile: ${found.versionName}"
                showOffer = found != null
            } catch (e: CancellationException) {
                throw e
            } catch (e: IOException) {
                status = "Verifica non riuscita: ${e.message}"
            } catch (e: SecurityException) {
                status = "Verifica bloccata: ${e.message}"
            } catch (e: JSONException) {
                status = "Risposta degli aggiornamenti non valida."
            } finally {
                busy = false
                if (!manual && candidate == null) showOffer = false
            }
        }
    }

    fun download() {
        val selected = candidate ?: return
        if (busy) return
        busy = true
        showOffer = false
        status = "Download e verifica dell'aggiornamento…"
        scope.launch {
            try {
                val file = withContext(Dispatchers.IO) { downloadAndVerify(selected) }
                readyFile = file
                status = "Aggiornamento ${selected.versionName} pronto da installare."
                showReady = true
            } catch (e: CancellationException) {
                throw e
            } catch (e: IOException) {
                status = "Aggiornamento non riuscito: ${e.message}"
            } catch (e: SecurityException) {
                status = "Aggiornamento rifiutato: ${e.message}"
            } finally {
                busy = false
            }
        }
    }

    fun reportInstallError(message: String) {
        status = message
    }

    fun close() = scope.cancel()

    private fun findUpdate(): UpdateCandidate? {
        val current = parsedVersion ?: throw IOException("Versione installata non riconosciuta.")
        return newestUpdate(readReleaseData(), current)
    }

    private fun readReleaseData(): String {
        val connection = connect(URL(RELEASES_URL), setOf("api.github.com"))
        try {
            val data = connection.inputStream.use { input ->
                val output = java.io.ByteArrayOutputStream()
                val buffer = ByteArray(8192)
                while (true) {
                    val count = input.read(buffer)
                    if (count < 0) break
                    if (output.size() + count > 2 * 1024 * 1024) throw IOException("Risposta troppo grande.")
                    output.write(buffer, 0, count)
                }
                output.toByteArray()
            }
            return data.toString(Charsets.UTF_8)
        } finally {
            connection.disconnect()
        }
    }

    private suspend fun downloadAndVerify(update: UpdateCandidate): File {
        val directory = File(context.cacheDir, "updates")
        if (!directory.exists() && !directory.mkdirs()) throw IOException("Impossibile creare la cartella temporanea.")
        val temp = File.createTempFile("update-", ".apk", directory)
        try {
            val connection = connect(URL(update.url), DOWNLOAD_HOSTS)
            try {
                connection.inputStream.use { input ->
                    temp.outputStream().use { output ->
                        copyVerifiedUpdate(input, output, update.size, update.sha256)
                    }
                }
            } finally {
                connection.disconnect()
            }
            verifyArchive(temp, update)
            val target = File(directory, "cosmic-update.apk")
            if (!temp.renameTo(target)) throw IOException("Impossibile salvare il pacchetto verificato.")
            return target
        } finally {
            if (temp.exists() && !temp.delete()) {
                android.util.Log.w("CosmicUpdates", "Impossibile rimuovere il download temporaneo")
            }
        }
    }

    @Suppress("DEPRECATION")
    private fun packageInfo(name: String): PackageInfo {
        val flags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
            PackageManager.GET_SIGNING_CERTIFICATES else PackageManager.GET_SIGNATURES
        return pm.getPackageInfo(name, flags)
    }

    @Suppress("DEPRECATION")
    private fun verifyArchive(file: File, update: UpdateCandidate) {
        val flags = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
            PackageManager.GET_SIGNING_CERTIFICATES else PackageManager.GET_SIGNATURES
        val archive = pm.getPackageArchiveInfo(file.absolutePath, flags)
            ?: throw SecurityException("APK non valido.")
        val oldCode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) installed.longVersionCode
            else installed.versionCode.toLong()
        val newCode = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) archive.longVersionCode
            else archive.versionCode.toLong()
        if (archive.packageName != context.packageName || archive.versionName != update.versionName ||
            newCode <= oldCode) throw SecurityException("Identità o versione APK non valida.")
        val originalSignatures = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
            installed.signingInfo?.apkContentsSigners else installed.signatures
        val newSignatures = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P)
            archive.signingInfo?.apkContentsSigners else archive.signatures
        if (originalSignatures.isNullOrEmpty() || newSignatures.isNullOrEmpty() ||
            originalSignatures.toSet() != newSignatures.toSet()) {
            throw SecurityException("La firma dell'APK non corrisponde a quella installata.")
        }
    }

    private fun connect(initial: URL, allowedHosts: Set<String>): HttpsURLConnection {
        var url = initial
        repeat(6) {
            if (url.protocol != "https" || url.host !in allowedHosts || url.userInfo != null ||
                (url.port != -1 && url.port != 443)) throw IOException("Indirizzo di download non autorizzato.")
            val connection = url.openConnection() as HttpsURLConnection
            connection.instanceFollowRedirects = false
            connection.connectTimeout = 15000
            connection.readTimeout = 30000
            connection.setRequestProperty("Accept", "application/vnd.github+json")
            connection.setRequestProperty("User-Agent", "Cosmic-Android")
            val code = connection.responseCode
            if (code in 300..399) {
                val location = connection.getHeaderField("Location")
                connection.disconnect()
                if (location.isNullOrEmpty()) throw IOException("Reindirizzamento non valido.")
                url = URL(url, location)
            } else {
                if (code !in 200..299) {
                    connection.disconnect()
                    throw IOException("Server HTTP $code.")
                }
                return connection
            }
        }
        throw IOException("Troppi reindirizzamenti.")
    }
}
