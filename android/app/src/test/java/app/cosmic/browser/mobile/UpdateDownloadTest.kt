package app.cosmic.browser.mobile

import kotlinx.coroutines.runBlocking
import org.junit.Assert.assertArrayEquals
import org.junit.Test
import java.io.ByteArrayInputStream
import java.io.ByteArrayOutputStream
import java.io.IOException

class UpdateDownloadTest {
    private val data = "abc".toByteArray()
    private val sha = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"

    @Test
    fun acceptsExactSizeAndHash() = runBlocking {
        val output = ByteArrayOutputStream()
        copyVerifiedUpdate(ByteArrayInputStream(data), output, 3, sha)
        assertArrayEquals(data, output.toByteArray())
    }

    @Test(expected = SecurityException::class)
    fun rejectsWrongHash() = runBlocking {
        copyVerifiedUpdate(ByteArrayInputStream(data), ByteArrayOutputStream(), 3, "0".repeat(64))
    }

    @Test(expected = SecurityException::class)
    fun rejectsTruncatedDownload() = runBlocking {
        copyVerifiedUpdate(ByteArrayInputStream(data), ByteArrayOutputStream(), 4, sha)
    }

    @Test(expected = IOException::class)
    fun rejectsOversizedDownload() = runBlocking {
        copyVerifiedUpdate(ByteArrayInputStream(data), ByteArrayOutputStream(), 2, sha)
    }
}
