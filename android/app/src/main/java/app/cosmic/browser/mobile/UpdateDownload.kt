package app.cosmic.browser.mobile

import kotlinx.coroutines.ensureActive
import java.io.IOException
import java.io.InputStream
import java.io.OutputStream
import java.security.MessageDigest
import kotlin.coroutines.coroutineContext

internal suspend fun copyVerifiedUpdate(
    input: InputStream, output: OutputStream, expectedSize: Long, expectedHash: String
) {
    val digest = MessageDigest.getInstance("SHA-256")
    val buffer = ByteArray(64 * 1024)
    var total = 0L
    while (true) {
        coroutineContext.ensureActive()
        val count = input.read(buffer)
        if (count < 0) break
        total += count
        if (total > expectedSize || total > MAX_UPDATE_BYTES) {
            throw IOException("Il pacchetto supera la dimensione prevista.")
        }
        digest.update(buffer, 0, count)
        output.write(buffer, 0, count)
    }
    val hash = digest.digest().joinToString("") { "%02x".format(it.toInt() and 0xff) }
    if (total != expectedSize || hash != expectedHash) {
        throw SecurityException("Dimensione o impronta SHA-256 non corrispondente.")
    }
}
